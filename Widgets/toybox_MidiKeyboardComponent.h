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

#pragma once

#include <melatonin_blur/melatonin_blur.h>

// Forward declaration only — the keyboard holds a pointer to the builder for
// the inline trigger editor, and resolves the container tree through it. The
// full definition is pulled in by the foleys module unity build in the .cpp.
namespace foleys { class MagicGUIBuilder; }

namespace juce
{

//==============================================================================
/**
    A component that displays a piano keyboard, whose notes can be clicked on.

    This component will mimic a physical midi keyboard, showing the current state of
    a MidiKeyboardState object. When the on-screen keys are clicked on, it will play these
    notes by calling the noteOn() and noteOff() methods of its MidiKeyboardState object.

    Another feature is that the computer keyboard can also be used to play notes. By
    default it maps the top two rows of a standard qwerty keyboard to the notes, but
    these can be remapped if needed. It will only respond to keypresses when it has
    the keyboard focus, so to disable this feature you can call setWantsKeyboardFocus (false).

    The component is also a ChangeBroadcaster, so if you want to be informed when the
    keyboard is scrolled, you can register a ChangeListener for callbacks.

    @see MidiKeyboardState

    @tags{Audio}
*/
class NewMidiKeyboardComponent  : public KeyboardComponentBase,
                                  private MidiKeyboardState::Listener,
                                  public juce::TooltipClient,
                                  private Timer
{
public:
    //==============================================================================
    /** Creates a NewMidiKeyboardComponent.

        @param state        the midi keyboard model that this component will represent
        @param orientation  whether the keyboard is horizontal or vertical
    */
    NewMidiKeyboardComponent (MidiKeyboardState& state, Orientation orientation);

    /** Destructor. */
    ~NewMidiKeyboardComponent() override;

    //==============================================================================
    /** Changes the velocity used in midi note-on messages that are triggered by clicking
        on the component.

        Values are 0 to 1.0, where 1.0 is the heaviest.

        @see setMidiChannel
    */
    void setVelocity (float velocity, bool useMousePositionForVelocity);

    //==============================================================================
    /** Changes the midi channel number that will be used for events triggered by clicking
        on the component.

        The channel must be between 1 and 16 (inclusive). This is the channel that will be
        passed on to the MidiKeyboardState::noteOn() method when the user clicks the component.

        Although this is the channel used for outgoing events, the component can display
        incoming events from more than one channel - see setMidiChannelsToDisplay()

        @see setVelocity
    */
    void setMidiChannel (int midiChannelNumber);

    /** Returns the midi channel that the keyboard is using for midi messages.
        @see setMidiChannel
    */
    int getMidiChannel() const noexcept            { return midiChannel; }

    /** Sets a mask to indicate which incoming midi channels should be represented by
        key movements.

        The mask is a set of bits, where bit 0 = midi channel 1, bit 1 = midi channel 2, etc.

        If the MidiKeyboardState has a key down for any of the channels whose bits are set
        in this mask, the on-screen keys will also go down.

        By default, this mask is set to 0xffff (all channels displayed).

        @see setMidiChannel
    */
    void setMidiChannelsToDisplay (int midiChannelMask);

    /** Returns the current set of midi channels represented by the component.
        This is the value that was set with setMidiChannelsToDisplay().
    */
    int getMidiChannelsToDisplay() const noexcept  { return midiInChannelMask; }

    //==============================================================================
    /** Deletes all key-mappings.

        @see setKeyPressForNote
    */
    void clearKeyMappings();

    /** Maps a key-press to a given note.

        @param key                  the key that should trigger the note
        @param midiNoteOffsetFromC  how many semitones above C the triggered note should
                                    be. The actual midi note that gets played will be
                                    this value + (12 * the current base octave). To change
                                    the base octave, see setKeyPressBaseOctave()
    */
    void setKeyPressForNote (const KeyPress& key, int midiNoteOffsetFromC);

    /** Removes any key-mappings for a given note.

        For a description of what the note number means, see setKeyPressForNote().
    */
    void removeKeyPressForNote (int midiNoteOffsetFromC);

    /** Changes the base note above which key-press-triggered notes are played.

        The set of key-mappings that trigger notes can be moved up and down to cover
        the entire scale using this method.

        The value passed in is an octave number between 0 and 10 (inclusive), and
        indicates which C is the base note to which the key-mapped notes are
        relative.
    */
    void setKeyPressBaseOctave (int newOctaveNumber);
    
    //==============================================================================
    /** Sets a callback that returns an optional override colour for each MIDI note.
        The keyboard polls this from its 20Hz timer and repaints keys whose colour
        has changed. Pass nullptr to disable.
    */
    void setNoteColourProvider (std::function<std::optional<Colour>(int midiNote)> fn);
    
    /** Sets a callback that returns an optional label for each MIDI note.
        Polled from the 20Hz timer; keys whose label changed are repainted.
        The trigger should only return a value for the first (lowest) note
        of its range; other notes return nullopt. Pass nullptr to disable. */
    void setNoteLabelProvider (std::function<std::optional<String>(int midiNote)> fn);
    
    /** Sets a callback that returns an optional tooltip string for each MIDI note.
        Polled by TooltipWindow on the message thread when the mouse hovers.
        Pass nullptr to disable. */
    void setNoteTooltipProvider (std::function<std::optional<String>(int midiNote)> fn);

    //==============================================================================
    /** A set of colour IDs to use to change the colour of various aspects of the keyboard.

        These constants can be used either via the Component::setColour(), or LookAndFeel::setColour()
        methods.

        @see Component::setColour, Component::findColour, LookAndFeel::setColour, LookAndFeel::findColour
    */
    enum ColourIds
    {
        whiteNoteColourId               = 0x1005000,
        blackNoteColourId               = 0x1005001,
        keySeparatorLineColourId        = 0x1005002,
        mouseOverKeyOverlayColourId     = 0x1005003,  /**< This colour will be overlaid on the normal note colour. */
        keyDownOverlayColourId          = 0x1005004,  /**< This colour will be overlaid on the normal note colour. */
        textLabelColourId               = 0x1005005,
        shadowColourId                  = 0x1005006,
        keyLabelTextColourId            = 0x1005007,
        editOutlineColourId             = 0x1005008   /**< Outline drawn around trigger key-groups in edit mode. */
    };

    //==============================================================================
    /** Use this method to draw a white note of the keyboard in a given rectangle.

        isOver indicates whether the mouse is over the key, isDown indicates whether the key is
        currently pressed down.

        When doing this, be sure to note the keyboard's orientation.
    */
    virtual void drawWhiteNote (int midiNoteNumber, Graphics& g, Rectangle<float> area,
                                bool isDown, bool isOver, Colour lineColour, Colour textColour);

    /** Use this method to draw a black note of the keyboard in a given rectangle.

        isOver indicates whether the mouse is over the key, isDown indicates whether the key is
        currently pressed down.

        When doing this, be sure to note the keyboard's orientation.
    */
    virtual void drawBlackNote (int midiNoteNumber, Graphics& g, Rectangle<float> area,
                                bool isDown, bool isOver, Colour noteFillColour);

    /** Callback when the mouse is clicked on a key.

        You could use this to do things like handle right-clicks on keys, etc.

        Return true if you want the click to trigger the note, or false if you
        want to handle it yourself and not have the note played.

        @see mouseDraggedToKey
    */
    virtual bool mouseDownOnKey (int midiNoteNumber, const MouseEvent& e);

    /** Callback when the mouse is dragged from one key onto another.

        Return true if you want the drag to trigger the new note, or false if you
        want to handle it yourself and not have the note played.

        @see mouseDownOnKey
    */
    virtual bool mouseDraggedToKey (int midiNoteNumber, const MouseEvent& e);

    /** Callback when the mouse is released from a key.

        @see mouseDownOnKey
    */
    virtual void mouseUpOnKey (int midiNoteNumber, const MouseEvent& e);

    /** Allows text to be drawn on the white notes.

        By default this is used to label the C in each octave, but could be used for other things.

        @see setOctaveForMiddleC
    */
    virtual String getWhiteNoteText (int midiNoteNumber);

    //==============================================================================
    /** @internal */
    void mouseMove (const MouseEvent&) override;
    /** @internal */
    void mouseDrag (const MouseEvent&) override;
    /** @internal */
    void mouseDown (const MouseEvent&) override;
    /** @internal */
    void mouseUp (const MouseEvent&) override;
    /** @internal */
    void mouseEnter (const MouseEvent&) override;
    /** @internal */
    void mouseExit (const MouseEvent&) override;
    /** @internal */
    void timerCallback() override;
    /** @internal */
    bool keyStateChanged (bool isKeyDown) override;
    /** @internal */
    bool keyPressed (const KeyPress&) override;
    /** @internal */
    void focusLost (FocusChangeType) override;
    /** @internal */
    void colourChanged() override;
    /** @internal */
    void paint (Graphics& g) override;
    /** @internal */
    bool hitTest (int x, int y) override;
    
    void setInitialLowestKeyShowing(int keyNumber) {
        initialLowestKeyShowing = keyNumber;
        setLowestVisibleKey (keyNumber);
        repaint();
    };
    
    int getInitialLowestKeyShowing() {return initialLowestKeyShowing; };

    //==============================================================================
    // Inline trigger editing (edit mode).
    //
    // When edit mode is on, clicking a key opens an editing menu instead of
    // playing the note. The keyboard owns the edit-mode flag (it needs it for
    // its own outline drawing) and a nested TriggerEditor that does the data
    // side (resolving a key to node(s) in the container tree and performing
    // copy / cut / paste / clear / colour / add-from-preset). See setEditContext.

    /** Turns inline trigger editing on/off. While on, key clicks open the edit
        menu and don't play notes; the trigger-group outlines are drawn. */
    void setEditMode (bool shouldBeOn);
    bool isEditMode() const noexcept { return editMode; }

    /** Gives the editor the builder + the id of the container node whose
        triggers this keyboard edits. Must match the container id used by the
        Trigger Bank Editor item if you want them to edit the same bank. */
    void setEditContext (foleys::MagicGUIBuilder* builder, const String& containerID);

    /** Folder scanned for the "Add Trigger" preset menu (e.g. the plugin's
        Triggers folder). When empty / missing, the submenu is disabled. */
    void setTriggerPresetFolder (const File& folder);
    
    /** Bulds the factory 'Triggers' folder. */
    static void createFactoryTriggerPresets();

    /** Sets the note whose trigger group is currently "active" (its menu is
        open) so the keyboard can draw a heavier outline on that run.
        -1 clears it. Driven internally by the editor's menu lifetime. */
    void setActiveEditNote (int note);

    /** Called when the user clicks the in-keyboard toggle button (top-left)
        while edit mode is on (the button shows an x). Wire this to clear the
        edit-mode value so any external control bound to it updates too, e.g.:
        @code
            keyboard.onExitEditMode = [this] { editModeValue.setValue (0); };
        @endcode
        The keyboard deliberately doesn't toggle editMode itself here — it lets
        that value drive setEditMode, keeping a single source of truth. */
    std::function<void()> onExitEditMode;

    /** Mirror of onExitEditMode — called when the user clicks the toggle
        button while edit mode is off (the button shows a +). Wire to set the
        edit-mode value to 1, e.g.:
        @code
            keyboard.onEnterEditMode = [this] { editModeValue.setValue (1); };
        @endcode
        Same single-source-of-truth rule as onExitEditMode: the keyboard
        doesn't flip editMode itself. */
    std::function<void()> onEnterEditMode;

    //==============================================================================
    // Macro-panel toggle button.
    //
    // A second fixed top-left button, drawn 8px below the edit-mode toggle,
    // showing three dots ("..."). It's a simple open/closed toggle that drives
    // an external value (typically backed by an APVTS parameter), independent of
    // edit mode and the trigger context. Same single-source-of-truth rule as
    // edit mode: clicking calls onToggleMacroPanel (which flips the bound value)
    // and the value drives setMacroPanelOpen back; the keyboard never flips its
    // own flag.

    /** Shows / hides the macro-panel button. Hidden by default; turn it on only
        when a macro value is actually bound. */
    void setMacroButtonVisible (bool shouldBeVisible);
    bool isMacroButtonVisible() const noexcept { return macroButtonVisible; }

    /** Reflects the macro panel's open state, driven by the bound value (the
        keyboard never flips it itself). While open, the keyboard hides itself
        entirely: it stops painting the keys, becomes a transparent click-through
        overlay (every click except the close button falls through to the
        component behind), hides its scroll buttons, and draws a single close
        button (an x-disc, same look as the edit-mode toggle) that toggles the
        value back to 0. */
    void setMacroPanelOpen (bool isOpen);
    bool isMacroPanelOpen() const noexcept { return macroPanelOpen; }

    /** Called when the user clicks the macro button. Wire this to flip the
        bound macro value (0/1), e.g.:
        @code
            keyboard.onToggleMacroPanel = [this]
                { macroPanelValue.setValue (macroPanelValue.getValue() ? 0 : 1); };
        @endcode */
    std::function<void()> onToggleMacroPanel;

private:
    //==============================================================================
    void drawKeyboardBackground (Graphics& g, Rectangle<float> area) override final;
    void drawWhiteKey (int midiNoteNumber, Graphics& g, Rectangle<float> area) override final;
    void drawBlackKey (int midiNoteNumber, Graphics& g, Rectangle<float> area) override final;

    void handleNoteOn  (MidiKeyboardState*, int, int, float) override;
    void handleNoteOff (MidiKeyboardState*, int, int, float) override;

    //==============================================================================
    void resetAnyKeysInUse();
    void updateNoteUnderMouse (Point<float>, bool isDown, int fingerNum);
    void updateNoteUnderMouse (const MouseEvent&, bool isDown);
    void repaintNote (int midiNoteNumber);
    void drawLabelForKey (Graphics& g, Rectangle<float> keyArea,
                          std::optional<Colour> baseFill, int midiNoteNumber);

    /** Edit-mode zone outlines. drawEditOutlines walks the visible range,
        groups each maximal run of same-coloured notes into a zone, and calls
        drawZoneOutline to stroke that zone's silhouette as a single path
        (straight top/bottom, L-stepped sides around the black keys). Called from
        paint() after the keys; the active group (menu open) is stroked heavier. */
    void drawEditOutlines (Graphics& g);
    void drawZoneOutline  (Graphics& g, int startNote, int endNote);

    /** Bounds of the edit-mode toggle button — top-left, "+" and "x" in place.
        iOS buttons are fatter for touch. Drawn / hit-tested whenever
        triggerEditor.hasContext() is true and the macro panel isn't open. */
    Rectangle<float> getCloseButtonBounds() const;

    /** Bounds of the macro-panel toggle button — top-right, the edit button
        mirrored across the keyboard (same distance from the right edge as the
        edit button is from the left). Holds both the idle "..." (hidden while
        edit mode is active) and the panel-open close (x) overlay. */
    Rectangle<float> getMacroButtonBounds() const;

    /** Bounds of just the panel-open close (x) box — derived from the macro
        button on the right with an x offset, so it can be nudged independently
        of the "..." button. */
    Rectangle<float> getMacroCloseButtonBounds() const;

    /** Edit-mode region edge resize. A trigger region is the contiguous run of
        same-coloured notes; dragging within a few pixels of its left/right edge
        changes the region's lower/upper note (applied to every node in the
        stack). hitTestRegionEdge reports the edge under a point, if any. */
    struct RegionEdge { bool valid = false; bool lowEdge = false; int regionStart = -1, regionEnd = -1; };
    RegionEdge hitTestRegionEdge (Point<float> pos);

    /** Edit-mode region drag handle. A 30-px band at the top of each region's
        keys; dragging within it translates the whole region (low + high notes
        shift together by one delta). hitTestRegionHandle reports the region
        under a point if it's inside the band, else invalid. */
    struct RegionHandle { bool valid = false; int regionStart = -1, regionEnd = -1; };
    RegionHandle hitTestRegionHandle (Point<float> pos);

    int initialLowestKeyShowing = 24;

    //==============================================================================
    MidiKeyboardState& state;
    int midiChannel = 1, midiInChannelMask = 0xffff;
    int keyMappingOctave = 6;

    float velocity = 1.0f;
    bool useMousePositionForVelocity = true;

    Array<int> mouseOverNotes, mouseDownNotes;
    Array<KeyPress> keyPresses;
    Array<int> keyPressNotes;
    BigInteger keysPressed, keysCurrentlyDrawnDown;

    std::atomic<bool> noPendingUpdates { true };
    
    std::function<std::optional<Colour>(int)> noteColourProvider;
    std::array<uint32_t, 128> lastColourArgb {};
    
    std::function<std::optional<String>(int)> noteLabelProvider;
    std::array<String, 128> lastLabels {};
    juce::Typeface::Ptr typeface;
    
    std::function<std::optional<String>(int)> noteTooltipProvider;

    melatonin::DropShadow blackKeyShadow { Colours::black.withAlpha (0.5f), 4, { 0, 2 } };

    //==============================================================================
    // Edit-mode state.
    bool editMode       = false;
    int  activeEditNote = -1;   // note whose group is highlighted while its menu is open
    int  activeRunStart = -1;   // computed run extent of the active note, for the heavy outline
    int  activeRunEnd   = -1;

    // Region edge-resize drag (set on mouseDown over an edge).
    bool resizingRegion   = false;
    bool resizeLowEdge    = false;
    int  resizeAnchorNote = -1;   // the edge's original note, drag deltas are measured from here

    // Hovered drag-handle region (top-band hover indicator). Cleared while a
    // drag is in progress, repainted on hover changes. -1 = nothing hovered.
    int hoveredHandleStart = -1;
    int hoveredHandleEnd   = -1;

    //==============================================================================
    // Macro-panel button state. Visibility and open-state are both externally
    // driven (the bound value); the keyboard only stores them for drawing.
    bool macroButtonVisible = false;
    bool macroPanelOpen     = false;

    // Set in mouseDown when the press is consumed by a button (macro toggle,
    // edit toggle, or a right-click menu) rather than a key. The press still
    // captures the mouse, so a tiny movement fires mouseDrag with the cursor
    // over a key beneath the button; without this flag that drag would leak a
    // note-on. Reset at the top of every mouseDown.
    bool mouseDownConsumedByButton = false;

    // Scroll position stashed while the panel is open (hiding the scroll buttons
    // resets the keyboard to its leftmost key); restored on reshow. -1 = unset.
    int  savedLowestVisibleKey = -1;

    //==============================================================================
    /** Plain nested helper — not a Component, never shown, no bounds, no paint.
        Owns the data side of inline editing: resolves a clicked key to the
        node(s) in the container tree and performs copy / cut / paste / clear /
        colour / add-from-preset, plus building the menu. Its lifetime equals
        the keyboard's, so the owner SafePointer used in showMenuForKey also
        guards `this`. */
    class TriggerEditor
    {
    public:
        explicit TriggerEditor (NewMidiKeyboardComponent& o) : owner (o) {}

        void setContext (foleys::MagicGUIBuilder* b, const String& id) { builder = b; containerID = id; }
        void setPresetFolder (const File& f) { presetFolder = f; }

        /** True iff the editor has a builder and a non-empty container ID, i.e.
            it could actually resolve a node to operate on. Mirrors the guard at
            the top of getContainer(). Drives the visibility of the in-keyboard
            edit-mode toggle button. */
        bool hasContext() const noexcept { return builder != nullptr && ! containerID.isEmpty(); }

        void showMenuForKey (int note, Point<int> screenPos);

        // Region edge resize — driven directly from the keyboard's mouse
        // handlers. beginEdgeResize captures the low (or high) note bounds of
        // every range pair on every node covering edgeNote, plus the clamp range
        // that keeps all ranges at least one note wide. Follower nodes grouped
        // with the stack (matching node id; types in kRangeFollowerTypes —
        // Mapper, Arpeggiator) have their input-low/high-note
        // captured too, following the drag but saturating individually rather
        // than constraining it. Drag deltas (from the
        // edge's original note) are applied absolutely so there's no drift,
        // coalesced into a single undo transaction.
        bool beginEdgeResize  (int edgeNote, bool lowEdge);
        void updateEdgeResize (int delta);
        void endEdgeResize();

        // Region drag handle — captures BOTH edges of every range pair on every
        // node covering anyNoteInRegion (plus both edges of any grouped
        // followers), so a single delta translates the whole region. Reuses
        // updateEdgeResize / endEdgeResize unchanged.
        bool beginRegionDrag  (int anyNoteInRegion);

    private:
        ValueTree        getContainer() const;
        Array<ValueTree> findNodesForKey (int note) const;
        static ValueTree findNodeByID (ValueTree tree, const String& id,
                                       Identifier excludeType = {});
        static bool      nodeCoversKey (const ValueTree& node, int note);

        // Fixed-slot model — mirrors TriggerBankEditorComponent. The bank is a
        // pool of MidiTrigger slots (trigger-index 0..N); we fill empty slots
        // in place and clear them in place, never adding or removing slots.
        static bool      isEmptySlot        (const ValueTree& node);
        Array<ValueTree> findEmptySlots     (const ValueTree& container, int count) const;
        static ValueTree propertiesOnlyCopy (const ValueTree& node);
        void             clearPropertiesOfNode (ValueTree node, UndoManager* um);
        void             fillSlot           (ValueTree slot, const ValueTree& payload, UndoManager* um);

        void copyKeys        (const Array<ValueTree>& nodes);
        void pasteToKey      (int note);
        void clearKey        (int note);
        void doClear         (int note);
        void clearAll();
        void setColourForKey (int note, Colour colour);
        void saveKeyAs       (int note);
        void applyPresetFile (int note, const File& file);
        void insertPayloadAtKey (ValueTree payloadRoot, int note);
        void autoColourSnippet  (ValueTree& root);
        void buildPresetMenu (PopupMenu& menu, const File& folder,
                              std::vector<File>& files, int baseId);
        static void remapNoteRanges (ValueTree node, int delta);

        NewMidiKeyboardComponent& owner;
        foleys::MagicGUIBuilder*  builder = nullptr;
        String                    containerID;
        File                      presetFolder;

        // Held as a member so the chooser outlives its async dialog —
        // juce::FileChooser requires the instance to stay alive until the
        // user closes the box. Owned by the saveKeyAs path only.
        std::unique_ptr<FileChooser> fileChooser;

        // Edge-resize session (valid while a drag is in progress).
        // minV/maxV are per-bound final-value clamps: trigger bounds use the
        // full 0..127 (the session-wide delta clamps govern them); grouped
        // follower bounds (Mapper input range) saturate against these instead
        // of constraining the drag.
        struct EdgeBound { ValueTree node; Identifier prop; int orig;
                           int minV = 0; int maxV = 127; };
        std::vector<EdgeBound> resizeBounds;
        int  resizeMinDelta = 0, resizeMaxDelta = 0;
        bool resizeActive   = false;
    };

    TriggerEditor triggerEditor { *this };
    
    // Cycles round kTriggerColours when applyPresetFile autoColours an inserted
    // snippet. Reached from TriggerEditor through owner; per-keyboard so multiple
    // keyboards cycle independently.

    //==============================================================================
    
    juce::String getTooltip() override
    {
        if (! noteTooltipProvider)
            return {};

        const auto pos = getMouseXYRelative();

        if (! getLocalBounds().contains (pos))
            return {};

        const int note = getNoteAndVelocityAtPosition (pos.toFloat()).note;
        if (note < 0)
            return {};

        if (auto tip = noteTooltipProvider (note))
            return *tip;

        return {};
    }
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NewMidiKeyboardComponent)
};

} // namespace juce
