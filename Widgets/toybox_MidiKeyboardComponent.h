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

    /** Sets the note whose trigger group is currently "active" (its menu is
        open) so the keyboard can draw a heavier outline on that run.
        -1 clears it. Driven internally by the editor's menu lifetime. */
    void setActiveEditNote (int note);

    /** Called when the user clicks the in-keyboard close box (top-left, shown
        only while edit mode is on). Wire this to clear the edit-mode value so
        that any external control bound to it updates too, e.g.:
        @code
            keyboard.onExitEditMode = [this] { editModeValue.setValue (0); };
        @endcode
        The keyboard deliberately doesn't toggle editMode itself here — it lets
        that value drive setEditMode, keeping a single source of truth. */
    std::function<void()> onExitEditMode;

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

    /** Fixed top-left bounds of the edit-mode close box, in component
        coordinates (so it stays put while the keys scroll). Only drawn and
        hit-tested while editMode is on. */
    Rectangle<float> getCloseButtonBounds() const;

    /** Edit-mode region edge resize. A trigger region is the contiguous run of
        same-coloured notes; dragging within a few pixels of its left/right edge
        changes the region's lower/upper note (applied to every node in the
        stack). hitTestRegionEdge reports the edge under a point, if any. */
    struct RegionEdge { bool valid = false; bool lowEdge = false; int regionStart = -1, regionEnd = -1; };
    RegionEdge hitTestRegionEdge (Point<float> pos);

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

        void showMenuForKey (int note, Point<int> screenPos);

        // Region edge resize — driven directly from the keyboard's mouse
        // handlers. beginEdgeResize captures the low (or high) note bounds of
        // every range pair on every node covering edgeNote, plus the clamp range
        // that keeps all ranges at least one note wide. Drag deltas (from the
        // edge's original note) are applied absolutely so there's no drift,
        // coalesced into a single undo transaction.
        bool beginEdgeResize  (int edgeNote, bool lowEdge);
        void updateEdgeResize (int delta);
        void endEdgeResize();

    private:
        ValueTree        getContainer() const;
        Array<ValueTree> findNodesForKey (int note) const;
        static ValueTree findNodeByID (ValueTree tree, const String& id);
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
        void clearAll();
        void setColourForKey (int note, Colour colour);
        void applyPresetFile (int note, const File& file);
        void insertPayloadAtKey (ValueTree payloadRoot, int note);
        void buildPresetMenu (PopupMenu& menu, const File& folder,
                              std::vector<File>& files, int baseId);
        static void remapNoteRanges (ValueTree node, int delta);

        NewMidiKeyboardComponent& owner;
        foleys::MagicGUIBuilder*  builder = nullptr;
        String                    containerID;
        File                      presetFolder;

        // Edge-resize session (valid while a drag is in progress).
        struct EdgeBound { ValueTree node; Identifier prop; int orig; };
        std::vector<EdgeBound> resizeBounds;
        int  resizeMinDelta = 0, resizeMaxDelta = 0;
        bool resizeActive   = false;
    };

    TriggerEditor triggerEditor { *this };

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
