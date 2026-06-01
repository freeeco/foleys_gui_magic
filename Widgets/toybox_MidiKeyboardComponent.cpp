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

    // 20 distinguishable colours offered in the key menu's Colour submenu.
    struct TriggerNamedColour { String name; Colour colour; };

    const std::vector<TriggerNamedColour> kTriggerColours
    {
        { "Red",        Colour (0xffe53935) }, { "Orange",     Colour (0xfffb8c00) },
        { "Amber",      Colour (0xffffb300) }, { "Yellow",     Colour (0xfffdd835) },
        { "Lime",       Colour (0xffc0ca33) }, { "Green",      Colour (0xff43a047) },
        { "Teal",       Colour (0xff00897b) }, { "Cyan",       Colour (0xff00acc1) },
        { "Light Blue", Colour (0xff039be5) }, { "Blue",       Colour (0xff1e88e5) },
        { "Indigo",     Colour (0xff3949ab) }, { "Violet",     Colour (0xff8e24aa) },
        { "Magenta",    Colour (0xffd81b60) }, { "Pink",       Colour (0xffec407a) },
        { "Rose",       Colour (0xfff06292) }, { "Brown",      Colour (0xff6d4c41) },
        { "Slate",      Colour (0xff546e7a) }, { "Grey",       Colour (0xff9e9e9e) },
        { "Mint",       Colour (0xff66bb6a) }, { "Lavender",   Colour (0xff9575cd) },
    };

    // Menu result-id ranges.
    enum
    {
        miCopy = 1, miCut, miPaste, miClear, miClearAll,
        miColourBase  = 100,    // + index into kTriggerColours
        miPresetBase  = 1000    // + index into the captured preset-files vector
    };
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
    g.fillAll (juce::Colours::darkgrey);

    KeyboardComponentBase::paint (g);
}

void NewMidiKeyboardComponent::repaintNote (int noteNum)
{
    if (getRangeStart() <= noteNum && noteNum <= getRangeEnd())
        repaint (getRectangleForKey (noteNum).getSmallestIntegerContainer());
}


void NewMidiKeyboardComponent::mouseMove (const MouseEvent& e)
{
    updateNoteUnderMouse (e, false);
}

void NewMidiKeyboardComponent::mouseDrag (const MouseEvent& e)
{
    auto newNote = getNoteAndVelocityAtPosition (e.position).note;

    if (newNote >= 0 && mouseDraggedToKey (newNote, e))
        updateNoteUnderMouse (e, true);
}

void NewMidiKeyboardComponent::mouseDown (const MouseEvent& e)
{
    auto newNote = getNoteAndVelocityAtPosition (e.position).note;

    if (newNote >= 0 && mouseDownOnKey (newNote, e))
        updateNoteUnderMouse (e, true);
}

void NewMidiKeyboardComponent::mouseUp (const MouseEvent& e)
{
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
}

void NewMidiKeyboardComponent::timerCallback()
{
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

    // 1) Fill with the white note base colour (overridden by trigger colour if set)
    auto custom = noteColourProvider ? noteColourProvider (midiNoteNumber) : std::nullopt;
    g.setColour (custom.value_or (findColour (whiteNoteColourId)));
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
            // Darken the custom colour directly — keeps the hue, just deeper
            auto c = isDown ? custom->darker (0.65f) : custom->darker (0.15f);
            g.setColour (c);
        }
        else
        {
            auto c = Colours::transparentWhite;
            if (isDown)       c = findColour (keyDownOverlayColourId);
            else if (isOver)  c = findColour (mouseOverKeyOverlayColourId);
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

    // 8) Edit-mode outline (drawn per-key so the black keys, painted after the
    //    white keys, mask the white keys' boundary verticals — keeping the
    //    white↔black borders clean).
    drawEditOutline (g, area, midiNoteNumber);
}

