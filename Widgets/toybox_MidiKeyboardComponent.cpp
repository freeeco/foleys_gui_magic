/*
  ==============================================================================

   This file is part of the JUCE framework.
   Copyright (c) Raw Material Software Limited

   JUCE is an open source framework subject to commercial or open source
   licensing.

   By downloading, installing, or using the JUCE framework, or combining the
   JUCE framework with any other source code, object code, content or any other
   copyrightable work, you agree to the terms of the JUCE End User Licence
   Agreement, and all incorporated terms including the JUCE Privacy Policy and
   the JUCE Website Terms of Service, as applicable, which will bind you. If you
   do not agree to the terms of these agreements, we will not license the JUCE
   framework to you, and you must discontinue the installation or download
   process and cease use of the JUCE framework.

   JUCE End User Licence Agreement: https://juce.com/legal/juce-8-licence/
   JUCE Privacy Policy: https://juce.com/juce-privacy-policy
   JUCE Website Terms of Service: https://juce.com/juce-website-terms-of-service/

   Or:

   You may also use this code under the terms of the AGPLv3:
   https://www.gnu.org/licenses/agpl-3.0.en.html

   THE JUCE FRAMEWORK IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL
   WARRANTIES, WHETHER EXPRESSED OR IMPLIED, INCLUDING WARRANTY OF
   MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, ARE DISCLAIMED.

  ==============================================================================
*/

namespace juce
{

namespace
{
    //==============================================================================
    // Trigger node property tables (used only by the inline TriggerEditor).

    // (low, high) note-range property pairs that define a trigger node's key
    // footprint. Used to resolve a clicked key to the node(s) sitting on it.
    struct TriggerRangePair { const char* lo; const char* hi; };

    const std::vector<TriggerRangePair> kTriggerRangePairs
    {
        { "select-low-note-value",    "select-high-note-value"    },
        { "retrigger-low-note-value", "retrigger-high-note-value" },
        { "stop-low-note-value",      "stop-high-note-value"      },
        { "gate-low-note-value",      "gate-high-note-value"      },
        { "transpose-low-note-value", "transpose-high-note-value" },
        { "trigger-low-note-value",   "trigger-high-note-value"   },
    };

    // Node types whose input-low-note / input-high-note filter follows grouped
    // (matching node id) trigger range edits — edge resize and paste/duplicate
    // transposition. Followers track the trigger-derived delta but saturate
    // individually rather than constraining it.
    const StringArray kRangeFollowerTypes { "Mapper", "Arpeggiator", "LFO", "Envelope" };

    // Every note-valued property — the range pairs above plus the transpose
    // root, which is a position inside the range and so shifts with it on
    // paste. remapNoteRanges adds the (single, pre-clamped) delta to each of
    // these that's present on a node.
    const StringArray kTriggerNoteValuedProps
    {
        "select-low-note-value",    "select-high-note-value",
        "retrigger-low-note-value", "retrigger-high-note-value",
        "stop-low-note-value",      "stop-high-note-value",
        "gate-low-note-value",      "gate-high-note-value",
        "transpose-low-note-value", "transpose-high-note-value",
        "transpose-root-note-value",
        "trigger-low-note-value",   "trigger-high-note-value",
    };
    
    // Macro pool sizes — upper bounds only. The allocator self-limits to params that
    // actually exist in the patch (Step 5 in insertPayloadAtKey checks getParameter),
    // so these just need to be >= the macro_select / macro_knob / macro_button counts
    // exposed by the RNBO patch.
    static constexpr int kMaxSelectMacros = 32;
    static constexpr int kMaxKnobMacros   = 32;
    static constexpr int kMaxButtonMacros = 32;

    //==============================================================================
    // Group-id helpers — see insertPayloadAtKey / doClear.

    /** Strips a trailing " (n)" suffix, returning the stem. Leaves the id
        untouched if no suffix is present, or if the parens don't enclose digits.
        "Note Repeat (2)" -> "Note Repeat"; "Note Repeat" -> "Note Repeat". */
    inline String stripGroupSuffix (const String& id)
    {
        if (! id.endsWithChar (')'))
            return id;

        const int openParen = id.lastIndexOfChar ('(');
        if (openParen < 1 || id[openParen - 1] != ' ')
            return id;

        const auto inside = id.substring (openParen + 1, id.length() - 1);
        if (inside.isEmpty() || ! inside.containsOnly ("0123456789"))
            return id;

        return id.substring (0, openParen - 1);
    }

    /** True for ids ending in "[global]". Marks a node as a shared singleton:
        on paste, it's skipped if its id is already present; on clear, the
        companion is removed only when no other instance of its stem remains. */
    inline bool isGlobalID (const String& id)
    {
        return id.trim().endsWithIgnoreCase ("[global]");
    }

    //==============================================================================
    // Uniquify pass for insertPayloadAtKey.
    //
    // PGM's uniquification scheme stamps both UID-style identifiers and ":"-style
    // value references with a millisecond-since-2000 suffix so pastes don't share
    // bindings with their source. The detection rules:
    //
    //   * Value refs    — property *value* contains ":", no whitespace, doesn't
    //                     start with "http", and ends with -<8+ digits>. Standalone
    //                     only — composite strings (anything with ",;/") are
    //                     treated as opaque external references and left alone.
    //   * UID refs      — property *name* contains "-uid" and value matches
    //                     <text>_<8+ digits> as a whole identifier (no ",;:/" or
    //                     whitespace). Same standalone-only rule: a Playlist score
    //                     string carrying clip UIDs with timing reaches outside the
    //                     bundle and stays verbatim.
    //
    // Two-pass: pass 1 discovers and mints new stamps, pass 2 substitutes by
    // exact-value lookup so cross-refs within the bundle land on the same new
    // stamp. A single shared counter across both maps keeps stamps distinct.
    //
    // Non-uniquified refs ("gui:select_expression", "Playhead_main") have no
    // matching suffix and so naturally fall through — they're stable identifiers
    // by convention.

    inline bool isStandaloneUniquifiedValue (const String& value)
    {
        if (! value.contains (":"))                          return false;
        if (value != value.removeCharacters (" \t\n\r"))      return false;
        if (value.startsWithIgnoreCase ("http"))             return false;
        if (value.containsAnyOf (",;/"))                     return false;

        const int lastDash = value.lastIndexOfChar ('-');
        if (lastDash < 0)                                    return false;

        const auto suffix = value.substring (lastDash + 1);
        return suffix.length() >= 8 && suffix.containsOnly ("0123456789");
    }

    inline bool isStandaloneUniquifiedUID (const String& value)
    {
        if (value.isEmpty())                                 return false;
        if (value.containsAnyOf (",;:/ \t\n\r"))             return false;

        const int lastUnderscore = value.lastIndexOfChar ('_');
        if (lastUnderscore <= 0)                             return false;

        const auto suffix = value.substring (lastUnderscore + 1);
        return suffix.length() >= 8 && suffix.containsOnly ("0123456789");
    }

    inline void uniquifyPayloadRefs (Array<ValueTree>& payload, Array<ValueTree>& extras)
    {
        const auto baseStamp = (int64) (Time::getCurrentTime()
                                        - Time (2000, 0, 1, 0, 0, 0)).inMilliseconds();
        int counter = 0;
        std::map<String, String> valueMap, uidMap;

        // Pass 1 — discover & mint.
        std::function<void (ValueTree&)> discover = [&] (ValueTree& node)
        {
            for (int i = 0; i < node.getNumProperties(); ++i)
            {
                const auto name  = node.getPropertyName (i);
                const auto value = node.getProperty (name).toString();

                const bool isUidProp = name.toString().containsIgnoreCase ("-uid");

                if (isUidProp)
                {
                    if (isStandaloneUniquifiedUID (value) && uidMap.find (value) == uidMap.end())
                    {
                        const auto stem = value.upToLastOccurrenceOf ("_", false, false);
                        uidMap[value] = stem + "_" + String (baseStamp + counter++);
                    }
                }
                else if (isStandaloneUniquifiedValue (value) && valueMap.find (value) == valueMap.end())
                {
                    const auto stem = value.upToLastOccurrenceOf ("-", false, false);
                    valueMap[value] = stem + "-" + String (baseStamp + counter++);
                }
            }

            for (auto child : node)
                discover (child);
        };

        // Pass 2 — substitute. Exact-value lookup, so cross-refs (multiple
        // properties carrying the same original string) all land on the same new
        // string. Composites that contain a uniquified UID as a substring don't
        // match here either — that's the deliberate "leave score strings alone"
        // behaviour.
        std::function<void (ValueTree&)> substitute = [&] (ValueTree& node)
        {
            for (int i = 0; i < node.getNumProperties(); ++i)
            {
                const auto name  = node.getPropertyName (i);
                const auto value = node.getProperty (name).toString();

                if (auto it = valueMap.find (value); it != valueMap.end())
                {
                    node.setProperty (name, it->second, nullptr);
                    continue;
                }
                if (auto it = uidMap.find (value); it != uidMap.end())
                    node.setProperty (name, it->second, nullptr);
            }

            for (auto child : node)
                substitute (child);
        };

        for (auto& n : payload) discover (n);
        for (auto& n : extras)  discover (n);

        if (valueMap.empty() && uidMap.empty())
            return;

        for (auto& n : payload) substitute (n);
        for (auto& n : extras)  substitute (n);
    }

    // 20 distinguishable colours offered in the key menu's Colour submenu. The
    // names describe how each reads once applied at its `brightness` factor (see
    // the menu handler), not the bright base hue. Most are halved; a couple that
    // still read too bright are taken down further.
    struct TriggerNamedColour { String name; Colour colour; float brightness = 0.5f; };

// Rainbow order, looks nice but no good for adjacent keys?

//    const std::vector<TriggerNamedColour> kTriggerColours
//    {
//        { "Red",     Colour (0xffe53935) }, { "Bronze",  Colour (0xfffb8c00) },
//        { "Amber",   Colour (0xffffb300) }, { "Mustard", Colour (0xfffdd835) },
//        { "Olive",   Colour (0xffc0ca33) }, { "Green",   Colour (0xff43a047) },
//        { "Teal",    Colour (0xff00897b) }, { "Cyan",    Colour (0xff00acc1) },
//        { "Blue",    Colour (0xff039be5) }, { "Navy",    Colour (0xff1e88e5), 0.25f },
//        { "Indigo",  Colour (0xff3949ab) }, { "Violet",  Colour (0xff8e24aa) },
//        { "Wine",    Colour (0xffd81b60), 0.25f }, { "Rose",    Colour (0xffec407a) },
//        { "Plum",    Colour (0xfff06292) }, { "Brown",   Colour (0xff6d4c41) },
//        { "Slate",   Colour (0xff546e7a) }, { "Grey",    Colour (0xff9e9e9e) },
//        { "Forest",  Colour (0xff66bb6a) }, { "Purple",  Colour (0xff9575cd) },
//    };

// Better (stripes) order?

    const std::vector<TriggerNamedColour> kTriggerColours
    {
        { "Red",     Colour (0xffe53935) },                { "Indigo",  Colour (0xff3949ab) },
        { "Bronze",  Colour (0xfffb8c00) },                { "Violet",  Colour (0xff8e24aa) },
        { "Amber",   Colour (0xffffb300) },                { "Wine",    Colour (0xffd81b60), 0.25f },
        { "Mustard", Colour (0xfffdd835) },                { "Rose",    Colour (0xffec407a) },
        { "Olive",   Colour (0xffc0ca33) },                { "Plum",    Colour (0xfff06292) },
        { "Green",   Colour (0xff43a047) },                { "Brown",   Colour (0xff6d4c41) },
        { "Teal",    Colour (0xff00897b) },                { "Slate",   Colour (0xff546e7a) },
        { "Cyan",    Colour (0xff00acc1) },                { "Grey",    Colour (0xff9e9e9e) },
        { "Blue",    Colour (0xff039be5) },                { "Forest",  Colour (0xff66bb6a) },
        { "Navy",    Colour (0xff1e88e5), 0.25f },         { "Purple",  Colour (0xff9575cd) },
    };

    // Auto-colour tuning. autoColourSnippet writes a trigger colour T pulled from
    // kTriggerColours (with its brightness factor applied, exactly like the menu),
    // then derives the GUI panel's Background gradient from T:
    //   panel  P = T * kAutoColourPanelBrightness         (lighter than the trigger)
    //   shadow S = P * kAutoColourPanelShadowBrightness   (bottom stop of the gradient)
    // Multi-trigger snippets ramp the upper triggers brighter than the anchor:
    //   trigger_i = T * (1 + (i / (N-1)) * kAutoColourMultiTriggerSpread)
    // Tweak freely.
    constexpr float kAutoColourTriggerSaturation     = 0.65f;
    constexpr float kAutoColourTriggerBrightness     = 1.1f;
    constexpr float kAutoColourMultiTriggerSpread    = 0.15f;
    constexpr float kAutoColourMultiTriggerHueRotate = 0.05f;
    constexpr float kAutoColourPanelBrightness       = 1.6f;
    constexpr float kAutoColourPanelSaturation       = 0.65f;
    constexpr float kAutoColourPanelShadowBrightness = 0.96f;

    // Shared writer for the auto-colour palette of one logical group. The anchor
    // (sort index 0) gets T (= baseTriggerColour with the trigger-side brightness
    // and saturation transforms); subsequent triggers ramp through hue-rotated,
    // brightness-spread variants of T. Each Background gets the same 3-stop
    // fill-gradient (shadow at 0%, panel from 64%-100%) driven by P and S.
    // Called from autoColourSnippet (off-tree, nullptr um) and from
    // TriggerEditor::setColourForKey (live, with the keyboard's UM).
    // triggersSortedByLowestNote must already be in the order the spread should
    // ramp; backgrounds may be empty.
    inline void writeAutoColourPalette (const Array<ValueTree>& triggersSortedByLowestNote,
                                        const Array<ValueTree>& backgrounds,
                                        Colour                  baseTriggerColour,
                                        UndoManager*            um)
    {
        static const Identifier triggerColourProp ("trigger-colour");
        static const Identifier fillGradientProp  ("fill-gradient");

        const Colour T = baseTriggerColour.withMultipliedBrightness (kAutoColourTriggerBrightness)
                                          .withMultipliedSaturation (kAutoColourTriggerSaturation);

        const int N = triggersSortedByLowestNote.size();
        for (int i = 0; i < N; ++i)
        {
            Colour c = T;
            if (i > 0)
            {
                const float hueDelta   = (float) i * kAutoColourMultiTriggerHueRotate;
                const float brightness = (i % 2 == 1) ? (1.0f - kAutoColourMultiTriggerSpread)
                                                      : (1.0f + kAutoColourMultiTriggerSpread);
                c = T.withRotatedHue (hueDelta).withMultipliedBrightness (brightness);
            }
            ValueTree node = triggersSortedByLowestNote.getReference (i);
            node.setProperty (triggerColourProp, c.toString(), um);
        }

        if (backgrounds.isEmpty())
            return;

        const Colour P = T.withMultipliedBrightness (kAutoColourPanelBrightness)
                          .withMultipliedSaturation (kAutoColourPanelSaturation);
        const Colour S = P.withMultipliedBrightness (kAutoColourPanelShadowBrightness);

        const String gradient = "linear-gradient(0,0% " + S.toString()
                              + ",64% " + P.toString()
                              + ",100% " + P.toString() + ")";

        for (int i = 0; i < backgrounds.size(); ++i)
        {
            ValueTree bg = backgrounds.getReference (i);
            bg.setProperty (fillGradientProp, gradient, um);
        }
    }

    // Menu result-id ranges.
    enum
    {
        miCopy = 1, miCut, miPaste, miClear, miClearAll, miSaveAs,
        miColourBase  = 100,    // + index into kTriggerColours
        miPresetBase  = 1000    // + index into the captured preset-files vector
    };

    //==============================================================================
    // Stain tuning — how strongly a configured-but-disabled trigger shows
    // through on its keys. Bump these to make the stain more visible.

    // White keys: amount of trigger hue blended into the white base.
    //   0.0 = pure white (invisible stain), 1.0 = pure trigger colour.
    constexpr float kWhiteStainTint = 0.28f;

