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

//==============================================================================
NewMidiKeyboardComponent::NewMidiKeyboardComponent (MidiKeyboardState& stateToUse, Orientation orientationToUse)
    : KeyboardComponentBase (orientationToUse), state (stateToUse)
{
    state.addListener (this);

    // initialise with a default set of qwerty key-mappings.
    const std::string_view keys { "awsedftgyhujkolp;" };

    for (const char& c : keys)
        setKeyPressForNote ({c, 0, 0}, (int) std::distance (keys.data(), &c));

    mouseOverNotes.insertMultiple (0, -1, 32);
    mouseDownNotes.insertMultiple (0, -1, 32);

    colourChanged();
    setWantsKeyboardFocus (true);

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
    if (noPendingUpdates.exchange (true))
        return;

    for (auto i = getRangeStart(); i <= getRangeEnd(); ++i)
    {
        const auto isOn = state.isNoteOnForChannels (midiInChannelMask, i);

        if (keysCurrentlyDrawnDown[i] != isOn)
        {
            keysCurrentlyDrawnDown.setBit (i, isOn);
            repaintNote (i);
        }
    }
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

    // 1) Fill with the white note base colour
    g.setColour (findColour (whiteNoteColourId));
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
        auto c = Colours::transparentWhite;
        if (isDown)       c = findColour (keyDownOverlayColourId);
        else if (isOver)  c = findColour (mouseOverKeyOverlayColourId);

        g.setColour (c);
        g.fillPath (keyShape);
    }

    // 5) Text label (C markers)
    auto text = getWhiteNoteText (midiNoteNumber);

    if (text.isNotEmpty())
    {
        auto fontHeight = jmin (14.0f, getKeyWidth() * 0.9f);

        g.setColour (textColour);
        g.setFont (withDefaultMetrics (FontOptions { fontHeight }).withHorizontalScale (0.8f));

        switch (currentOrientation)
        {
            case horizontalKeyboard:            g.drawText (text, keyArea.withTrimmedLeft (1.0f).withTrimmedBottom (7.0f), Justification::centredBottom, false); break;
            case verticalKeyboardFacingLeft:    g.drawText (text, keyArea.reduced (2.0f), Justification::centredLeft,   false); break;
            case verticalKeyboardFacingRight:   g.drawText (text, keyArea.reduced (2.0f), Justification::centredRight,  false); break;
            default: break;
        }
    }
}

void NewMidiKeyboardComponent::drawBlackNote (int /*midiNoteNumber*/, Graphics& g, Rectangle<float> area,
                                           bool isDown, bool isOver, Colour noteFillColour)
{
    auto c = noteFillColour;
    if (isDown)  c = c.brighter (0.30f);
    if (isOver)  c = c.overlaidWith (findColour (mouseOverKeyOverlayColourId));

    const auto currentOrientation = getOrientation();
    const auto cornerSize = 3.0f;

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
                                   cornerSize * 0.6f, cornerSize * 0.6f,
                                   false, false,
                                   currentOrientation == horizontalKeyboard,
                                   currentOrientation == horizontalKeyboard);

    g.setColour (c.brighter (isDown ? 0.14f : 0.22f));
    g.fillPath (faceShape);

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
    if (currentOrientation == horizontalKeyboard)
    {
        g.setColour (Colours::white.withAlpha (0.07f));
        g.fillRect (faceArea.withHeight (1.0f));
    }

    // 5) Gradient for depth — lighter at top, slightly lighter at bottom
    //    to keep contrast visible against the side edges
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

    // 7) Overlay key-down tint — reduced opacity so the 3D structure reads through
    if (isDown)
    {
        g.setColour (findColour (keyDownOverlayColourId).withMultipliedAlpha (0.4f));
        g.fillPath (keyShape);
    }
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
bool NewMidiKeyboardComponent::mouseDownOnKey    ([[maybe_unused]] int midiNoteNumber, [[maybe_unused]] const MouseEvent& e)  { return true; }
bool NewMidiKeyboardComponent::mouseDraggedToKey ([[maybe_unused]] int midiNoteNumber, [[maybe_unused]] const MouseEvent& e)  { return true; }
void NewMidiKeyboardComponent::mouseUpOnKey      ([[maybe_unused]] int midiNoteNumber, [[maybe_unused]] const MouseEvent& e)  {}

} // namespace juce
