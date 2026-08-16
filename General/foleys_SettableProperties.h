/*
 ==============================================================================
    Copyright (c) 2019-2022 Foleys Finest Audio - Daniel Walz
    All rights reserved.

    License for non-commercial projects:

    Redistribution and use in source and binary forms, with or without modification,
    are permitted provided that the following conditions are met:
    1. Redistributions of source code must retain the above copyright notice, this
       list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright notice,
       this list of conditions and the following disclaimer in the documentation
       and/or other materials provided with the distribution.
    3. Neither the name of the copyright holder nor the names of its contributors
       may be used to endorse or promote products derived from this software without
       specific prior written permission.

    License for commercial products:

    To sell commercial products containing this module, you are required to buy a
    License from https://foleysfinest.com/developer/pluginguimagic/

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
    IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
    INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
    BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
    LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
    OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
    OF THE POSSIBILITY OF SUCH DAMAGE.
 ==============================================================================
*/

#pragma once

namespace foleys
{

/**
 Declares the patching direction of a value-carrying property for the node editor.
 none means the property is not rendered as a port.
 */
enum class PortRole
{
    none,
    in,
    out,
    both
};

/**
 Declares whether a port is in its box's default visible set.
 hidden ports live behind the + menu and appear only when connected or user-added.
 */
enum class PortVisibility
{
    hidden,
    shown
};

/**
 Declares what a port accepts, for the node editor's connection validation and
 for port and label colour. This is the port's declaration, not the domain of
 whatever is currently plugged into it — audioValue is a legal port but never a
 legal wire, since a wire always carries exactly one domain.

 audioValue is an input-side relaxation only: a value may drive an audioValue
 input, but audio may never drive a plain value input, and an audioValue output
 behaves as a plain audio output. Use it for audio inputs where a constant is
 meaningful — filter cutoff, VCA gain, FM depth — and plain audio for signal
 path inputs and outputs, where DC does nothing useful.
 */
enum class PortType
{
    value,
    midi,
    audio,
    audioValue
};

/**
 A SettableProperty is a value that can be selected by the designer and will be
 set for the Component each time the ValueTree is loaded.
 */
struct SettableProperty
{
    enum PropertyType
    {
        Text,           /*< Plain text, e.g. for buttons */
        Number,         /*< A number, e.g. line width */
        Colour,         /*< Show the colour selector and palette names */
        Toggle,         /*< Show a toggle for bool properties */
        Choice,         /*< Shows choices provided */
        Gradient,       /*< Show a bespoke gradient editor */
    };

    const juce::ValueTree  node;
    const juce::Identifier name;
    const PropertyType     type;
    const juce::var        defaultValue;
    const std::function<void(juce::ComboBox&)> menuCreationLambda;
    const juce::String     tooltip    = {};
    const juce::String     uidPrefix  = {};
    const PortRole         portRole   = PortRole::none;
    const PortVisibility   portVisibility = PortVisibility::hidden;
    const juce::String     portName   = {};   /*< Display label for the node editor port; falls back to the property name */
    const PortType         portType   = PortType::value;   /*< What the port accepts; uidPrefix carries no type meaning */
};

} // namespace foleys