    // Black keys: how much lighter than a normal black key the stained base
    // sits, before the hue is mixed in. Higher = paler key body.
    constexpr float kBlackStainLighten = 0.18f;

    // Black keys: amount of trigger hue blended into the lightened base.
    //   0.0 = no colour (just lightened), 1.0 = pure trigger colour.
    constexpr float kBlackStainTint = 0.32f;

    // Mouse-over is a hint ("the mouse is here"), not a functional state — keep
    // it as a faint tint, well below the keyDown overlay so it doesn't read as
    // a press. Bump these if the hover becomes invisible on busy backgrounds.
    constexpr float kHoverCustomDarken = 0.14f;   // darken amount on coloured keys (vs 0.15)
    constexpr float kHoverOverlayAlpha = 0.35f;   // multiplier on mouseOverKeyOverlayColourId

    // Edit-mode region drag handle — a band along the top of each region's keys
    // that translates the whole range (low + high notes shift together by one
    // delta).
    constexpr float kRegionHandleHeight     = 30.0f;
}

//==============================================================================
NewMidiKeyboardComponent::NewMidiKeyboardComponent (MidiKeyboardState& stateToUse, Orientation orientationToUse)
    : KeyboardComponentBase (orientationToUse), state (stateToUse)
{
    state.addListener (this);
    setLowestVisibleKey (initialLowestKeyShowing);

    const bool standalone = juce::PluginHostType::getPluginLoadedAs()
                          == juce::AudioProcessor::wrapperType_Standalone;

    if (standalone)
    {
        const std::string_view keys { "awsedftgyhujkolp;" };
        for (const char& c : keys)
            setKeyPressForNote ({c, 0, 0}, (int) std::distance (keys.data(), &c));
    }

    mouseOverNotes.insertMultiple (0, -1, 32);
    mouseDownNotes.insertMultiple (0, -1, 32);

    // The standard MidiKeyboardComponent colour IDs (0x1005000–0x1005006) get
    // their defaults from LookAndFeel_V2. editOutlineColourId is a custom ID the
    // L&F doesn't know, so seed a default here — otherwise findColour trips a
    // jassertfalse when the stylesheet hasn't set "edit-outline-color". The
    // stylesheet still overrides this when the colour is specified.
    setColour (editOutlineColourId, Colours::white.withAlpha (0.65f));

    colourChanged();

    setWantsKeyboardFocus           (standalone);
    setMouseClickGrabsKeyboardFocus (standalone);

    startTimerHz (20);
}

NewMidiKeyboardComponent::~NewMidiKeyboardComponent()
{
    state.removeListener (this);
}

//==============================================================================
void NewMidiKeyboardComponent::setVelocity (float v, bool useMousePosition)
{
    velocity = v;
    useMousePositionForVelocity = useMousePosition;
}

//==============================================================================
void NewMidiKeyboardComponent::setMidiChannel (int midiChannelNumber)
{
    jassert (midiChannelNumber > 0 && midiChannelNumber <= 16);

    if (midiChannel != midiChannelNumber)
    {
        resetAnyKeysInUse();
        midiChannel = jlimit (1, 16, midiChannelNumber);
    }
}

void NewMidiKeyboardComponent::setMidiChannelsToDisplay (int midiChannelMask)
{
    midiInChannelMask = midiChannelMask;
    noPendingUpdates.store (false);
}

//==============================================================================
void NewMidiKeyboardComponent::setNoteColourProvider (std::function<std::optional<Colour>(int)> fn)
{
    noteColourProvider = fn;
    repaint();
}

void NewMidiKeyboardComponent::setNoteLabelProvider (std::function<std::optional<String>(int)> fn)
{
    noteLabelProvider = fn;
    repaint();
}

void NewMidiKeyboardComponent::setNoteTooltipProvider (std::function<std::optional<String>(int)> fn)
{
    noteTooltipProvider = std::move (fn);
}

//==============================================================================
void NewMidiKeyboardComponent::clearKeyMappings()
{
    resetAnyKeysInUse();
    keyPressNotes.clear();
    keyPresses.clear();
}

void NewMidiKeyboardComponent::setKeyPressForNote (const KeyPress& key, int midiNoteOffsetFromC)
{
    removeKeyPressForNote (midiNoteOffsetFromC);

    keyPressNotes.add (midiNoteOffsetFromC);
    keyPresses.add (key);
}

void NewMidiKeyboardComponent::removeKeyPressForNote (int midiNoteOffsetFromC)
{
    for (int i = keyPressNotes.size(); --i >= 0;)
    {
        if (keyPressNotes.getUnchecked (i) == midiNoteOffsetFromC)
        {
            keyPressNotes.remove (i);
            keyPresses.remove (i);
        }
    }
}

void NewMidiKeyboardComponent::setKeyPressBaseOctave (int newOctaveNumber)
{
    jassert (newOctaveNumber >= 0 && newOctaveNumber <= 10);

    keyMappingOctave = newOctaveNumber;
}

//==============================================================================
void NewMidiKeyboardComponent::resetAnyKeysInUse()
{
    if (! keysPressed.isZero())
    {
        for (int i = 128; --i >= 0;)
            if (keysPressed[i])
                state.noteOff (midiChannel, i, 0.0f);

        keysPressed.clear();
    }

    for (int i = mouseDownNotes.size(); --i >= 0;)
    {
        auto noteDown = mouseDownNotes.getUnchecked (i);

        if (noteDown >= 0)
        {
            state.noteOff (midiChannel, noteDown, 0.0f);
            mouseDownNotes.set (i, -1);
        }

        mouseOverNotes.set (i, -1);
    }
}

void NewMidiKeyboardComponent::updateNoteUnderMouse (const MouseEvent& e, bool isDown)
{
    updateNoteUnderMouse (e.getEventRelativeTo (this).position, isDown, e.source.getIndex());
}

void NewMidiKeyboardComponent::updateNoteUnderMouse (Point<float> pos, bool isDown, int fingerNum)
{
    const auto noteInfo = getNoteAndVelocityAtPosition (pos);
    const auto newNote = noteInfo.note;
    const auto oldNote = mouseOverNotes.getUnchecked (fingerNum);
    const auto oldNoteDown = mouseDownNotes.getUnchecked (fingerNum);
    const auto eventVelocity = useMousePositionForVelocity ? noteInfo.velocity * velocity : velocity;

    if (oldNote != newNote)
    {
        repaintNote (oldNote);
        repaintNote (newNote);
        mouseOverNotes.set (fingerNum, newNote);
    }

    if (isDown)
    {
        if (newNote != oldNoteDown)
        {
            if (oldNoteDown >= 0)
            {
                mouseDownNotes.set (fingerNum, -1);

                if (! mouseDownNotes.contains (oldNoteDown))
                    state.noteOff (midiChannel, oldNoteDown, eventVelocity);
            }

            if (newNote >= 0 && ! mouseDownNotes.contains (newNote))
            {
                state.noteOn (midiChannel, newNote, eventVelocity);
                mouseDownNotes.set (fingerNum, newNote);
            }
        }
    }
    else if (oldNoteDown >= 0)
    {
        mouseDownNotes.set (fingerNum, -1);

        if (! mouseDownNotes.contains (oldNoteDown))
            state.noteOff (midiChannel, oldNoteDown, eventVelocity);
    }
}

void NewMidiKeyboardComponent::paint (Graphics& g)
{
    // Macro panel open: the keyboard is hidden so the component behind shows
    // through. Paint nothing but a close button — an x-disc identical to the
    // edit-mode toggle — at the macro button's position. Clicking it toggles
    // the bound value back to 0 (handled in mouseDown, same hit target as
    // getMacroCloseButtonBounds()).
    if (macroPanelOpen)
    {
        const auto b = getMacroCloseButtonBounds();
        g.setColour (Colours::lightgrey);
        g.fillEllipse (b);
        g.setColour (Colours::grey);
        g.drawEllipse (b, 1.0f);

        const auto x   = b.reduced (b.getWidth() * 0.32f);
        const float th = jmax (1.5f, b.getWidth() * 0.10f);
        g.setColour (Colours::darkgrey);
        g.drawLine (x.getX(), x.getY(),      x.getRight(), x.getBottom(), th);
        g.drawLine (x.getX(), x.getBottom(), x.getRight(), x.getY(),      th);
        return;
    }

    g.fillAll (juce::Colours::darkgrey);

    KeyboardComponentBase::paint (g);

    drawEditOutlines (g);

    // Edit-mode toggle button — fixed at the top-left, drawn last so it sits
    // on top and doesn't scroll with the keys. Light-grey disc with a
    // dark-grey cross (in edit mode, to exit) or plus (out of edit mode, to
    // enter). Only shown when the editor has a container to operate on.
    if (triggerEditor.hasContext())
    {
        const auto b = getCloseButtonBounds();
        g.setColour (Colours::lightgrey);
        g.fillEllipse (b);
        g.setColour (Colours::grey);
        g.drawEllipse (b, 1.0f);

        const auto x   = b.reduced (b.getWidth() * 0.32f);
        const float th = jmax (1.5f, b.getWidth() * 0.10f);
        g.setColour (Colours::darkgrey);

        if (editMode)
        {
            g.drawLine (x.getX(), x.getY(),      x.getRight(), x.getBottom(), th);
            g.drawLine (x.getX(), x.getBottom(), x.getRight(), x.getY(),      th);
        }
        else
        {
            const float cx = x.getCentreX(), cy = x.getCentreY();
            g.drawLine (x.getX(),  cy,       x.getRight(), cy,           th);
            g.drawLine (cx,        x.getY(), cx,           x.getBottom(), th);
        }
    }

    // Macro-panel toggle button — the three dots ("..."), in the bottom slot
    // below the edit toggle. Shown whenever a macro value is bound, but hidden
    // while edit mode is active to keep the corner uncluttered. (Only drawn
    // while closed; the open state is the full-keyboard overlay above.)
    if (macroButtonVisible && ! editMode)
    {
        const auto b = getMacroButtonBounds();
        g.setColour (Colours::lightgrey);
        g.fillEllipse (b);
        g.setColour (Colours::grey);
        g.drawEllipse (b, 1.0f);

        const float dotR = jmax (1.0f, b.getWidth() * 0.07f);
        const float gap  = b.getWidth() * 0.24f;
        const float cy   = b.getCentreY();
        const float cx   = b.getCentreX();
        g.setColour (Colours::darkgrey);

        for (int i = -1; i <= 1; ++i)
        {
            const float dcx = cx + (float) i * gap;
            g.fillEllipse (dcx - dotR, cy - dotR, dotR * 2.0f, dotR * 2.0f);
        }
    }
}

bool NewMidiKeyboardComponent::hitTest (int x, int y)
{
    // While the macro panel is open the keyboard is a transparent overlay: only
    // the close button is "solid", so every other click passes through to the
    // component behind. Closed, the whole component is interactive as usual.
    if (macroPanelOpen)
        return getMacroCloseButtonBounds().expanded (4.0f).contains ((float) x, (float) y);

    return true;
}

void NewMidiKeyboardComponent::repaintNote (int noteNum)
{
    if (getRangeStart() <= noteNum && noteNum <= getRangeEnd())
        repaint (getRectangleForKey (noteNum).getSmallestIntegerContainer());
}


void NewMidiKeyboardComponent::mouseMove (const MouseEvent& e)
{
    updateNoteUnderMouse (e, false);

    if (editMode && ! resizingRegion)
    {
        if (hitTestRegionHandle (e.position).valid)
            setMouseCursor (MouseCursor::DraggingHandCursor);
        else
            setMouseCursor (hitTestRegionEdge (e.position).valid ? MouseCursor::LeftRightResizeCursor
                                                                 : MouseCursor::NormalCursor);
    }
}

void NewMidiKeyboardComponent::mouseDrag (const MouseEvent& e)
{
    
#if MAX_MIDI_TRIGGERS > 0
    // Belt-and-braces with mouseDown's popup-button short-circuit: a
    // right-button drag shouldn't slip a note-on through before
    // showMenuAsync's overlay grabs focus.
    if (e.mods.isPopupMenu())
        return;
#endif

    if (resizingRegion)
    {
        // Sample the note from the x position at a fixed height inside the
        // black-key band so the drag has full chromatic resolution wherever the
        // pointer is vertically. Deltas are measured from the edge's original
        // note, so the edit is absolute (no drift) and clamps cleanly.
        const float x       = jlimit (0.0f, (float) getWidth() - 1.0f, e.position.x);
        const float sampleY = (float) getHeight() * 0.25f;
        const int   target  = getNoteAndVelocityAtPosition ({ x, sampleY }).note;
        if (target >= 0)
            triggerEditor.updateEdgeResize (target - resizeAnchorNote);
        return;
    }

    auto newNote = getNoteAndVelocityAtPosition (e.position).note;

    if (newNote >= 0 && mouseDraggedToKey (newNote, e))
        updateNoteUnderMouse (e, true);
}

void NewMidiKeyboardComponent::mouseDown (const MouseEvent& e)
{
    // Macro-panel toggle button — independent of edit mode / triggers, fixed at
    // the top-left below the edit toggle. Left-click flips the bound macro value
    // via the callback; takes priority over the keys (and any edit-region edge)
    // beneath it. The keyboard never flips its own macroPanelOpen flag. Matches
    // the button's visibility: clickable when the panel is open (the close x) or
    // when the dots are showing (macro bound and not in edit mode).
    if (! e.mods.isPopupMenu()
        && (macroPanelOpen || (macroButtonVisible && ! editMode))
        && (macroPanelOpen ? getMacroCloseButtonBounds() : getMacroButtonBounds()).expanded (4.0f).contains (e.position))
    {
        if (onToggleMacroPanel != nullptr)
            onToggleMacroPanel();
        return;
    }

#if MAX_MIDI_TRIGGERS > 0
    if (e.mods.isPopupMenu())
    {
        const int note = getNoteAndVelocityAtPosition (e.position).note;
        if (note >= 0)
            triggerEditor.showMenuForKey (note, e.getScreenPosition());
        return;
    }
    
    // Toggle button takes priority over the keys beneath it whenever it's
    // visible (i.e. the editor has a container). Click swaps editMode by way
    // of the host-owned value — the keyboard doesn't flip it directly.
    if (triggerEditor.hasContext() && ! macroPanelOpen && getCloseButtonBounds().expanded (4.0f).contains (e.position))
    {
        if (editMode) { if (onExitEditMode  != nullptr) onExitEditMode();  }
        else          { if (onEnterEditMode != nullptr) onEnterEditMode(); }
        return;
    }

    // Grabbing the top-band handle translates the whole region (low + high
    // edges shift together). Checked before the edge hit so the upper corners
    // are reliably grabbable as "translate" rather than "resize".
    if (editMode)
    {
        const auto handle = hitTestRegionHandle (e.position);
        if (handle.valid)
        {
            const int anchor = getNoteAndVelocityAtPosition (e.position).note;
            if (anchor >= 0 && triggerEditor.beginRegionDrag (anchor))
            {
                resizingRegion     = true;
                resizeAnchorNote   = anchor;
                setMouseCursor (MouseCursor::DraggingHandCursor);
                return;
            }
        }
    }

    // Grabbing a region edge starts a width resize instead of opening the menu.
    if (editMode)
    {
        const auto edge = hitTestRegionEdge (e.position);
        if (edge.valid)
        {
            const int anchor = edge.lowEdge ? edge.regionStart : edge.regionEnd;
            if (triggerEditor.beginEdgeResize (anchor, edge.lowEdge))
            {
                resizingRegion   = true;
                resizeLowEdge    = edge.lowEdge;
                resizeAnchorNote = anchor;
                return;
            }
        }
    }
#endif

    auto newNote = getNoteAndVelocityAtPosition (e.position).note;

    if (newNote >= 0 && mouseDownOnKey (newNote, e))
        updateNoteUnderMouse (e, true);
}

Rectangle<float> NewMidiKeyboardComponent::getCloseButtonBounds() const
{
    // Edit toggle: top-left, sitting just inboard of the left octave-scroll
    // button. That scroll button is wider on iOS (its end keys double as touch
    // octave shifters), so derive the x inset from the actual scroll-button
    // width + a fixed gap rather than a magic constant — that way it clears the
    // octave button by the same gap on every platform. iOS also uses a fatter
    // disc. The macro button on the right mirrors this, so both sides clear.
#if JUCE_IOS
    const float margin = 12.0f;
    const float d      = jlimit (16.0f, 34.0f, (float) getHeight() * 0.32f);
#else
    const float margin = 6.0f;
    const float d      = jlimit (16.0f, 28.0f, (float) getHeight() * 0.16f);
#endif
    const float gap = 8.0f;                                  // clearance from the octave-scroll button
    const float x   = (float) getScrollButtonWidth() + gap;
    return { x, margin + 2.0f, d, d };
}

Rectangle<float> NewMidiKeyboardComponent::getMacroButtonBounds() const
{
    // Macro toggle: top-right — the edit button mirrored across the keyboard, so
    // it sits the same distance from the right edge as the edit button is from
    // the left. Same y and size.
    const auto edit = getCloseButtonBounds();
    return edit.withX ((float) getWidth() - edit.getRight());
}

Rectangle<float> NewMidiKeyboardComponent::getMacroCloseButtonBounds() const
{
    // The panel-open close (x), only. Starts from the macro button on the right;
    // change macroCloseXOffset to nudge it independently of the "..." button.
    constexpr float macroCloseXOffset = 0.0f;
    return getMacroButtonBounds().translated (macroCloseXOffset, 0.0f);
}

NewMidiKeyboardComponent::RegionEdge NewMidiKeyboardComponent::hitTestRegionEdge (Point<float> pos)
{
    RegionEdge hit;
    if (! editMode || ! noteColourProvider)
        return hit;

    const int note = getNoteAndVelocityAtPosition (pos).note;
    if (note < 0)
        return hit;

    const auto c = noteColourProvider (note);
    if (! c.has_value())
        return hit;

    // Grow to the contiguous same-colour run (the region).
    int start = note, end = note;
    while (start - 1 >= getRangeStart() && noteColourProvider (start - 1) == c) --start;
    while (end   + 1 <= getRangeEnd()   && noteColourProvider (end   + 1) == c) ++end;

    const float leftX  = getRectangleForKey (start).getX();
    const float rightX = getRectangleForKey (end).getRight();
    const float thresh = 7.0f;
    const float dl     = std::abs (pos.x - leftX);
    const float dr     = std::abs (pos.x - rightX);

    if (dl <= thresh || dr <= thresh)
    {
        hit.valid       = true;
        hit.lowEdge     = (dl <= dr);   // closer edge wins for a narrow region
        hit.regionStart = start;
        hit.regionEnd   = end;
    }
    return hit;
}

NewMidiKeyboardComponent::RegionHandle NewMidiKeyboardComponent::hitTestRegionHandle (Point<float> pos)
{
    RegionHandle hit;
    if (! editMode || ! noteColourProvider)
        return hit;

    // Horizontal keyboard only — the band sits along the top of the keys.
    if (getOrientation() != horizontalKeyboard)
        return hit;

    if (pos.y > kRegionHandleHeight)
        return hit;

    // Close button overlaps the band at the top-left and takes priority.
    if (triggerEditor.hasContext() && getCloseButtonBounds().expanded (4.0f).contains (pos))
        return hit;

    const int note = getNoteAndVelocityAtPosition (pos).note;
    if (note < 0)
        return hit;

    const auto c = noteColourProvider (note);
    if (! c.has_value())
        return hit;

    int start = note, end = note;
    while (start - 1 >= getRangeStart() && noteColourProvider (start - 1) == c) --start;
    while (end   + 1 <= getRangeEnd()   && noteColourProvider (end   + 1) == c) ++end;

    hit.valid       = true;
    hit.regionStart = start;
    hit.regionEnd   = end;
    return hit;
}

void NewMidiKeyboardComponent::mouseUp (const MouseEvent& e)
{
    if (resizingRegion)
    {
        triggerEditor.endEdgeResize();
        resizingRegion = false;

        if (editMode)
        {
            if (hitTestRegionHandle (e.position).valid)
                setMouseCursor (MouseCursor::DraggingHandCursor);
            else
                setMouseCursor (hitTestRegionEdge (e.position).valid ? MouseCursor::LeftRightResizeCursor
                                                                     : MouseCursor::NormalCursor);
        }
        return;
    }

    updateNoteUnderMouse (e, false);

    auto note = getNoteAndVelocityAtPosition (e.position).note;

    if (note >= 0)
        mouseUpOnKey (note, e);
}

void NewMidiKeyboardComponent::mouseEnter (const MouseEvent& e)
{
    updateNoteUnderMouse (e, false);
}

void NewMidiKeyboardComponent::mouseExit (const MouseEvent& e)
{
    updateNoteUnderMouse (e, false);

    if (hoveredHandleStart >= 0)
    {
        hoveredHandleStart = -1;
        hoveredHandleEnd   = -1;
        repaint();
    }

    if (! resizingRegion)
        setMouseCursor (MouseCursor::NormalCursor);
}

void NewMidiKeyboardComponent::timerCallback()
{
    // Hidden while the macro panel is open — skip provider polling and per-key
    // repaints. Pending MIDI updates wait; setMacroPanelOpen repaints on close.
    if (macroPanelOpen)
        return;

    const bool noMidiUpdates = noPendingUpdates.exchange (true);

    if (noMidiUpdates && ! noteColourProvider && ! noteLabelProvider)
        return;

    bool labelsChanged = false;

    for (auto i = getRangeStart(); i <= getRangeEnd(); ++i)
    {
        const auto isOn = state.isNoteOnForChannels (midiInChannelMask, i);
        bool needsRepaint = false;

        if (keysCurrentlyDrawnDown[i] != isOn)
        {
            keysCurrentlyDrawnDown.setBit (i, isOn);
            needsRepaint = true;
        }

        if (noteColourProvider)
        {
            const auto custom = noteColourProvider (i);
            const uint32_t argb = custom.has_value() ? custom->getARGB() : 0u;

            if (lastColourArgb[(size_t) i] != argb)
            {
                lastColourArgb[(size_t) i] = argb;
                needsRepaint = true;
            }
        }

        if (noteLabelProvider)
        {
            const auto custom = noteLabelProvider (i);
            const String newLabel = custom.has_value() ? *custom : String();

            if (lastLabels[(size_t) i] != newLabel)
            {
                lastLabels[(size_t) i] = newLabel;
                labelsChanged = true;
            }
        }

        if (needsRepaint)
            repaintNote (i);
    }

    // Label content changed somewhere — the leftmost-visible-per-label
    // computation may now resolve to a different note than before, so
    // repaint everything rather than guessing which keys are affected.
    if (labelsChanged)
        repaint();
}

bool NewMidiKeyboardComponent::keyStateChanged (bool /*isKeyDown*/)
{
    bool keyPressUsed = false;

    for (int i = keyPresses.size(); --i >= 0;)
    {
        auto note = 12 * keyMappingOctave + keyPressNotes.getUnchecked (i);

        if (keyPresses.getReference (i).isCurrentlyDown())
        {
            if (! keysPressed[note])
            {
                keysPressed.setBit (note);
                state.noteOn (midiChannel, note, velocity);
                keyPressUsed = true;
            }
        }
        else
        {
            if (keysPressed[note])
            {
                keysPressed.clearBit (note);
                state.noteOff (midiChannel, note, 0.0f);
                keyPressUsed = true;
            }
        }
    }

    return keyPressUsed;
}

bool NewMidiKeyboardComponent::keyPressed (const KeyPress& key)
{
    return keyPresses.contains (key);
}

void NewMidiKeyboardComponent::focusLost (FocusChangeType)
{
    resetAnyKeysInUse();
}

//==============================================================================
void NewMidiKeyboardComponent::drawKeyboardBackground (Graphics& g, Rectangle<float> area)
{
    // Dark background — white keys are painted on top with rounded corners,
    // and the gaps between keys show through as dark separator lines.
    g.fillAll (findColour (keySeparatorLineColourId));

    auto width = area.getWidth();
    auto height = area.getHeight();
    auto currentOrientation = getOrientation();
    Point<float> shadowGradientStart, shadowGradientEnd;

    if (currentOrientation == verticalKeyboardFacingLeft)
    {
        shadowGradientStart.x = width - 1.0f;
        shadowGradientEnd.x   = width - 5.0f;
    }
    else if (currentOrientation == verticalKeyboardFacingRight)
    {
        shadowGradientEnd.x = 5.0f;
    }
    else
    {
        shadowGradientEnd.y = 5.0f;
    }

    auto keyboardWidth = getRectangleForKey (getRangeEnd()).getRight();
    auto shadowColour = findColour (shadowColourId);

    if (! shadowColour.isTransparent())
    {
        g.setGradientFill ({ shadowColour, shadowGradientStart,
                             shadowColour.withAlpha (0.0f), shadowGradientEnd,
                             false });

        switch (currentOrientation)
        {
            case horizontalKeyboard:            g.fillRect (0.0f, 0.0f, keyboardWidth, 5.0f); break;
            case verticalKeyboardFacingLeft:    g.fillRect (width - 5.0f, 0.0f, 5.0f, keyboardWidth); break;
            case verticalKeyboardFacingRight:   g.fillRect (0.0f, 0.0f, 5.0f, keyboardWidth); break;
            default: break;
        }
    }

    auto lineColour = findColour (keySeparatorLineColourId);

    if (! lineColour.isTransparent())
    {
        g.setColour (lineColour);

        switch (currentOrientation)
        {
            case horizontalKeyboard:            g.fillRect (0.0f, height - 1.0f, keyboardWidth, 1.0f); break;
            case verticalKeyboardFacingLeft:    g.fillRect (0.0f, 0.0f, 1.0f, keyboardWidth); break;
            case verticalKeyboardFacingRight:   g.fillRect (width - 1.0f, 0.0f, 1.0f, keyboardWidth); break;
            default: break;
        }
    }
}

void NewMidiKeyboardComponent::drawWhiteNote (int midiNoteNumber, Graphics& g, Rectangle<float> area,
                                           bool isDown, bool isOver, Colour lineColour, Colour textColour)
{
    const auto currentOrientation = getOrientation();
    const auto cornerSize = 4.0f;

    // Inset slightly so dark background shows through as key separator gaps
    auto keyArea = (currentOrientation == horizontalKeyboard)
                       ? area.reduced (0.5f, 0.0f)
                       : area.reduced (0.0f, 0.5f);

    // Build a path with only the bottom corners rounded (for horizontal orientation)
    Path keyShape;
    keyShape.addRoundedRectangle (keyArea.getX(), keyArea.getY(),
                                  keyArea.getWidth(), keyArea.getHeight(),
                                  cornerSize, cornerSize,
                                  false,  // top-left
                                  false,  // top-right
                                  currentOrientation == horizontalKeyboard,   // bottom-left
                                  currentOrientation == horizontalKeyboard);  // bottom-right

    // 1) Fill with the white note base colour. Trigger colour overrides it when
    //    active (opaque); a non-opaque trigger colour means the trigger is
    //    configured here but not currently enabled — paint a faint tint instead.
    auto raw           = noteColourProvider ? noteColourProvider (midiNoteNumber) : std::nullopt;
    const bool stained = raw && ! raw->isOpaque();
    auto custom        = (raw && ! stained) ? raw : std::nullopt;

    Colour baseFill = findColour (whiteNoteColourId);
    if (custom)       baseFill = *custom;
    else if (stained) baseFill = baseFill.interpolatedWith (raw->withAlpha (1.0f), kWhiteStainTint);

    g.setColour (baseFill);
    g.fillPath (keyShape);

    // 2) Subtle top-to-bottom gradient — slightly darker at the bottom,
    //    simulating overhead light hitting the face of the key
    if (currentOrientation == horizontalKeyboard)
    {
        g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.0f),  keyArea.getX(), keyArea.getY(),
                                           Colours::black.withAlpha (0.10f), keyArea.getX(), keyArea.getBottom(),
                                           false));
    }
    else
    {
        g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.0f),  keyArea.getX(), keyArea.getY(),
                                           Colours::black.withAlpha (0.10f), keyArea.getRight(), keyArea.getY(),
                                           false));
    }
    g.fillPath (keyShape);

    // 3) Inner shadow at the top edge — as if the key goes under a panel
    if (currentOrientation == horizontalKeyboard)
    {
        auto shadowHeight = jmin (6.0f, keyArea.getHeight() * 0.04f);
        g.setGradientFill (ColourGradient (Colours::black.withAlpha (0.08f), keyArea.getX(), keyArea.getY(),
                                           Colours::black.withAlpha (0.0f),  keyArea.getX(), keyArea.getY() + shadowHeight,
                                           false));
        g.fillRect (keyArea.withHeight (shadowHeight));
    }

    // 4) Overlay hover / key-down state
    if (isDown || isOver)
    {
        if (custom)
        {
            // Darken the custom colour directly — keeps the hue, just deeper.
            // Hover is a faint tint (kHoverCustomDarken); keyDown stays strong.
            auto c = isDown ? custom->darker (0.65f) : custom->darker (kHoverCustomDarken);
            g.setColour (c);
        }
        else
        {
            auto c = Colours::transparentWhite;
            if (isDown)       c = findColour (keyDownOverlayColourId);
            else if (isOver)  c = findColour (mouseOverKeyOverlayColourId).withMultipliedAlpha (kHoverOverlayAlpha);
            g.setColour (c);
        }

        g.fillPath (keyShape);
    }

    // 5) Text label (C markers)
    auto text = getWhiteNoteText (midiNoteNumber);

    if (text.isNotEmpty())
    {
        auto fontHeight = jmin (14.0f, getKeyWidth() * 0.9f);

        g.setColour (textColour);
        auto fontOptions = FontOptions { fontHeight };
#ifdef DEFAULT_FONT_NAME
            if (! typeface)
            {
                int dataSize = 0;
                if (auto* data = BinaryData::getNamedResource (DEFAULT_FONT_NAME, dataSize))
                    typeface = Typeface::createSystemTypefaceFor (data, (size_t) dataSize);
            }
            if (typeface)
                fontOptions = FontOptions (typeface).withHeight (fontHeight);
#endif
        g.setFont (withDefaultMetrics (fontOptions).withHorizontalScale (0.8f));

        switch (currentOrientation)
        {
            case horizontalKeyboard:            g.drawText (text, keyArea.withTrimmedLeft (1.0f).withTrimmedBottom (7.0f), Justification::centredBottom, false); break;
            case verticalKeyboardFacingLeft:    g.drawText (text, keyArea.reduced (2.0f), Justification::centredLeft,   false); break;
            case verticalKeyboardFacingRight:   g.drawText (text, keyArea.reduced (2.0f), Justification::centredRight,  false); break;
            default: break;
        }
    }
    
    // 6) Darker separator hairlines when this key has a custom colour
    if (custom)
    {
        g.setColour (custom->darker (0.6f));

        if (currentOrientation == horizontalKeyboard)
        {
            g.fillRect (area.getX(),                area.getY(), 0.5f, area.getHeight());
            g.fillRect (area.getRight() - 0.5f,     area.getY(), 0.5f, area.getHeight());
        }
        else
        {
            g.fillRect (area.getX(), area.getY(),               area.getWidth(), 0.5f);
            g.fillRect (area.getX(), area.getBottom() - 0.5f,   area.getWidth(), 0.5f);
        }
    }
    
    // 7) Label (rotated 90° CCW, painted on the leftmost visible note per label)
    drawLabelForKey (g, keyArea, custom, midiNoteNumber);
}