void NewMidiKeyboardComponent::drawBlackNote (int midiNoteNumber, Graphics& g, Rectangle<float> area,
                                           bool isDown, bool isOver, Colour noteFillColour)
{
    auto custom = noteColourProvider ? noteColourProvider (midiNoteNumber) : std::nullopt;
    auto c = custom.value_or (noteFillColour);

    if (custom)
    {
        if (isDown)      c = c.darker (0.9f);
        else if (isOver) c = c.darker (0.15f);
    }
    else
    {
        if (isDown)      c = c.brighter (0.30f);
        if (isOver)      c = c.overlaidWith (findColour (mouseOverKeyOverlayColourId));
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

    // 9) Edit-mode outline (black keys paint on top of the white keys, so this
    //    sits in front and forms the front face of the boundary verticals).
    drawEditOutline (g, area, midiNoteNumber);
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
    if (editMode)
    {
        // In edit mode a click opens the trigger menu instead of playing.
        triggerEditor.showMenuForKey (midiNoteNumber, e.getScreenPosition());
        return false;
    }

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
        setActiveEditNote (-1);

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

void NewMidiKeyboardComponent::drawEditOutline (Graphics& g, Rectangle<float> area, int midiNoteNumber)
{
    if (! editMode || ! noteColourProvider)
        return;

    const auto here = noteColourProvider (midiNoteNumber);
    if (! here.has_value())
        return;

    // Colour of a note, or no value if it's outside the drawn range (which we
    // treat as a boundary so zones close at the keyboard edges).
    auto colourAt = [&] (int n) -> std::optional<Colour>
    {
        if (n < getRangeStart() || n > getRangeEnd())
            return {};
        return noteColourProvider (n);
    };
    auto differs = [&] (int a, int b) { return colourAt (a) != colourAt (b); };

    const bool active  = (activeRunStart >= 0
                          && midiNoteNumber >= activeRunStart
                          && midiNoteNumber <= activeRunEnd);
    const bool isBlack = MidiMessage::isMidiNoteBlack (midiNoteNumber);

    const float t = active ? 3.0f : 1.5f;
    g.setColour (findColour (editOutlineColourId));

    if (getOrientation() == horizontalKeyboard)
    {
        g.fillRect (area.getX(), area.getY(), area.getWidth(), t);                  // top rim

        if (! isBlack)
        {
            // White key: reaches the keyboard bottom, so its bottom is always a
            // rim. Its left/right edges are the *lower* seams — boundaries with
            // the adjacent WHITE keys (skipping any black key between, e.g. C↔D
            // across C#). Each seam is drawn once: we always draw our LEFT edge
            // at a boundary, and the right neighbour — if it's a coloured key —
            // draws the shared seam as its own left edge. So we draw our RIGHT
            // edge only when that neighbour won't: when it has no colour (a
            // non-trigger key) or is off the end of the range.
            g.fillRect (area.getX(), area.getBottom() - t, area.getWidth(), t);     // bottom rim

            const int prevWhite = MidiMessage::isMidiNoteBlack (midiNoteNumber - 1) ? midiNoteNumber - 2
                                                                                    : midiNoteNumber - 1;
            const int nextWhite = MidiMessage::isMidiNoteBlack (midiNoteNumber + 1) ? midiNoteNumber + 2
                                                                                    : midiNoteNumber + 1;
            const bool drawLeft  = differs (midiNoteNumber, prevWhite);
            const bool drawRight = differs (midiNoteNumber, nextWhite) && ! colourAt (nextWhite).has_value();

            const float vy = area.getY() + t;
            const float vh = area.getHeight() - 2.0f * t;                           // inset both ends → clean corners
            if (drawLeft)  g.fillRect (area.getX(),         vy, t, vh);
            if (drawRight) g.fillRect (area.getRight() - t, vy, t, vh);
        }
        else
        {
            // Black key: shorter than the white keys, so it has no
            // keyboard-bottom rim. Its left edge + bottom-left half are the
            // boundary against the white key below-left (note-1); its right edge
            // + bottom-right half are the boundary against the white below-right
            // (note+1). The bottom is split at the white seam beneath the key.
            // That seam, when it's a boundary, is owned by the right white key
            // when that key is coloured (drawn just right of the seam line) and
            // by the left white key otherwise (just left of it) — exactly as the
            // white-key branch decides above. We leave the matching t-wide gap on
            // that side so the white key's full-height seam passes through.
            const bool leftBound  = differs (midiNoteNumber, midiNoteNumber - 1);
            const bool rightBound = differs (midiNoteNumber, midiNoteNumber + 1);

            const float vy = area.getY() + t;
            const float vh = area.getHeight() - t;                                  // down to the black key's bottom
            if (leftBound)  g.fillRect (area.getX(),         vy, t, vh);
            if (rightBound) g.fillRect (area.getRight() - t, vy, t, vh);

            const bool seamExists  = differs (midiNoteNumber - 1, midiNoteNumber + 1);
            const bool seamOnRight = seamExists &&   colourAt (midiNoteNumber + 1).has_value();
            const bool seamOnLeft  = seamExists && ! colourAt (midiNoteNumber + 1).has_value();

            const float shelfY = area.getBottom() - t;
            const float seam   = getRectangleForKey (midiNoteNumber - 1).getRight();   // == getRectangleForKey(note+1).getX()
            if (leftBound)
            {
                const float x0 = area.getX() + t;                                   // abut the left edge
                const float x1 = seamOnLeft ? (seam - t) : seam;                    // abut the seam (on whichever side it sits)
                g.fillRect (x0, shelfY, jmax (0.0f, x1 - x0), t);
            }
            if (rightBound)
            {
                const float x0 = seamOnRight ? (seam + t) : seam;
                const float x1 = area.getRight() - t;                               // abut the right edge
                g.fillRect (x0, shelfY, jmax (0.0f, x1 - x0), t);
            }
        }
    }
    else
    {
        // Vertical orientation. (Note-adjacency model; the black-key step/shelf
        // handling above is horizontal-only for now.)
        const bool startOfRun = differs (midiNoteNumber, midiNoteNumber - 1);
        const bool endOfRun    = differs (midiNoteNumber, midiNoteNumber + 1);
        const bool nextInRange = (midiNoteNumber + 1) <= getRangeEnd();
        const bool nextIsWhite = nextInRange && ! MidiMessage::isMidiNoteBlack (midiNoteNumber + 1);
        // Only let the neighbour own the shared seam if it's actually a coloured
        // trigger (otherwise it draws nothing and the seam would be lost).
        const bool nextIsTrigger = colourAt (midiNoteNumber + 1).has_value();
        const bool endCoincides = endOfRun && nextInRange && nextIsTrigger && ! isBlack && nextIsWhite;
        const bool drawStartEdge = startOfRun;
        const bool drawEndEdge   = endOfRun && ! endCoincides;

        const bool facingLeft   = (getOrientation() == verticalKeyboardFacingLeft);
        const bool drawLeftRim  = ! (isBlack && ! facingLeft);   // facingRight → left is interior
        const bool drawRightRim = ! (isBlack &&   facingLeft);   // facingLeft  → right is interior

        if (drawLeftRim)  g.fillRect (area.getX(),         area.getY(), t, area.getHeight());
        if (drawRightRim) g.fillRect (area.getRight() - t, area.getY(), t, area.getHeight());

        const float lInset = drawLeftRim  ? t : 0.0f;
        const float rInset = drawRightRim ? t : 0.0f;
        const float vx = area.getX() + lInset;
        const float vw = area.getWidth() - lInset - rInset;
        if (drawStartEdge) g.fillRect (vx, area.getY(),          vw, t);
        if (drawEndEdge)   g.fillRect (vx, area.getBottom() - t, vw, t);
    }
}

//==============================================================================
// TriggerEditor — the data side of inline editing.
//==============================================================================

ValueTree NewMidiKeyboardComponent::TriggerEditor::findNodeByID (ValueTree tree, const String& targetID)
{
    if (tree.hasProperty ("id")
        && tree.getProperty ("id").toString().equalsIgnoreCase (targetID))
        return tree;

    for (auto child : tree)
        if (auto found = findNodeByID (child, targetID); found.isValid())
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

    if (nodes.size() == 1)
    {
        // Bare properties-only node — byte-compatible with the bank editor's
        // clipboard, so a single-key copy pastes into the node editor too.
        SystemClipboard::copyTextToClipboard (propertiesOnlyCopy (nodes.getFirst()).toXmlString());
    }
    else
    {
        // _multiCopy wrapper for a stack — matches the ToolBox / snippet
        // convention. Each child is properties-only, same as the single case.
        ValueTree multi ("_multiCopy");
        for (auto& n : nodes)
            multi.appendChild (propertiesOnlyCopy (n), nullptr);
        SystemClipboard::copyTextToClipboard (multi.toXmlString());
    }
}

void NewMidiKeyboardComponent::TriggerEditor::insertPayloadAtKey (ValueTree payloadRoot, int note)
{
    auto container = getContainer();
    if (! container.isValid() || ! payloadRoot.isValid())
        return;

    static const Identifier midiTriggerType ("MidiTrigger");

    // The bank is a fixed pool of MidiTrigger slots, so only MidiTrigger
    // payloads can fill them (mirrors pasteToSelected's type-match guard
    // against the destination node).
    Array<ValueTree> payload;
    auto take = [&] (const ValueTree& src)
    {
        if (src.getType() == midiTriggerType)
            payload.add (src.createCopy());
    };

    if (payloadRoot.getType().toString() == "_multiCopy")
        for (auto child : payloadRoot)
            take (child);
    else
        take (payloadRoot);

    if (payload.isEmpty())
        return;

    // One shared delta for the whole payload preserves the arrangement within
    // each node (e.g. transpose root stays centred) and between stacked nodes.
    // Anchor on the payload's lowest note → that note lands on the click.
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
            return;                              // wider than the keyboard, can't fit

        delta = note - lowest;
        if (lowest  + delta < 0)   delta = -lowest;       // clamp the delta as a unit
        if (highest + delta > 127) delta = 127 - highest;
    }

    // Fill the next empty slots in place — never append, never reindex. A
    // slot keeps its own trigger-index for life.
    auto slots = findEmptySlots (container, payload.size());
    if (slots.size() < payload.size())
        return;                                  // bank is full, nothing free

    auto* um = builder ? &builder->getUndoManager() : nullptr;
    if (um) um->beginNewTransaction ("Add trigger");

    for (int i = 0; i < payload.size(); ++i)
    {
        remapNoteRanges (payload.getReference (i), delta);     // on the detached copy
        fillSlot (slots.getReference (i), payload.getReference (i), um);
    }

    owner.repaint();
}

void NewMidiKeyboardComponent::TriggerEditor::pasteToKey (int note)
{
    insertPayloadAtKey (ValueTree::fromXml (SystemClipboard::getTextFromClipboard()), note);
}

void NewMidiKeyboardComponent::TriggerEditor::applyPresetFile (int note, const File& file)
{
    insertPayloadAtKey (ValueTree::fromXml (file.loadFileAsString()), note);
}

void NewMidiKeyboardComponent::TriggerEditor::clearKey (int note)
{
    auto container = getContainer();
    if (! container.isValid())
        return;

    auto nodes = findNodesForKey (note);
    if (nodes.isEmpty())
        return;

    auto* um = builder ? &builder->getUndoManager() : nullptr;
    if (um) um->beginNewTransaction ("Clear key");

    // Clear, don't delete — mirrors clearSelected. The slot stays alive with
    // its trigger-index; only its contents go.
    for (auto& node : nodes)
        clearPropertiesOfNode (node, um);

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

    owner.repaint();
}

void NewMidiKeyboardComponent::TriggerEditor::setColourForKey (int note, Colour colour)
{
    auto container = getContainer();
    if (! container.isValid())
        return;

    auto nodes = findNodesForKey (note);   // applies to all nodes covering the key
    if (nodes.isEmpty())
        return;

    auto* um = builder ? &builder->getUndoManager() : nullptr;
    if (um) um->beginNewTransaction ("Set trigger colour");

    for (auto& n : nodes)
        n.setProperty ("trigger-colour", colour.toString(), um);

    owner.repaint();
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
    menu.addItem (miCopy,  "Copy",  hasNodes);
    menu.addItem (miCut,   "Cut",   hasNodes);
    menu.addItem (miPaste, "Paste", hasClip);
    menu.addItem (miClear, "Clear", hasNodes);

    // Colour — recolour the node(s) on this key. Item text drawn in the colour.
    PopupMenu colourMenu;
    for (int i = 0; i < (int) kTriggerColours.size(); ++i)
        colourMenu.addColouredItem (miColourBase + i,
                                    kTriggerColours[(size_t) i].name,
                                    kTriggerColours[(size_t) i].colour);
    menu.addSubMenu ("Colour", colourMenu, hasNodes);

    // Add Trigger — insert a preset node/stack from disk onto this key.
    std::vector<File> presetFiles;
    PopupMenu addTriggerMenu;
    buildPresetMenu (addTriggerMenu, presetFolder, presetFiles, miPresetBase);
    menu.addSubMenu ("Add Trigger", addTriggerMenu, presetFolder.isDirectory());

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
        else if (result >= miColourBase && result < miColourBase + (int) kTriggerColours.size())
            setColourForKey (note, kTriggerColours[(size_t) (result - miColourBase)].colour);
        else if (result >= miPresetBase && (size_t) (result - miPresetBase) < presetFiles.size())
            applyPresetFile (note, presetFiles[(size_t) (result - miPresetBase)]);

        kb->setActiveEditNote (-1);
    });
}

} // namespace juce