void NewMidiKeyboardComponent::drawBlackNote (int midiNoteNumber, Graphics& g, Rectangle<float> area,
                                           bool isDown, bool isOver, Colour noteFillColour)
{
    // Trigger colour overrides the default fill when active (opaque); a
    // non-opaque trigger colour means the trigger is configured here but not
    // currently enabled — lighten the key and tint it with the trigger hue.
    auto raw           = noteColourProvider ? noteColourProvider (midiNoteNumber) : std::nullopt;
    const bool stained = raw && ! raw->isOpaque();
    auto custom        = (raw && ! stained) ? raw : std::nullopt;

    Colour c;
    if (custom)       c = *custom;
    else if (stained) c = noteFillColour.brighter (kBlackStainLighten).interpolatedWith (raw->withAlpha (1.0f), kBlackStainTint);
    else              c = noteFillColour;

    if (custom)
    {
        if (isDown)      c = c.darker (0.9f);
        else if (isOver) c = c.darker (kHoverCustomDarken);
    }
    else
    {
        if (isDown)      c = c.brighter (0.30f);
        if (isOver)      c = c.overlaidWith (findColour (mouseOverKeyOverlayColourId).withMultipliedAlpha (kHoverOverlayAlpha));
    }

    const auto currentOrientation = getOrientation();
    const auto cornerSize = 4.2f;

    // Bottom gap shrinks by 50% when pressed — gives the "pushed in" illusion
    const auto bottomGapFraction = isDown ? 0.045f : 0.09f;

    // Rounded bottom corners to match white keys
    Path keyShape;
    keyShape.addRoundedRectangle (area.getX(), area.getY(),
                                  area.getWidth(), area.getHeight(),
                                  cornerSize, cornerSize,
                                  false, false,
                                  currentOrientation == horizontalKeyboard,
                                  currentOrientation == horizontalKeyboard);

    // 1) Drop shadow rendered before the key fill
    blackKeyShadow.render (g, keyShape);

    // 2) Base fill (the dark sides/edges)
    g.setColour (c);
    g.fillPath (keyShape);
    
    // 2) Lighter face on the upper portion of the key
    auto faceArea = area;
    
    if (currentOrientation == horizontalKeyboard)
        faceArea = area.reduced (area.getWidth() * 0.12f, 0.0f)
        .withTrimmedBottom (area.getHeight() * bottomGapFraction);
    else if (currentOrientation == verticalKeyboardFacingLeft)
        faceArea = area.reduced (0.0f, area.getHeight() * 0.12f)
        .withTrimmedLeft (area.getWidth() * bottomGapFraction);
    else
        faceArea = area.reduced (0.0f, area.getHeight() * 0.12f)
        .withTrimmedRight (area.getWidth() * bottomGapFraction);
    
    Path faceShape;
    faceShape.addRoundedRectangle (faceArea.getX(), faceArea.getY(),
                                   faceArea.getWidth(), faceArea.getHeight(),
                                   cornerSize * 0.8f, cornerSize * 0.8f,
                                   false, false,
                                   currentOrientation == horizontalKeyboard,
                                   currentOrientation == horizontalKeyboard);
    
    const auto faceTint = custom
    ? (isDown ?  0.14f :  0.10f)   // slight less brighter on coloured keys
    : (isDown ?  0.14f :  0.22f);  // original brighten for default look
    
    if (! isDown){
        g.setColour (faceTint >= 0.0f ? c.brighter (faceTint) : c.darker (-faceTint));
        g.fillPath (faceShape);
    }

    // 3) Specular highlight — soft bright spot on the upper third of the face
    if (currentOrientation == horizontalKeyboard)
    {
        auto specHeight = faceArea.getHeight() * 0.35f;
        g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.09f), faceArea.getX(), faceArea.getY(),
                                           Colours::white.withAlpha (0.0f),  faceArea.getX(), faceArea.getY() + specHeight,
                                           false));
        g.fillRect (faceArea.withHeight (specHeight));
    }

    // 4) Thin highlight line along the top rim of the face
    if (currentOrientation == horizontalKeyboard && ! isDown)
    {
        g.setColour (Colours::white.withAlpha (0.07f));
        g.fillRect (faceArea.withHeight (1.0f));
    }

    // 5) Gradient for depth — lighter at top, slightly darker at bottom
    //    (skipped when pressed, since the white-at-top component reads
    //    as sheen and we want the pressed state matte)
    if (! isDown)
    {
        if (currentOrientation == horizontalKeyboard)
        {
            g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.10f), area.getX(), area.getY(),
                                               Colours::black.withAlpha (0.06f), area.getX(), area.getBottom(),
                                               false));
        }
        else
        {
            g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.10f), area.getX(), area.getY(),
                                               Colours::black.withAlpha (0.06f), area.getRight(), area.getY(),
                                               false));
        }

        g.fillPath (keyShape);
    }

    // 6) Dark crease where the face meets the lower ledge
    if (currentOrientation == horizontalKeyboard)
    {
        g.setColour (Colours::black.withAlpha (0.18f));
        g.fillRect (faceArea.getX(), faceArea.getBottom(), faceArea.getWidth(), 1.0f);
    }

    // 6b) Subtle specular highlight at the lower corners of the face —
    //     light catching the rounded bottom edge
    if (currentOrientation == horizontalKeyboard)
    {
        auto highlightHeight = faceArea.getHeight() * 0.12f;
        auto highlightY = faceArea.getBottom() - highlightHeight;
        g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.0f),  faceArea.getX(), highlightY,
                                           Colours::white.withAlpha (0.06f), faceArea.getX(), faceArea.getBottom(),
                                           false));
        g.fillRect (faceArea.getX(), highlightY, faceArea.getWidth(), highlightHeight);
    }

    // 7) Overlay key-down tint — only for default-look keys. Custom-coloured
    //    keys are already pressed-state-darkened
    if (isDown && ! custom)
    {
        g.setColour (findColour (keyDownOverlayColourId).withMultipliedAlpha (0.4f));
        g.fillPath (keyShape);
    }
    
    // 8) Label (rotated 90° CCW, painted on the leftmost visible note per label)
    drawLabelForKey (g, area, custom, midiNoteNumber);
}

String NewMidiKeyboardComponent::getWhiteNoteText (int midiNoteNumber)
{
    if (midiNoteNumber % 12 == 0)
        return MidiMessage::getMidiNoteName (midiNoteNumber, true, true, getOctaveForMiddleC());

    return {};
}

void NewMidiKeyboardComponent::colourChanged()
{
    setOpaque (findColour (whiteNoteColourId).isOpaque());
    repaint();
}

void NewMidiKeyboardComponent::drawLabelForKey (Graphics& g, Rectangle<float> keyArea,
                                                std::optional<Colour> baseFill,
                                                int midiNoteNumber)
{
    if (! noteLabelProvider)
        return;

    // Labels are only drawn on keys whose trigger is fully active; for
    // stained / unset keys baseFill is nullopt and we skip.
    if (! baseFill.has_value())
        return;

    const auto myLabel = noteLabelProvider (midiNoteNumber);
    if (! myLabel.has_value() || myLabel->isEmpty())
        return;

    // X fraction within the key — black-key overlap pushes C/F leftwards
    // and E/B rightwards; everything else stays centred.
    auto xFraction = [] (int n)
    {
        switch (n % 12)
        {
            case 0:           return 0.34f;  // C
            case 4: case 11:  return 0.75f;  // E, B
            case 5:           return 0.25f;  // F
            case 7:           return 0.45f;  // G
            case 9:           return 0.60f;  // A
            default:          return 0.50f;  // D, and all black keys
        }
    };

    auto labelCentre = [&] (int n, Rectangle<float> r)
    {
        return Point<float> { r.getX() + r.getWidth() * xFraction (n), r.getCentreY() };
    };

    // Visibility area: shrink by the scroll-button overlay so labels
    // don't hide under the buttons at the keyboard's ends.
    auto bounds = getLocalBounds().toFloat();
    // + half the rotated label's horizontal extent (font height ≤ 13) so
    // the label itself — not just its centre — clears the scroll buttons.
    const auto buttonInset = (float) getScrollButtonWidth() + 7.0f;
    bounds = (getOrientation() == horizontalKeyboard)
                 ? bounds.reduced (buttonInset, 0.0f)
                 : bounds.reduced (0.0f, buttonInset);

    const auto myCentre = labelCentre (midiNoteNumber, keyArea);
    if (! bounds.contains (myCentre))
        return;

    // Walk backwards within our contiguous range. The moment we hit a note
    // that doesn't carry our label, we've crossed into a different range
    // (or a gap) and any earlier same-label notes belong to a separate
    // trigger that should render its own label.
    for (int j = midiNoteNumber - 1; j >= getRangeStart(); --j)
    {
        const auto other = noteLabelProvider (j);
        if (! other.has_value() || *other != *myLabel)
            break;

        if (bounds.contains (labelCentre (j, getRectangleForKey (j))))
            return;
    }

    // Draw the rotated label.
    Graphics::ScopedSaveState save (g);
    g.addTransform (AffineTransform::rotation (-MathConstants<float>::halfPi, myCentre.x, myCentre.y));

    const auto labelBounds = Rectangle<float> (keyArea.getHeight(), keyArea.getWidth()).withCentre (myCentre);
    const auto fontHeight  = jmin (15.0f, keyArea.getWidth() * 0.65f);

    auto fontOptions = FontOptions { fontHeight };
#ifdef DEFAULT_FONT_NAME
        if (! typeface)
        {
            int dataSize = 0;
            if (auto* data = BinaryData::getNamedResource (DEFAULT_FONT_NAME, dataSize))
                typeface = Typeface::createSystemTypefaceFor (data, (size_t) dataSize);
        }
        if (typeface)
            fontOptions = FontOptions (typeface).withHeight (fontHeight);
#endif
    g.setFont (withDefaultMetrics (fontOptions));

    g.setColour (findColour (textLabelColourId));
    g.drawFittedText (*myLabel, labelBounds.toNearestInt(), Justification::centred, 1, 0.85f);
}

//==============================================================================
void NewMidiKeyboardComponent::drawWhiteKey (int midiNoteNumber, Graphics& g, Rectangle<float> area)
{
    drawWhiteNote (midiNoteNumber, g, area, state.isNoteOnForChannels (midiInChannelMask, midiNoteNumber),
                   mouseOverNotes.contains (midiNoteNumber), findColour (keySeparatorLineColourId), findColour (textLabelColourId));
}

void NewMidiKeyboardComponent::drawBlackKey (int midiNoteNumber, Graphics& g, Rectangle<float> area)
{
    drawBlackNote (midiNoteNumber, g, area, state.isNoteOnForChannels (midiInChannelMask, midiNoteNumber),
                   mouseOverNotes.contains (midiNoteNumber), findColour (blackNoteColourId));
}

//==============================================================================
void NewMidiKeyboardComponent::handleNoteOn (MidiKeyboardState*, int /*midiChannel*/, int /*midiNoteNumber*/, float /*velocity*/)
{
    noPendingUpdates.store (false);
}

void NewMidiKeyboardComponent::handleNoteOff (MidiKeyboardState*, int /*midiChannel*/, int /*midiNoteNumber*/, float /*velocity*/)
{
    noPendingUpdates.store (false);
}

//==============================================================================
bool NewMidiKeyboardComponent::mouseDownOnKey (int midiNoteNumber, const MouseEvent& e)
{
#if MAX_MIDI_TRIGGERS > 0
    if (editMode)
    {
        // In edit mode a click opens the trigger menu instead of playing.
        triggerEditor.showMenuForKey (midiNoteNumber, e.getScreenPosition());
        return false;
    }
#endif

    return true;
}

bool NewMidiKeyboardComponent::mouseDraggedToKey ([[maybe_unused]] int midiNoteNumber, [[maybe_unused]] const MouseEvent& e)
{
    // Suppress drag-play while editing.
    return ! editMode;
}

void NewMidiKeyboardComponent::mouseUpOnKey ([[maybe_unused]] int midiNoteNumber, [[maybe_unused]] const MouseEvent& e) {}

//==============================================================================
// Edit mode — keyboard side (flag, active-group highlight, outline drawing).
//==============================================================================

void NewMidiKeyboardComponent::setEditMode (bool shouldBeOn)
{
    if (editMode == shouldBeOn)
        return;

    editMode = shouldBeOn;

    if (! editMode)
    {
        setActiveEditNote (-1);
        resizingRegion = false;
        triggerEditor.endEdgeResize();
        setMouseCursor (MouseCursor::NormalCursor);
    }

    repaint();
}

void NewMidiKeyboardComponent::setMacroButtonVisible (bool shouldBeVisible)
{
    if (macroButtonVisible == shouldBeVisible)
        return;

    macroButtonVisible = shouldBeVisible;
    repaint();
}

void NewMidiKeyboardComponent::setMacroPanelOpen (bool isOpen)
{
    if (macroPanelOpen == isOpen)
        return;

    macroPanelOpen = isOpen;

    if (isOpen)
    {
        // Hiding the scroll buttons snaps the keyboard back to its leftmost key,
        // so stash the current scroll position to restore when we reshow.
        savedLowestVisibleKey = getLowestVisibleKey();
        setOpaque (false);
        setScrollButtonsVisible (false);
    }
    else
    {
        setScrollButtonsVisible (true);
        if (savedLowestVisibleKey >= 0)
            setLowestVisibleKey (savedLowestVisibleKey);   // restore after re-enabling so it sticks
        setOpaque (findColour (whiteNoteColourId).isOpaque());
    }

    repaint();
}

void NewMidiKeyboardComponent::setEditContext (foleys::MagicGUIBuilder* b, const String& id)
{
    triggerEditor.setContext (b, id);
}

void NewMidiKeyboardComponent::setTriggerPresetFolder (const File& folder)
{
    triggerEditor.setPresetFolder (folder);
}

void NewMidiKeyboardComponent::createFactoryTriggerPresets()
{
#if MAX_MIDI_TRIGGERS > 0
    File triggersFolder;

#if JUCE_WINDOWS
    triggersFolder = File::getSpecialLocation (File::SpecialLocationType::windowsLocalAppData)
                        .getChildFile (ProjectInfo::companyName)
                        .getChildFile (ProjectInfo::projectName)
                        .getChildFile ("Triggers");
#elif JUCE_MAC
    triggersFolder = File::getRealUserHomeDirectory()
                        .getChildFile ("Library")
                        .getChildFile ("Application Support")
                        .getChildFile (ProjectInfo::companyName)
                        .getChildFile (ProjectInfo::projectName)
                        .getChildFile ("Triggers");
#elif JUCE_IOS
    triggersFolder = File::getContainerForSecurityApplicationGroupIdentifier (APP_GROUP_ID)
                        .getChildFile (ProjectInfo::companyName)
                        .getChildFile (ProjectInfo::projectName)
                        .getChildFile ("Triggers");
#else
    triggersFolder = File::getSpecialLocation (File::userApplicationDataDirectory)
                        .getChildFile (ProjectInfo::companyName)
                        .getChildFile (ProjectInfo::projectName)
                        .getChildFile ("Triggers");
#endif

    if (triggersFolder == File())   // invalid (e.g. missing App Group entitlement)
        return;

    const bool firstRun = ! triggersFolder.isDirectory();

    if (const auto r = triggersFolder.createDirectory(); r.failed())
    {
        DBG ("Could not create Triggers folder: " + r.getErrorMessage());
        jassertfalse;
        return;
    }

    //==========================================================================
    // Base library: the zip is the bulk container, unpacked once on first run.
    //==========================================================================
    if (firstRun)
    {
        int size = 0;
        if (const char* data = BinaryData::getNamedResource ("Triggers_zip", size))  // "Triggers.zip" → "Triggers_zip"
        {
            MemoryInputStream mis (data, (size_t) size, false);
            ZipFile zip (mis);

            if (const auto r = zip.uncompressTo (triggersFolder, /*shouldOverwriteFiles*/ false); r.failed())
            {
                DBG ("Could not unpack factory triggers: " + r.getErrorMessage());
                jassertfalse;
            }
            else
            {
                // macOS Archive Utility sidecar — never wanted.
                triggersFolder.getChildFile ("__MACOSX").deleteRecursively();

                // A Finder-zipped folder leaves a redundant Triggers/Triggers/… nest.
                // Hoist: rename outer aside, lift inner up, drop the husk.
                if (auto nested = triggersFolder.getChildFile (triggersFolder.getFileName());
                    nested.isDirectory())
                {
                    auto temp = triggersFolder.getSiblingFile ("TriggersTemp");
                    temp.deleteRecursively();
                    triggersFolder.moveFileTo (temp);                       // Triggers -> TriggersTemp
                    temp.getChildFile (triggersFolder.getFileName())
                        .moveFileTo (triggersFolder);                       // TriggersTemp/Triggers -> Triggers
                    temp.deleteRecursively();                               // drop empty TriggersTemp
                }
            }
        }
    }

    //==========================================================================
    // Supplementary individual triggers: checked every run, so an update can
    // drop new "folder--folder--My Trigger.trigger" resources into the binary
    // without rebuilding the zip. Cheap (suffix + exists checks); existing
    // files are never clobbered. Up to 10 nested folder levels.
    //==========================================================================
    const String dotExt = ".trigger";

    for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
    {
        const char*  resourceName = BinaryData::namedResourceList[i];
        const String original     = BinaryData::getNamedResourceOriginalFilename (resourceName);

        if (! original.endsWithIgnoreCase (dotExt))   // skips the zip (.zip), fonts, images, etc.
            continue;

        // Unroll the "--" path: every segment before the last is a folder.
        File   dest      = triggersFolder;
        String remaining = original;

        for (int depth = 0; depth < 10 && remaining.contains ("--"); ++depth)
        {
            dest      = dest.getChildFile (remaining.upToFirstOccurrenceOf ("--", false, false).trim());
            remaining = remaining.fromFirstOccurrenceOf ("--", false, false);
        }
        // remaining is now the filename, e.g. "My Trigger.trigger"

        if (const auto r = dest.createDirectory(); r.failed())   // creates intermediate parents
        {
            DBG ("Could not create trigger folder '" + dest.getFullPathName() + "': " + r.getErrorMessage());
            jassertfalse;
            continue;
        }

        int dataSize = 0;
        if (const char* fileData = BinaryData::getNamedResource (resourceName, dataSize))
        {
            const File outFile = dest.getChildFile (remaining);
            if (! outFile.exists())
                outFile.replaceWithData (fileData, (size_t) dataSize);
        }
    }
#endif
}

void NewMidiKeyboardComponent::setActiveEditNote (int note)
{
    activeEditNote = note;
    activeRunStart = -1;
    activeRunEnd   = -1;

    // Expand the active note's contiguous same-colour run so the whole group
    // can be drawn with the heavier outline while its menu is open.
    if (note >= 0 && noteColourProvider)
    {
        const auto here = noteColourProvider (note);
        if (here.has_value())
        {
            int lo = note, hi = note;
            while (lo - 1 >= getRangeStart() && noteColourProvider (lo - 1) == here) --lo;
            while (hi + 1 <= getRangeEnd()   && noteColourProvider (hi + 1) == here) ++hi;
            activeRunStart = lo;
            activeRunEnd   = hi;
        }
    }

    repaint();
}

void NewMidiKeyboardComponent::drawEditOutlines (Graphics& g)
{
    if (! editMode || ! noteColourProvider)
        return;

    auto colourAt = [&] (int n) -> std::optional<Colour>
    {
        if (n < getRangeStart() || n > getRangeEnd())
            return {};
        return noteColourProvider (n);
    };

    // Walk the range, grouping maximal runs of consecutive notes that share a
    // colour. Each run is one zone; stroke its silhouette as a single path.
    for (int n = getRangeStart(); n <= getRangeEnd(); )
    {
        const auto c = colourAt (n);
        if (! c.has_value()) { ++n; continue; }

        const int start = n;
        while (n + 1 <= getRangeEnd() && colourAt (n + 1) == c)
            ++n;
        drawZoneOutline (g, start, n);
        ++n;
    }
}

void NewMidiKeyboardComponent::drawZoneOutline (Graphics& g, int startNote, int endNote)
{
    // Turtle-trace the zone's outer silhouette as one path and stroke it. The
    // top and bottom are straight lines; the left and right sides step in an L
    // around the black keys (a black key at the start/end protrudes past the
    // white keys; a foreign black key overlapping the boundary white key's top
    // corner notches in). Because the path is the *visible* boundary, stroking
    // it over the keys is correct — no masking needed and corners are clean.
    const bool active = (activeRunStart >= 0
                         && startNote <= activeRunEnd
                         && endNote   >= activeRunStart);
    const float t = active ? 3.0f : 1.5f;

    auto isBlack = [] (int note) { return MidiMessage::isMidiNoteBlack (note); };

    g.setColour (findColour (editOutlineColourId));

    // The silhouette tracing below is horizontal-only. For vertical keyboards,
    // fall back to stroking the zone's bounding box (no black-key stepping).
    if (getOrientation() != horizontalKeyboard)
    {
        auto bounds = getRectangleForKey (startNote);
        for (int nn = startNote + 1; nn <= endNote; ++nn)
            bounds = bounds.getUnion (getRectangleForKey (nn));

        Path bp;
        bp.addRectangle (bounds.reduced (t * 0.5f));
        g.strokePath (bp, PathStrokeType (t));
        return;
    }

    // A lone black key never reaches the keyboard bottom, so it has no white
    // keys and its silhouette is just its own rectangle.
    const int leftWhite  = isBlack (startNote) ? startNote + 1 : startNote;
    const int rightWhite = isBlack (endNote)   ? endNote   - 1 : endNote;
    if (leftWhite > rightWhite)
    {
        Path bp;
        bp.addRectangle (getRectangleForKey (startNote).reduced (t * 0.5f));
        g.strokePath (bp, PathStrokeType (t));
        return;
    }

    // The stroke is centred on the path, so a rim sitting exactly on the key
    // top/bottom would have half its width clipped by the component bounds and
    // render thinner than the (un-clipped) interior verticals. Pull the rims in
    // by half the stroke width so the whole line stays inside and all edges read
    // the same width.
    const float halfT  = t * 0.5f;
    const float top    = getRectangleForKey (startNote).getY()      + halfT;
    const float bottom = getRectangleForKey (leftWhite).getBottom() - halfT;

    // ---- left side ----------------------------------------------------------
    // Derive the left seam from the PREVIOUS white key's right edge rather than
    // this zone's leftmost white's left edge. That's the very same key the zone
    // on our left uses for its right seam, so the shared vertical lands on the
    // exact same pixel from both sides (JUCE's key layout rounds the two edges
    // ~1px apart otherwise, which reads as a fatter vertical). At the keyboard's
    // left end there's no previous key, so fall back to our own left edge.
    const int   prevWhite   = (leftWhite - 1 >= getRangeStart() && isBlack (leftWhite - 1)) ? leftWhite - 2
                                                                                            : leftWhite - 1;
    const float bottomLeftX = (prevWhite >= getRangeStart()) ? getRectangleForKey (prevWhite).getRight()
                                                             : getRectangleForKey (leftWhite).getX();
    float topLeftX = bottomLeftX;
    float blackBottomL = 0.0f;
    bool  leftStepped = false;
    if (isBlack (startNote))                       // black start → protrudes left
    {
        const auto k = getRectangleForKey (startNote);
        topLeftX     = k.getX();
        blackBottomL = k.getBottom();
        leftStepped  = true;
    }
    else if (startNote - 1 >= getRangeStart() && isBlack (startNote - 1))  // foreign black over top-left → notch in
    {
        const auto k = getRectangleForKey (startNote - 1);
        topLeftX     = k.getRight();
        blackBottomL = k.getBottom();
        leftStepped  = true;
    }

    // ---- right side ---------------------------------------------------------
    const float bottomRightX = getRectangleForKey (rightWhite).getRight();
    float topRightX = bottomRightX;
    float blackBottomR = 0.0f;
    bool  rightStepped = false;
    if (isBlack (endNote))                         // black end → protrudes right
    {
        const auto k = getRectangleForKey (endNote);
        topRightX    = k.getRight();
        blackBottomR = k.getBottom();
        rightStepped = true;
    }
    else if (endNote + 1 <= getRangeEnd() && isBlack (endNote + 1))        // foreign black over top-right → notch in
    {
        const auto k = getRectangleForKey (endNote + 1);
        topRightX    = k.getX();
        blackBottomR = k.getBottom();
        rightStepped = true;
    }

    // ---- trace --------------------------------------------------------------
    Path p;
    p.startNewSubPath (topLeftX, top);
    p.lineTo (topRightX, top);                     // top
    if (rightStepped)                              // down the right side, stepping at the black-key bottom
    {
        p.lineTo (topRightX,    blackBottomR);
        p.lineTo (bottomRightX, blackBottomR);
        p.lineTo (bottomRightX, bottom);
    }
    else
        p.lineTo (bottomRightX, bottom);
    p.lineTo (bottomLeftX, bottom);                // bottom
    if (leftStepped)                               // up the left side, stepping at the black-key bottom
    {
        p.lineTo (bottomLeftX, blackBottomL);
        p.lineTo (topLeftX,    blackBottomL);
        p.lineTo (topLeftX,    top);
    }
    else
        p.lineTo (topLeftX, top);
    p.closeSubPath();

    g.strokePath (p, PathStrokeType (t));
}

//==============================================================================
// TriggerEditor — the data side of inline editing.
//==============================================================================

ValueTree NewMidiKeyboardComponent::TriggerEditor::findNodeByID (ValueTree tree, const String& targetID,
                                                                 Identifier excludeType)
{
    // excludeType, when valid, suppresses matches on nodes of that type — the
    // walk still recurses through them, but they don't satisfy the search.
    // Used by the extras-dedup pass in insertPayloadAtKey to ignore MidiTrigger
    // matches (which represent the freshly-placed slot, not a pre-existing
    // linkable sibling).
    if ((! excludeType.isValid() || tree.getType() != excludeType)
        && tree.hasProperty ("id")
        && tree.getProperty ("id").toString().equalsIgnoreCase (targetID))
        return tree;

    for (auto child : tree)
        if (auto found = findNodeByID (child, targetID, excludeType); found.isValid())
            return found;

    return {};
}

ValueTree NewMidiKeyboardComponent::TriggerEditor::getContainer() const
{
    if (builder == nullptr || containerID.isEmpty())
        return {};

    return findNodeByID (builder->getGuiRootNode(), containerID);
}

bool NewMidiKeyboardComponent::TriggerEditor::nodeCoversKey (const ValueTree& node, int note)
{
    for (const auto& pair : kTriggerRangePairs)
    {
        const Identifier lo (pair.lo), hi (pair.hi);
        if (node.hasProperty (lo) && node.hasProperty (hi))
            if (note >= (int) node.getProperty (lo) && note <= (int) node.getProperty (hi))
                return true;
    }
    return false;
}

Array<ValueTree> NewMidiKeyboardComponent::TriggerEditor::findNodesForKey (int note) const
{
    Array<ValueTree> result;
    auto container = getContainer();
    if (container.isValid())
        for (auto child : container)
            if (nodeCoversKey (child, note))
                result.add (child);
    return result;
}

bool NewMidiKeyboardComponent::TriggerEditor::isEmptySlot (const ValueTree& node)
{
    static const Identifier midiTriggerType ("MidiTrigger");
    static const Identifier indexProp ("trigger-index");

    // Free == a MidiTrigger carrying nothing but its trigger-index, i.e.
    // exactly what clearPropertiesOfNode leaves behind. (Order-independent.)
    if (node.getType() != midiTriggerType)
        return false;

    for (int i = 0; i < node.getNumProperties(); ++i)
        if (node.getPropertyName (i) != indexProp)
            return false;

    return true;
}

Array<ValueTree> NewMidiKeyboardComponent::TriggerEditor::findEmptySlots (const ValueTree& container, int count) const
{
    // Fill the lowest free trigger-index slots first, regardless of child order.
    int maxIdx = -1;
    for (auto child : container)
        if (child.hasProperty ("trigger-index"))
            maxIdx = jmax (maxIdx, (int) child.getProperty ("trigger-index"));

    Array<ValueTree> result;
    for (int idx = 0; idx <= maxIdx && result.size() < count; ++idx)
        for (auto child : container)
            if ((int) child.getProperty ("trigger-index", -1) == idx && isEmptySlot (child))
            {
                result.add (child);
                break;
            }

    return result;
}

ValueTree NewMidiKeyboardComponent::TriggerEditor::propertiesOnlyCopy (const ValueTree& node)
{
    // Mirrors TriggerBankEditorComponent::copySelected: a fresh node of the
    // same type carrying every property except trigger-index (the destination
    // slot supplies its own). A single bare node is byte-compatible with the
    // bank editor's clipboard, so the two interoperate.
    static const Identifier indexProp ("trigger-index");

    ValueTree out (node.getType());
    for (int i = 0; i < node.getNumProperties(); ++i)
    {
        const auto name = node.getPropertyName (i);
        if (name != indexProp)
            out.setProperty (name, node.getProperty (name), nullptr);
    }
    return out;
}

void NewMidiKeyboardComponent::TriggerEditor::clearPropertiesOfNode (ValueTree node, UndoManager* um)
{
    // Identical to TriggerBankEditorComponent::clearPropertiesOfNode: strip
    // everything but trigger-index, building the list before removing (you
    // can't remove-by-index while iterating). Children are left untouched.
    if (! node.isValid())
        return;

    static const Identifier indexProp ("trigger-index");

    Array<Identifier> toRemove;
    for (int i = 0; i < node.getNumProperties(); ++i)
    {
        const auto name = node.getPropertyName (i);
        if (name != indexProp)
            toRemove.add (name);
    }

    for (auto& name : toRemove)
        node.removeProperty (name, um);
}

void NewMidiKeyboardComponent::TriggerEditor::fillSlot (ValueTree slot, const ValueTree& payload, UndoManager* um)
{
    // Replace semantics, mirroring pasteToSelected: clear the slot's
    // properties (trigger-index survives) then copy the payload's in.
    clearPropertiesOfNode (slot, um);

    static const Identifier indexProp ("trigger-index");
    for (int i = 0; i < payload.getNumProperties(); ++i)
    {
        const auto name = payload.getPropertyName (i);
        if (name != indexProp)
            slot.setProperty (name, payload.getProperty (name), um);
    }
}

void NewMidiKeyboardComponent::TriggerEditor::remapNoteRanges (ValueTree node, int delta)
{
    if (delta == 0)
        return;

    // Pre-insert config on a detached copy — the appendChild is the undoable
    // unit, so no UndoManager here. Delta is already clamped as a unit by the
    // caller, so individual properties are never clamped (that would distort
    // the spacing we're preserving).
    for (const auto& p : kTriggerNoteValuedProps)
    {
        const Identifier prop (p);
        if (node.hasProperty (prop))
            node.setProperty (prop, (int) node.getProperty (prop) + delta, nullptr);
    }
}

void NewMidiKeyboardComponent::TriggerEditor::copyKeys (const Array<ValueTree>& nodes)
{
    if (nodes.isEmpty())
        return;

    static const Identifier midiTriggerType ("MidiTrigger");

    // findNodesForKey only returns triggers whose select/trigger/etc. range
    // covers the clicked key. For a multi-trigger group (e.g. Note_Repeat's
    // SPEED + REPEAT triggers under one id, with a View carrying the LFO and
    // controls), clicking the SPEED key would otherwise hand us just the one
    // trigger. Expand the selection by group id — same sweep doClear runs on
    // the way out — so Copy / Cut grab the whole group. MidiTrigger siblings
    // use propertiesOnlyCopy (slot index dropped, no children to worry about);
    // non-trigger siblings use a full createCopy so View / nested LFO content
    // survives.
    StringArray groupIDs;
    for (const auto& n : nodes)
    {
        const auto id = n.getProperty ("id").toString();
        if (id.isNotEmpty())
            groupIDs.addIfNotAlreadyThere (id);
    }

    Array<ValueTree> triggers, extras;
    for (auto& n : nodes)
        triggers.add (propertiesOnlyCopy (n));

    if (! groupIDs.isEmpty())
        if (auto container = getContainer(); container.isValid())
            for (auto child : container)
            {
                if (nodes.contains (child))
                    continue;   // already captured above

                const auto childID = child.getProperty ("id").toString();
                if (childID.isEmpty() || ! groupIDs.contains (childID, true))
                    continue;

                if (child.getType() == midiTriggerType)
                    triggers.add (propertiesOnlyCopy (child));
                else
                    extras.add (child.createCopy());
            }

    const int total = triggers.size() + extras.size();
    if (total == 1)
    {
        // Bare properties-only node — byte-compatible with the bank editor's
        // clipboard, so a single-key copy pastes into the node editor too.
        SystemClipboard::copyTextToClipboard (triggers.getFirst().toXmlString());
    }
    else
    {
        // _multiCopy wrapper for a group — matches the ToolBox / snippet
        // convention. Triggers come first so the file reads naturally (slot
        // pool up top, support nodes after) and matches the authored order
        // in shipped snippets.
        ValueTree multi ("_multiCopy");
        for (auto& n : triggers) multi.appendChild (n, nullptr);
        for (auto& n : extras)   multi.appendChild (n, nullptr);
        SystemClipboard::copyTextToClipboard (multi.toXmlString());
    }
}

void NewMidiKeyboardComponent::TriggerEditor::insertPayloadAtKey (ValueTree payloadRoot, int note)
{
    auto container = getContainer();
    if (! container.isValid() || ! payloadRoot.isValid())
        return;

    static const Identifier midiTriggerType  ("MidiTrigger");
    static const Identifier midiEditorType   ("MidiEditor");
    static const Identifier midiStateIdxProp ("midi-state-index");
    static const Identifier idProp           ("id");

    // ── Step 1 — split the payload into triggers (slot-pooled) and extras
    // (appended). MidiTriggers always go through findEmptySlots; everything
    // else (LFOs, Mappers, ListBoxes, ...) lands at the end of the container.
    Array<ValueTree> payload, extras;
    auto take = [&] (const ValueTree& src)
    {
        if (src.getType() == midiTriggerType) payload.add (src.createCopy());
        else                                   extras.add  (src.createCopy());
    };

    if (payloadRoot.getType().toString() == "_multiCopy")
        for (auto child : payloadRoot)
            take (child);
    else
        take (payloadRoot);

    if (payload.isEmpty())
        return;

    // ── Step 2 — transpose delta from the trigger note-range props.
    // Computed on the payload triggers only (extras don't carry these); the
    // delta is then applied to triggers via remapNoteRanges and to range-
    // follower extras (Mapper / Arpeggiator / LFO / Envelope) in Step 7.
    int lowest  = std::numeric_limits<int>::max();
    int highest = std::numeric_limits<int>::min();
    for (auto& n : payload)
        for (const auto& p : kTriggerNoteValuedProps)
        {
            const Identifier prop (p);
            if (n.hasProperty (prop))
            {
                const int v = (int) n.getProperty (prop);
                lowest  = jmin (lowest, v);
                highest = jmax (highest, v);
            }
        }

    int delta = 0;
    if (lowest != std::numeric_limits<int>::max())
    {
        if (highest - lowest > 127)
            return;

        delta = note - lowest;
        if (lowest  + delta < 0)   delta = -lowest;
        if (highest + delta > 127) delta = 127 - highest;
    }

    // ── Step 3 — uniquify uniquified UIDs and ":" values across the whole
    // bundle. Standalone refs only; composites (e.g. a Playlist score string
    // carrying clip UIDs) reach outside the bundle and stay verbatim. See
    // uniquifyPayloadRefs for the detection rules.
    uniquifyPayloadRefs (payload, extras);

    // ── Step 4 — group-id collision policy.
    //
    // Nodes sharing an id form one logical group (clearKey / copyKeys treat
    // them as a unit). When an incoming group's id is already present in the
    // container:
    //   * Regular ids get a Finder-style " (n)" suffix, applied uniformly to
    //     every node in the bundle that shared the old id. Cross-refs inside
    //     the bundle remain consistent because the rename map is computed
    //     once before any writes.
    //   * "[global]" ids are dropped from the bundle entirely — the existing
    //     companion is shared. If a [global]'s UIDs were stamped by Step 3,
    //     references from the bundle now point to a node that won't land;
    //     authors of [global] singletons should use absolute UIDs
    //     (e.g. "LFO_main") to avoid this. See doClear for the matching
    //     companion-removal rule.
    StringArray bundleIDs, takenIDs;
    for (const auto& n : payload) { auto id = n.getProperty (idProp).toString(); if (id.isNotEmpty()) bundleIDs.addIfNotAlreadyThere (id); }
    for (const auto& n : extras)  { auto id = n.getProperty (idProp).toString(); if (id.isNotEmpty()) bundleIDs.addIfNotAlreadyThere (id); }

    for (auto child : container)
    {
        const auto id = child.getProperty (idProp).toString();
        if (id.isNotEmpty()) takenIDs.addIfNotAlreadyThere (id);
    }

    std::map<String, String> idMap;   // old id -> new id for Finder renames
    StringArray               skipIDs; // [global] ids already present
    for (const auto& id : bundleIDs)
    {
        if (! takenIDs.contains (id, true))
            continue;

        if (isGlobalID (id))
        {
            skipIDs.add (id);
            continue;
        }

        for (int n = 2; n < 10000; ++n)
        {
            const auto candidate = id + " (" + String (n) + ")";
            if (! takenIDs.contains (candidate, true))
            {
                idMap[id] = candidate;
                takenIDs.add (candidate);   // steer subsequent renames around it
                break;
            }
        }
    }

    auto applyIDMap = [&] (Array<ValueTree>& arr)
    {
        for (auto& n : arr)
        {
            const auto id = n.getProperty (idProp).toString();
            if (auto it = idMap.find (id); it != idMap.end())
                n.setProperty (idProp, it->second, nullptr);
        }
    };
    applyIDMap (payload);
    applyIDMap (extras);

    auto dropSkipped = [&] (Array<ValueTree>& arr)
    {
        for (int i = arr.size(); --i >= 0;)
            if (skipIDs.contains (arr[i].getProperty (idProp).toString(), true))
                arr.remove (i);
    };
    dropSkipped (payload);
    dropSkipped (extras);

    if (payload.isEmpty())
        return;   // every trigger in the bundle was a [global] that's already loaded

    // ── Step 5 — macro allocation. The only place `parameter` is automatically
    // reassigned. For each PropertyControl in the bundle whose `parameter` is
    // non-empty, claim the lowest-free macro in its control-type's pool (skipping
    // ids already bound in the live container, already claimed in this bundle, or
    // not present in the patch), rewrite `parameter`, and seed the macro from
    // `default`. Exhausted / missing param → clear `parameter` so the control
    // falls back to property-stored. Writes target the off-tree copies (nullptr),
    // exactly like the id rewrites above; the seed touches the live param.
    {
        static const Identifier pParameter      ("parameter");
        static const Identifier pControlType    ("control-type");
        static const Identifier pDefinitions    ("definitions");
        static const Identifier pDefault        ("default");
        static const Identifier propControlType ("PropertyControl");

        auto& magicState = builder->getMagicState();

        // inlined seed - same as the processor's seedMacro, reached straight through
        // magicState so we don't need the concrete processor type here.
        auto seedMacro = [&] (const String& macroID, float plainValue)
        {
            if (auto* p = magicState.getParameter (macroID))
                p->setValueNotifyingHost (p->convertTo0to1 (plainValue));
        };

        auto poolFor = [] (const String& ctype, String& prefix, int& size)
        {
            if      (ctype == "menu")   { prefix = "macro_select_"; size = kMaxSelectMacros; }
            else if (ctype == "slider") { prefix = "macro_knob_";   size = kMaxKnobMacros;   }
            else if (ctype == "button") { prefix = "macro_button_"; size = kMaxButtonMacros; }
            else                        { prefix = {};              size = 0;                }
        };

        // 0-based index of `default` within `definitions` (menu seed), or -1.
        // Mirrors the menu value extraction: token between first '[' and last ']'.
        auto menuIndexOfDefault = [] (const String& definitions, const String& def) -> int
        {
            if (definitions.isEmpty() || def.isEmpty())
                return -1;

            int index = 0;
            for (auto entry : StringArray::fromTokens (definitions, "|", ""))
            {
                entry = entry.trim();
                if (entry.isEmpty())
                    continue;
                const int i = entry.indexOfChar ('[');
                const int j = entry.lastIndexOfChar (']');
                const auto val = (i >= 0 && j > i) ? entry.substring (i + 1, j).trim() : entry;
                if (val == def)
                    return index;
                ++index;
            }
            return -1;
        };

        // `taken` = every macro id already bound in the live container.
        StringArray taken;
        std::function<void (const ValueTree&)> scanTaken = [&] (const ValueTree& node)
        {
            if (node.getType() == propControlType)
            {
                const auto p = node.getProperty (pParameter).toString();
                if (p.isNotEmpty())
                    taken.addIfNotAlreadyThere (p);
            }
            for (auto child : node)
                scanTaken (child);
        };
        scanTaken (container);

        // Rewrite + seed each opted-in PropertyControl in the bundle.
        std::function<void (ValueTree&)> allocate = [&] (ValueTree& node)
        {
            if (node.getType() == propControlType
                && node.getProperty (pParameter).toString().isNotEmpty())
            {
                String prefix; int poolSize = 0;
                poolFor (node.getProperty (pControlType).toString(), prefix, poolSize);

                String claimed;
                for (int k = 1; k <= poolSize; ++k)
                {
                    const auto cand = prefix + String (k);
                    if (taken.contains (cand, true))                continue;   // live or batch-claimed
                    if (magicState.getParameter (cand) == nullptr)  continue;   // patch lacks it
                    claimed = cand;
                    break;
                }

                if (claimed.isNotEmpty())
                {
                    node.setProperty (pParameter, claimed, nullptr);
                    taken.add (claimed);   // de-dupe the rest of this bundle

                    // Seed the live macro from `default` (raw domain).
                    const auto ctype = node.getProperty (pControlType).toString();
                    const auto def   = node.getProperty (pDefault).toString();
                    if (def.isNotEmpty())
                    {
                        if (ctype == "menu")
                        {
                            const int idx = menuIndexOfDefault (node.getProperty (pDefinitions).toString(), def);
                            if (idx >= 0)
                                seedMacro (claimed, (float) idx);
                        }
                        else if (ctype == "slider")
                        {
                            seedMacro (claimed, def.getFloatValue());
                        }
                        else if (ctype == "button")
                        {
                            seedMacro (claimed, def == "1" ? 1.0f : 0.0f);
                        }
                    }
                }
                else
                {
                    node.removeProperty (pParameter, nullptr);   // exhausted/missing → property-stored
                }
            }

            for (auto child : node)
                allocate (child);
        };

        for (auto& n : payload) allocate (n);
        for (auto& n : extras)  allocate (n);
    }

    // ── Step 6 — place the MidiTriggers into empty slots.
    auto slots = findEmptySlots (container, payload.size());
    if (slots.size() < payload.size())
        return;

    auto* um = builder ? &builder->getUndoManager() : nullptr;
    if (um) um->beginNewTransaction ("Add trigger");

    for (int i = 0; i < payload.size(); ++i)
    {
        remapNoteRanges (payload.getReference (i), delta);
        fillSlot (slots.getReference (i), payload.getReference (i), um);
    }

    // ── Step 7 — append extras. MidiEditor still de-dupes by midi-state-index
    // (no id). Range-follower types get their input range shifted by delta,
    // saturating at 0/127; a full 0..127 input range is the "no filter"
    // sentinel and stays untouched. The shift walks each extra's subtree so
    // followers nested inside a group-root View (e.g. Note_Repeat's LFO
    // tucked inside the user-facing panel) follow the transpose along with
    // their containing group.
    std::function<bool (const ValueTree&, int)> hasMidiEditorWithIndex =
        [&] (const ValueTree& tree, int index) -> bool
        {
            if (tree.getType() == midiEditorType
                && tree.hasProperty (midiStateIdxProp)
                && (int) tree.getProperty (midiStateIdxProp) == index)
                return true;

            for (auto child : tree)
                if (hasMidiEditorWithIndex (child, index))
                    return true;

            return false;
        };

    static const Identifier followerLow  ("input-low-note");
    static const Identifier followerHigh ("input-high-note");

    std::function<void (ValueTree&)> shiftFollowers = [&] (ValueTree& node)
    {
        if (kRangeFollowerTypes.contains (node.getType().toString()))
        {
            const bool hasLo = node.hasProperty (followerLow);
            const bool hasHi = node.hasProperty (followerHigh);
            const int  loV   = hasLo ? (int) node.getProperty (followerLow)  : 0;
            const int  hiV   = hasHi ? (int) node.getProperty (followerHigh) : 127;

            if (! (loV == 0 && hiV == 127))
            {
                if (hasLo) node.setProperty (followerLow,  jlimit (0, 127, loV + delta), nullptr);
                if (hasHi) node.setProperty (followerHigh, jlimit (0, 127, hiV + delta), nullptr);
            }
        }

        for (auto child : node)
            shiftFollowers (child);
    };

    for (auto& extra : extras)
    {
        if (extra.getType() == midiEditorType
            && extra.hasProperty (midiStateIdxProp)
            && hasMidiEditorWithIndex (container, (int) extra.getProperty (midiStateIdxProp)))
            continue;

        if (delta != 0)
            shiftFollowers (extra);

        container.appendChild (extra, um);
    }

    owner.repaint();
}

void NewMidiKeyboardComponent::TriggerEditor::autoColourSnippet (ValueTree& root)
{
    if (! root.isValid())
        return;

    static const Identifier midiTriggerType   ("MidiTrigger");
    static const Identifier idProp            ("id");
    static const Identifier triggerColourProp ("trigger-colour");
    static const String     backgroundID      ("Background");

    // ── Step 1 — gather payload MidiTriggers that already carry a trigger-colour.
    // A snippet can include invisible "switch" MidiTriggers stacked on top of
    // visible ones (e.g. Note Repeat's per-key enable switches sharing the
    // panel's id) — those have no trigger-colour set and must not gain one.
    // _multiCopy: top-level children; otherwise root is the sole node (the
    // same shape insertPayloadAtKey reads).
    Array<ValueTree> triggers;
    auto takeIfColoured = [&] (const ValueTree& n)
    {
        if (n.getType() == midiTriggerType && n.hasProperty (triggerColourProp))
            triggers.add (n);
    };

    if (root.getType().toString() == "_multiCopy")
    {
        for (auto child : root)
            takeIfColoured (child);
    }
    else
    {
        takeIfColoured (root);
    }

    if (triggers.isEmpty())
        return;

    // ── Step 2 — sort by lowest range note ascending, file-order descending on
    // ties. Sort index 0 is the anchor (gets the unmodified T); the descending
    // tiebreak means the later trigger wins when ranges coincide — "last in the
    // file" as agreed.
    auto lowestNoteOf = [] (const ValueTree& n)
    {
        int lowest = std::numeric_limits<int>::max();
        for (const auto& p : kTriggerNoteValuedProps)
        {
            const Identifier prop (p);
            if (n.hasProperty (prop))
                lowest = jmin (lowest, (int) n.getProperty (prop));
        }
        return lowest;
    };

    Array<std::pair<int, ValueTree>> sorted;
    for (int i = 0; i < triggers.size(); ++i)
        sorted.add ({ i, triggers[i] });

    std::sort (sorted.begin(), sorted.end(),
               [&] (const auto& a, const auto& b)
               {
                   const int la = lowestNoteOf (a.second);
                   const int lb = lowestNoteOf (b.second);
                   if (la != lb) return la < lb;
                   return a.first > b.first;
               });

    // ── Step 3 — collect "taken" colours from the container. Top-level
    // MidiTriggers, grouped by id (same convention insertPayloadAtKey uses):
    // one colour per id-group, id-less triggers stand alone. Empty / unset
    // trigger-colour contributes nothing.
    StringArray takenColours;
    if (auto container = getContainer(); container.isValid())
    {
        std::map<String, String> idToColour;
        for (auto child : container)
        {
            if (child.getType() != midiTriggerType)
                continue;

            const auto colour = child.getProperty (triggerColourProp).toString();
            if (colour.isEmpty())
                continue;

            const auto id = child.getProperty (idProp).toString();
            if (id.isNotEmpty())
            {
                if (idToColour.find (id) == idToColour.end())
                    idToColour[id] = colour;
            }
            else
            {
                takenColours.addIfNotAlreadyThere (colour);
            }
        }
        for (const auto& kv : idToColour)
            takenColours.addIfNotAlreadyThere (kv.second);
    }

    // ── Step 4 — pick a palette entry. Starting from owner.nextAutoColourIndex,
    // advance past entries whose final (post-brightness) colour is already in
    // use. Single wrap; if every colour is taken, settle on the start index.
    const int paletteSize = (int) kTriggerColours.size();
    const int startIndex  = ((owner.nextAutoColourIndex % paletteSize) + paletteSize) % paletteSize;
    int       chosen      = startIndex;

    for (int step = 0; step < paletteSize; ++step)
    {
        const int candidate = (startIndex + step) % paletteSize;
        const auto& tc      = kTriggerColours[(size_t) candidate];
        const auto  finalC  = tc.colour.withMultipliedBrightness (tc.brightness).toString();
        if (! takenColours.contains (finalC, true))
        {
            chosen = candidate;
            break;
        }
    }
    owner.nextAutoColourIndex = (chosen + 1) % paletteSize;

    // ── Step 5 — locate the Background (first match, skipping MidiTrigger
    // subtrees — a panel always lives outside them). Snippets carry at most
    // one Background; we wrap it in an array for the shared writer.
    Array<ValueTree> backgrounds;
    std::function<ValueTree (const ValueTree&)> findBackground = [&] (const ValueTree& node) -> ValueTree
    {
        if (node.getType() == midiTriggerType)
            return {};
        if (node.getProperty (idProp).toString() == backgroundID)
            return node;
        for (auto child : node)
            if (auto found = findBackground (child); found.isValid())
                return found;
        return {};
    };
    if (auto bg = findBackground (root); bg.isValid())
        backgrounds.add (bg);

    // Flatten the sorted-with-index pairs into a plain trigger array — the
    // writer just iterates by position, the original-index tiebreak is already
    // baked into the order.
    Array<ValueTree> sortedTriggers;
    sortedTriggers.ensureStorageAllocated (sorted.size());
    for (const auto& s : sorted)
        sortedTriggers.add (s.second);

    // Hand off to the shared writer — off-tree (nullptr UM); the live insert
    // happens later in insertPayloadAtKey. Preserves the authored 3-stop
    // background gradient pattern. Same call shape setColourForKey uses.
    const auto& tc = kTriggerColours[(size_t) chosen];
    writeAutoColourPalette (sortedTriggers,
                            backgrounds,
                            tc.colour.withMultipliedBrightness (tc.brightness),
                            nullptr);
}

void NewMidiKeyboardComponent::TriggerEditor::pasteToKey (int note)
{
    insertPayloadAtKey (ValueTree::fromXml (SystemClipboard::getTextFromClipboard()), note);
}

void NewMidiKeyboardComponent::TriggerEditor::applyPresetFile (int note, const File& file)
{
    auto root = ValueTree::fromXml (file.loadFileAsString());
    autoColourSnippet (root);
    insertPayloadAtKey (root, note);
}

void NewMidiKeyboardComponent::TriggerEditor::saveKeyAs (int note)
{
    // Snapshot the triggers covering this key, then expand by group id — same
    // sweep copyKeys / doClear use — so multi-trigger groups (e.g. Note_Repeat's
    // SPEED + REPEAT under one id, paired with a View carrying the LFO and
    // controls) get saved whole. MidiTrigger siblings use propertiesOnlyCopy
    // (slot index dropped, destination supplies its own on load); non-trigger
    // siblings use createCopy so View / nested LFO content survives. Anything
    // in the bank that isn't part of this group is left out — the curated-bank
    // assumption is gone; the group id is the bundling boundary now.
    auto nodes = findNodesForKey (note);
    if (nodes.isEmpty())
        return;

    static const Identifier midiTriggerType ("MidiTrigger");

    StringArray groupIDs;
    for (const auto& n : nodes)
    {
        const auto id = n.getProperty ("id").toString();
        if (id.isNotEmpty())
            groupIDs.addIfNotAlreadyThere (id);
    }

    Array<ValueTree> triggers, extras;
    for (auto& n : nodes)
        triggers.add (propertiesOnlyCopy (n));

    if (! groupIDs.isEmpty())
        if (auto container = getContainer(); container.isValid())
            for (auto child : container)
            {
                if (nodes.contains (child))
                    continue;   // already captured above

                const auto childID = child.getProperty ("id").toString();
                if (childID.isEmpty() || ! groupIDs.contains (childID, true))
                    continue;

                if (child.getType() == midiTriggerType)
                    triggers.add (propertiesOnlyCopy (child));
                else
                    extras.add (child.createCopy());
            }

    auto folder = presetFolder;
    if (! folder.isDirectory())
        folder.createDirectory();

    String defaultName = "trigger";
    if (triggers.getReference (0).hasProperty ("id"))
        defaultName = triggers.getReference (0).getProperty ("id").toString();
    else if (triggers.getReference (0).hasProperty ("trigger-label"))
        defaultName = triggers.getReference (0).getProperty ("trigger-label").toString();

    auto suggestedFile = folder.getChildFile (defaultName).withFileExtension (".xml");

    // System file chooser — pattern lifted from
    // SubPresetManager::savePresetAsMenuButton. The chooser must outlive
    // its async dialog, so it's held as a TriggerEditor member. The lambda
    // captures only the snapshot + the chooser result, so the keyboard
    // going away mid-dialog is safe: destroying the chooser tears down the
    // lambda without firing, and no live keyboard state is touched.
    fileChooser = std::make_unique<FileChooser> (
        "Save Trigger As", suggestedFile, "*.xml",
        true,     // useOSNativeDialogBox
        false,    // treatFilePackagesAsDirectories
        &owner);

    fileChooser->launchAsync (FileBrowserComponent::saveMode
                              | FileBrowserComponent::canSelectFiles
                              | FileBrowserComponent::warnAboutOverwriting,
                              [triggers, extras] (const FileChooser& chooser)
    {
        auto outFile = chooser.getResult();
        if (outFile == File())
            return;                            // user cancelled

        if (! outFile.hasFileExtension (".xml"))
            outFile = outFile.withFileExtension (".xml");

        // Bare node only for a single trigger with nothing tagging along;
        // otherwise _multiCopy. Triggers come first so the file reads
        // naturally (slot pool up top, support nodes after) and matches the
        // authored order in shipped snippets.
        ValueTree out;
        const int total = triggers.size() + extras.size();
        if (total == 1)
        {
            out = triggers.getFirst();
        }
        else
        {
            out = ValueTree ("_multiCopy");
            for (auto& n : triggers) out.appendChild (n, nullptr);
            for (auto& n : extras)   out.appendChild (n, nullptr);
        }

        // replaceWithText writes via a TemporaryFile + atomic rename — both
        // truncates an existing file (createOutputStream defaults to append)
        // and protects against partial writes corrupting the original.
        outFile.replaceWithText (out.toXmlString());
    });
}

void NewMidiKeyboardComponent::TriggerEditor::clearKey (int note)
{
    if (builder == nullptr) return;

        DBG ("[" << juce::Time::getMillisecondCounterHiRes() << "] keyboard: setValue(true) BEFORE");
        builder->getMagicState().getPropertyAsValue ("flag:tree_edit_in_progress").setValue (true);
        DBG ("[" << juce::Time::getMillisecondCounterHiRes() << "] keyboard: setValue(true) AFTER");

        auto safe = juce::Component::SafePointer<NewMidiKeyboardComponent> (&owner);

        juce::Timer::callAfterDelay (10, [this, safe, note]
        {
            if (safe.getComponent() != nullptr)
            {
                DBG ("[" << juce::Time::getMillisecondCounterHiRes() << "] keyboard: doClear BEFORE");
                doClear (note);
                DBG ("[" << juce::Time::getMillisecondCounterHiRes() << "] keyboard: doClear AFTER");
            }
        });

        juce::Timer::callAfterDelay (1000, [this, safe]
        {
            if (safe.getComponent() != nullptr && builder != nullptr)
            {
                DBG ("[" << juce::Time::getMillisecondCounterHiRes() << "] keyboard: setValue(false) BEFORE");
                builder->getMagicState().getPropertyAsValue ("flag:tree_edit_in_progress").setValue (false);
                DBG ("[" << juce::Time::getMillisecondCounterHiRes() << "] keyboard: setValue(false) AFTER");
            }
        });
}

void NewMidiKeyboardComponent::TriggerEditor::doClear (int note)
{
    auto container = getContainer();
    if (! container.isValid())
        return;

    auto nodes = findNodesForKey (note);
    if (nodes.isEmpty())
        return;

    auto* um = builder ? &builder->getUndoManager() : nullptr;
    if (um) um->beginNewTransaction ("Clear key");

    // Group-by-id: nodes sharing an id form a logical bundle (e.g. a MidiTrigger
    // paired with a ListBox shown in the macro panel). Capture ids first —
    // clearPropertiesOfNode strips the id property too, so we need them now if
    // we're to find the linked siblings afterwards.
    StringArray groupIDs;
    for (const auto& node : nodes)
    {
        const auto id = node.getProperty ("id").toString();
        if (id.isNotEmpty())
            groupIDs.addIfNotAlreadyThere (id);
    }

    // Clear, don't delete — mirrors clearSelected. The slot stays alive with
    // its trigger-index; only its contents go.
    for (auto& node : nodes)
        clearPropertiesOfNode (node, um);

    // Sweep the rest of the bank for siblings sharing one of those ids and
    // dispose of them too. MidiTriggers stay in the slot pool (cleared in
    // place — the same handling we'd want for a multi-row group); everything
    // else is removed outright, mirroring clearAll's two-tier disposal. Build
    // the lists first — removeChild shifts sibling indices mid-iteration.
    if (! groupIDs.isEmpty())
    {
        static const Identifier midiTriggerType ("MidiTrigger");

        Array<ValueTree> linkedTriggers, linkedExtras;
        for (auto child : container)
        {
            if (nodes.contains (child))
                continue;   // already handled above (its id is gone now anyway)

            const auto childID = child.getProperty ("id").toString();
            if (childID.isEmpty() || ! groupIDs.contains (childID, true))
                continue;

            if (child.getType() == midiTriggerType) linkedTriggers.add (child);
            else                                    linkedExtras.add (child);
        }

        for (auto& n : linkedTriggers)
            clearPropertiesOfNode (n, um);

        for (auto& n : linkedExtras)
            if (n.isValid() && n.getParent() == container)
                container.removeChild (n, um);

        if (! linkedExtras.isEmpty() && builder != nullptr)
            toybox::scheduleFlushUnusedMidiObjects (builder->getMagicState());
    }

    // [global] companions — remove "<stem> [global]" nodes whose stem has no
    // surviving instance left in the container. The cleared groupIDs each
    // yield a stem (strip a trailing " (n)" suffix). For each, scan the
    // container for any node whose id matches the stem or "stem (n)"; if
    // none remain, the companion goes with it. Mirrors the paste rule:
    // [global] outlives its consumers, but only as long as any exists.
    if (! groupIDs.isEmpty())
    {
        static const Identifier midiTriggerType ("MidiTrigger");

        StringArray stems;
        for (const auto& id : groupIDs)
        {
            if (isGlobalID (id))
                continue;   // a [global] itself never anchors a stem
            stems.addIfNotAlreadyThere (stripGroupSuffix (id));
        }

        for (const auto& stem : stems)
        {
            bool anyInstanceLeft = false;
            for (auto child : container)
            {
                const auto cid = child.getProperty ("id").toString();
                if (cid.isEmpty() || isGlobalID (cid)) continue;
                if (stripGroupSuffix (cid) == stem)
                {
                    anyInstanceLeft = true;
                    break;
                }
            }

            if (anyInstanceLeft)
                continue;

            const auto globalID = stem + " [global]";
            Array<ValueTree> globalsToRemove;
            for (auto child : container)
                if (child.getProperty ("id").toString().equalsIgnoreCase (globalID))
                    globalsToRemove.add (child);

            for (auto& g : globalsToRemove)
            {
                if (g.getType() == midiTriggerType)
                    clearPropertiesOfNode (g, um);
                else if (g.isValid() && g.getParent() == container)
                    container.removeChild (g, um);
            }

            if (! globalsToRemove.isEmpty() && builder != nullptr)
                toybox::scheduleFlushUnusedMidiObjects (builder->getMagicState());
        }
    }

    owner.repaint();
}

void NewMidiKeyboardComponent::TriggerEditor::clearAll()
{
    auto container = getContainer();
    if (! container.isValid())
        return;

    static const Identifier midiTriggerType ("MidiTrigger");

    auto* um = builder ? &builder->getUndoManager() : nullptr;
    if (um) um->beginNewTransaction ("Clear all triggers");

    // Mirrors clearAllTriggers: MidiTrigger slots keep their slot (index
    // preserved, contents cleared); other node types are removed entirely.
    // Snapshot first — removing extras shifts child indices mid-iteration.
    Array<ValueTree> children;
    for (auto child : container)
        children.add (child);

    for (auto& node : children)
    {
        if (node.getType() == midiTriggerType)
            clearPropertiesOfNode (node, um);
        else if (node.isValid() && node.getParent() == container)
            container.removeChild (node, um);
    }
    
    toybox::scheduleFlushUnusedMidiObjects (builder->getMagicState());

    owner.repaint();
}

void NewMidiKeyboardComponent::TriggerEditor::setColourForKey (int note, Colour colour)
{
    auto container = getContainer();
    if (! container.isValid())
        return;

    auto clickedNodes = findNodesForKey (note);
    if (clickedNodes.isEmpty())
        return;

    static const Identifier midiTriggerType ("MidiTrigger");
    static const Identifier idProp          ("id");
    static const String     backgroundID    ("Background");

    auto* um = builder ? &builder->getUndoManager() : nullptr;
    if (um) um->beginNewTransaction ("Set trigger colour");

    // Sort key inside a group: lowest range note ascending, ties broken by
    // descending file order. Same rule autoColourSnippet uses so a manually
    // coloured group ramps identically to an auto-coloured snippet.
    auto lowestNoteOf = [] (const ValueTree& n)
    {
        int lowest = std::numeric_limits<int>::max();
        for (const auto& p : kTriggerNoteValuedProps)
        {
            const Identifier prop (p);
            if (n.hasProperty (prop))
                lowest = jmin (lowest, (int) n.getProperty (prop));
        }
        return lowest;
    };

    // Background search inside a top-level View container. Defensive on the
    // MidiTrigger skip — at root the View isn't a MidiTrigger, but a Background
    // could be nested arbitrarily deep, so we recurse with the same skip rule
    // autoColourSnippet uses.
    std::function<void (const ValueTree&, Array<ValueTree>&)> collectBackgrounds =
        [&] (const ValueTree& node, Array<ValueTree>& out)
    {
        if (node.getType() == midiTriggerType)
            return;
        if (node.getProperty (idProp).toString() == backgroundID)
            out.add (node);
        for (auto child : node)
            collectBackgrounds (child, out);
    };

    // Partition the clicked nodes into id-groups (and id-less loners). Each
    // id-group is then expanded by sweeping the root: top-level MidiTriggers
    // sharing the id join the trigger set; top-level non-MidiTriggers sharing
    // the id are walked for Background descendants. Id-less clicked nodes
    // colour solo (anchor T only, no spread, no background).
    StringArray      groupIDs;
    Array<ValueTree> idlessTriggers;

    for (const auto& n : clickedNodes)
    {
        const auto id = n.getProperty (idProp).toString();
        if (id.isEmpty())
            idlessTriggers.add (n);
        else
            groupIDs.addIfNotAlreadyThere (id);
    }

    struct GroupBuffers { Array<ValueTree> triggers, backgrounds; };
    std::vector<GroupBuffers> groups (groupIDs.size());

    // Single root sweep — file order is preserved, which is the input the
    // tie-break below relies on.
    for (auto child : container)
    {
        const auto childID = child.getProperty (idProp).toString();
        if (childID.isEmpty())
            continue;
        const int idx = groupIDs.indexOf (childID);
        if (idx < 0)
            continue;

        if (child.getType() == midiTriggerType)
            groups[(size_t) idx].triggers.add (child);
        else
            collectBackgrounds (child, groups[(size_t) idx].backgrounds);
    }

    // Sort each group's triggers (lowest-note ascending; ties broken by
    // descending file order — stash the file-order index first so the sort is
    // stable against std::sort's reordering).
    for (auto& g : groups)
    {
        Array<std::pair<int, ValueTree>> indexed;
        indexed.ensureStorageAllocated (g.triggers.size());
        for (int i = 0; i < g.triggers.size(); ++i)
            indexed.add ({ i, g.triggers.getReference (i) });

        std::sort (indexed.begin(), indexed.end(),
                   [&] (const auto& a, const auto& b)
                   {
                       const int la = lowestNoteOf (a.second);
                       const int lb = lowestNoteOf (b.second);
                       if (la != lb) return la < lb;
                       return a.first > b.first;
                   });

        g.triggers.clearQuick();
        for (const auto& pair : indexed)
            g.triggers.add (pair.second);
    }

    // Write each id-group through the shared writer, then each id-less loner.
    // Same call shape autoColourSnippet uses — visually identical results.
    for (const auto& g : groups)
        writeAutoColourPalette (g.triggers, g.backgrounds, colour, um);

    if (! idlessTriggers.isEmpty())
    {
        const Array<ValueTree> noBackgrounds;
        for (auto& solo : idlessTriggers)
        {
            Array<ValueTree> one;
            one.add (solo);
            writeAutoColourPalette (one, noBackgrounds, colour, um);
        }
    }

    owner.repaint();
}

bool NewMidiKeyboardComponent::TriggerEditor::beginEdgeResize (int edgeNote, bool lowEdge)
{
    resizeActive = false;
    resizeBounds.clear();

    if (! getContainer().isValid())
        return false;

    auto nodes = findNodesForKey (edgeNote);   // entire stack on this edge
    if (nodes.isEmpty())
        return false;

    int minLowVal  = 128;   // smallest low across all pairs   (low-edge clamp)
    int maxHighVal = -1;    // largest high across all pairs    (high-edge clamp)
    int minGap     = 128;   // narrowest range (hi - lo)        (both clamps)

    for (auto& n : nodes)
    {
        for (const auto& pair : kTriggerRangePairs)
        {
            const Identifier lo (pair.lo), hi (pair.hi);
            if (! (n.hasProperty (lo) && n.hasProperty (hi)))
                continue;

            const int loV = (int) n.getProperty (lo);
            const int hiV = (int) n.getProperty (hi);
            minGap = jmin (minGap, hiV - loV);

            if (lowEdge)
            {
                resizeBounds.push_back ({ n, lo, loV });
                minLowVal = jmin (minLowVal, loV);
            }
            else
            {
                resizeBounds.push_back ({ n, hi, hiV });
                maxHighVal = jmax (maxHighVal, hiV);
            }
        }
    }

    if (resizeBounds.empty())
        return false;

    // Grouped followers — kRangeFollowerTypes nodes (Mapper, Arpeggiator)
    // sharing an id with any node in the stack (the same id-grouping
    // convention doClear uses) track the dragged
    // edge via their input-low/high-note filter. They follow the trigger's
    // delta but never constrain it: a default 0..127 follower would otherwise
    // pin the drag entirely. Instead each follower bound saturates against
    // its own opposite edge and the 0/127 hard limits (per-bound minV/maxV,
    // applied in updateEdgeResize). Missing props are captured at their
    // documented defaults (0 / 127) so an unauthored follower still follows —
    // the drag writes the explicit value.
    {
        static const Identifier followerLow  ("input-low-note");
        static const Identifier followerHigh ("input-high-note");

        StringArray groupIDs;
        for (const auto& n : nodes)
        {
            const auto id = n.getProperty ("id").toString();
            if (id.isNotEmpty())
                groupIDs.addIfNotAlreadyThere (id);
        }

        if (! groupIDs.isEmpty())
        {
            // Walk every group-root subtree (top-level container children whose
            // id is in groupIDs) and capture any range-follower descendant.
            // Views are PGM's only nestable type, so non-View nodes terminate
            // the walk on their own. A nested follower's own id is irrelevant
            // — group membership is inherited from the View root — which is
            // what lets a Note_Repeat-style snippet hide its LFO inside the
            // user-facing View while still tracking the trigger edge.
            std::function<void (const ValueTree&)> visit = [&] (const ValueTree& node)
            {
                if (kRangeFollowerTypes.contains (node.getType().toString()) && ! nodes.contains (node))
                {
                    const int loV = node.hasProperty (followerLow)  ? (int) node.getProperty (followerLow)  : 0;
                    const int hiV = node.hasProperty (followerHigh) ? (int) node.getProperty (followerHigh) : 127;

                    if (lowEdge)
                        resizeBounds.push_back ({ node, followerLow,  loV, 0,   hiV });  // can't rise past its high
                    else
                        resizeBounds.push_back ({ node, followerHigh, hiV, loV, 127 }); // can't fall past its low
                }

                for (auto child : node)
                    visit (child);
            };

            for (auto child : getContainer())
            {
                const auto childID = child.getProperty ("id").toString();
                if (childID.isNotEmpty() && groupIDs.contains (childID, true))
                    visit (child);
            }
        }
    }

    // Find the nearest note on the extending side that belongs to a *different*
    // region (a node not in this stack) so a drag can't push this region into
    // its neighbour. Nodes stacked on the same keys are all in `nodes` and move
    // together, so they never block each other. Shrinking can't cause an overlap,
    // so that direction is bounded only by the one-note minimum width (minGap).
    auto blockedByOther = [this, &nodes] (int note)
    {
        auto container = getContainer();
        for (auto child : container)
            if (! nodes.contains (child) && nodeCoversKey (child, note))
                return true;
        return false;
    };

    if (lowEdge)
    {
        int floorNote = 0;                       // hard floor at note 0
        for (int n = minLowVal - 1; n >= 0; --n)
            if (blockedByOther (n)) { floorNote = n + 1; break; }

        resizeMinDelta = floorNote - minLowVal;  // extend left only into free space
        resizeMaxDelta = minGap;                 // shrink from left, keep >= 1 note wide
    }
    else
    {
        int ceilNote = 127;                      // hard ceiling at note 127
        for (int n = maxHighVal + 1; n <= 127; ++n)
            if (blockedByOther (n)) { ceilNote = n - 1; break; }

        resizeMinDelta = -minGap;                // shrink from right, keep >= 1 note wide
        resizeMaxDelta = ceilNote - maxHighVal;  // extend right only into free space
    }

    if (auto* um = builder ? &builder->getUndoManager() : nullptr)
        um->beginNewTransaction (lowEdge ? "Resize trigger (lower note)"
                                         : "Resize trigger (upper note)");

    resizeActive = true;
    return true;
}

bool NewMidiKeyboardComponent::TriggerEditor::beginRegionDrag (int anyNoteInRegion)
{
    // Same shape as beginEdgeResize, but captures BOTH edges of every range
    // pair so one delta translates the whole region. updateEdgeResize and
    // endEdgeResize are reused unchanged.
    resizeActive = false;
    resizeBounds.clear();

    if (! getContainer().isValid())
        return false;

    auto nodes = findNodesForKey (anyNoteInRegion);
    if (nodes.isEmpty())
        return false;

    int minLowVal  = 128;
    int maxHighVal = -1;

    for (auto& n : nodes)
    {
        for (const auto& pair : kTriggerRangePairs)
        {
            const Identifier lo (pair.lo), hi (pair.hi);
            if (! (n.hasProperty (lo) && n.hasProperty (hi)))
                continue;

            const int loV = (int) n.getProperty (lo);
            const int hiV = (int) n.getProperty (hi);

            resizeBounds.push_back ({ n, lo, loV });
            resizeBounds.push_back ({ n, hi, hiV });

            minLowVal  = jmin (minLowVal,  loV);
            maxHighVal = jmax (maxHighVal, hiV);
        }
    }

    if (resizeBounds.empty())
        return false;

    // Grouped followers — same set as beginEdgeResize. Capture both edges;
    // each saturates individually at 0/127 (default per-bound clamp), matching
    // edge-drag semantics.
    {
        static const Identifier followerLow  ("input-low-note");
        static const Identifier followerHigh ("input-high-note");

        StringArray groupIDs;
        for (const auto& n : nodes)
        {
            const auto id = n.getProperty ("id").toString();
            if (id.isNotEmpty())
                groupIDs.addIfNotAlreadyThere (id);
        }

        if (! groupIDs.isEmpty())
        {
            std::function<void (const ValueTree&)> visit = [&] (const ValueTree& node)
            {
                if (kRangeFollowerTypes.contains (node.getType().toString()) && ! nodes.contains (node))
                {
                    const int loV = node.hasProperty (followerLow)  ? (int) node.getProperty (followerLow)  : 0;
                    const int hiV = node.hasProperty (followerHigh) ? (int) node.getProperty (followerHigh) : 127;

                    resizeBounds.push_back ({ node, followerLow,  loV });
                    resizeBounds.push_back ({ node, followerHigh, hiV });
                }

                for (auto child : node)
                    visit (child);
            };

            for (auto child : getContainer())
            {
                const auto childID = child.getProperty ("id").toString();
                if (childID.isNotEmpty() && groupIDs.contains (childID, true))
                    visit (child);
            }
        }
    }

    // Same blockedByOther scan as beginEdgeResize, run for both directions —
    // the region can't push into a neighbour on either side.
    auto blockedByOther = [this, &nodes] (int note)
    {
        auto container = getContainer();
        for (auto child : container)
            if (! nodes.contains (child) && nodeCoversKey (child, note))
                return true;
        return false;
    };

    int floorNote = 0;
    for (int n = minLowVal - 1; n >= 0; --n)
        if (blockedByOther (n)) { floorNote = n + 1; break; }

    int ceilNote = 127;
    for (int n = maxHighVal + 1; n <= 127; ++n)
        if (blockedByOther (n)) { ceilNote = n - 1; break; }

    resizeMinDelta = floorNote - minLowVal;
    resizeMaxDelta = ceilNote  - maxHighVal;

    if (auto* um = builder ? &builder->getUndoManager() : nullptr)
        um->beginNewTransaction ("Move trigger region");

    resizeActive = true;
    return true;
}

void NewMidiKeyboardComponent::TriggerEditor::updateEdgeResize (int delta)
{
    if (! resizeActive)
        return;

    const int d  = jlimit (resizeMinDelta, resizeMaxDelta, delta);
    auto*     um = builder ? &builder->getUndoManager() : nullptr;

    for (auto& b : resizeBounds)
        b.node.setProperty (b.prop, jlimit (b.minV, b.maxV, b.orig + d), um);   // absolute from captured originals; followers saturate

    owner.repaint();
}

void NewMidiKeyboardComponent::TriggerEditor::endEdgeResize()
{
    resizeActive = false;
    resizeBounds.clear();
}

void NewMidiKeyboardComponent::TriggerEditor::buildPresetMenu (PopupMenu& menu, const File& folder,
                                                               std::vector<File>& files, int baseId)
{
    if (! folder.isDirectory())
        return;

    Array<File> items;
    for (const auto& entry : RangedDirectoryIterator (folder, false, "*", File::findFilesAndDirectories))
    {
        auto f = entry.getFile();
        if (f.isDirectory() || f.hasFileExtension (".xml"))
            items.add (f);
    }

    struct Cmp
    {
        int compareElements (const File& a, const File& b) const
        {
            if (a.isDirectory() && ! b.isDirectory()) return -1;
            if (! a.isDirectory() && b.isDirectory()) return 1;
            return a.getFileName().compareIgnoreCase (b.getFileName());
        }
    } cmp;
    items.sort (cmp);

    for (auto& f : items)
    {
        if (f.isDirectory())
        {
            PopupMenu sub;
            buildPresetMenu (sub, f, files, baseId);
            menu.addSubMenu (f.getFileName(), sub);
        }
        else
        {
            menu.addItem (baseId + (int) files.size(), f.getFileNameWithoutExtension());
            files.push_back (f);
        }
    }
}

void NewMidiKeyboardComponent::TriggerEditor::showMenuForKey (int note, Point<int> screenPos)
{
    owner.setActiveEditNote (note);

    const auto nodesHere = findNodesForKey (note);
    const bool hasNodes  = ! nodesHere.isEmpty();
    const bool hasClip   = ValueTree::fromXml (SystemClipboard::getTextFromClipboard()).isValid();

    PopupMenu menu;
    menu.addItem (miCut,   "Cut",   hasNodes);
    menu.addItem (miCopy,  "Copy",  hasNodes);
    menu.addItem (miPaste, "Paste", hasClip);

    menu.addSeparator();
    menu.addItem (miClear, "Clear", hasNodes);
    menu.addSeparator();

    // Easter egg: holding Alt/Option as the menu opens reveals Save As...,
    // which writes the key's stack to the Triggers folder using the same
    // serialisation as Copy (properties-only triggers, _multiCopy when there's
    // more than one node). Round-trips through insertPayloadAtKey unchanged.
    const bool altHeld = ModifierKeys::getCurrentModifiers().isAltDown();
    if (altHeld && hasNodes)
    {
        menu.addItem (miSaveAs, "Save As...");
        menu.addSeparator();
    }

    // Add Trigger — insert a preset node/stack from disk onto this key.
    std::vector<File> presetFiles;
    PopupMenu addTriggerMenu;
    buildPresetMenu (addTriggerMenu, presetFolder, presetFiles, miPresetBase);
    menu.addSubMenu ("Add Trigger", addTriggerMenu, presetFolder.isDirectory());

    // Colour — recolour the node(s) on this key. (The assigned L&F overrides the
    // per-item text colour, so the names render in the normal menu colour; the
    // colour actually applied to the keyboard is halved in brightness in the
    // handler below.)
    PopupMenu colourMenu;
    for (int i = 0; i < (int) kTriggerColours.size(); ++i)
        colourMenu.addColouredItem (miColourBase + i,
                                    kTriggerColours[(size_t) i].name,
                                    kTriggerColours[(size_t) i].colour);
    menu.addSubMenu ("Colour", colourMenu, hasNodes);

    menu.addSeparator();
    menu.addItem (miClearAll, "Clear All");

    // Theme the menu (submenus inherit it) with the look & feel assigned to
    // the keyboard item in the PGM editor — that L&F lives on our parent
    // component, so we read it from there. Set before showing.
    if (auto* parent = owner.getParentComponent())
        menu.setLookAndFeel (&parent->getLookAndFeel());

    // Item height — mirror the L&Fs' getOptionsForComboBoxPopupMenu: when
    // MENU_HEIGHT is defined the menus use it as the item height. That hook only
    // fires for ComboBox popups, so a standalone menu sets the same height
    // itself. Without the define, leave it to the L&F's font-based default
    // (with an iOS tappability floor).
    auto opts = PopupMenu::Options().withTargetScreenArea ({ screenPos.x, screenPos.y, 1, 1 });
   #if defined (MENU_HEIGHT)
    opts = opts.withStandardItemHeight (MENU_HEIGHT);
   #elif JUCE_IOS
    opts = opts.withStandardItemHeight (36);
   #endif

    // Single guarded callback. The keyboard owns this editor, so a live owner
    // implies a live `this`; the SafePointer check protects both.
    Component::SafePointer<NewMidiKeyboardComponent> safe (&owner);
    menu.showMenuAsync (opts, [this, safe, note, nodesHere, presetFiles] (int result)
    {
        auto* kb = safe.getComponent();
        if (kb == nullptr)
            return;

        if      (result == miCopy)     copyKeys (nodesHere);
        else if (result == miCut)    { copyKeys (nodesHere); clearKey (note); }
        else if (result == miPaste)    pasteToKey (note);
        else if (result == miClear)    clearKey (note);
        else if (result == miClearAll) clearAll();
        else if (result == miSaveAs)   saveKeyAs (note);
        else if (result >= miColourBase && result < miColourBase + (int) kTriggerColours.size())
        {
            const auto& tc = kTriggerColours[(size_t) (result - miColourBase)];
            setColourForKey (note, tc.colour.withMultipliedBrightness (tc.brightness));
        }
        else if (result >= miPresetBase && (size_t) (result - miPresetBase) < presetFiles.size())
            applyPresetFile (note, presetFiles[(size_t) (result - miPresetBase)]);

        kb->setActiveEditNote (-1);
    });
}

} // namespace juce
