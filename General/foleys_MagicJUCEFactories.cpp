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

#include <juce_audio_utils/juce_audio_utils.h>

#include "foleys_MagicGUIBuilder.h"
#include "foleys_StringDefinitions.h"
#include "../Widgets/foleys_AutoOrientationSlider.h"
#include "../Widgets/foleys_XYDragComponent.h"
#include "../Widgets/foleys_MagicLevelMeter.h"
#include "../Widgets/foleys_MagicPlotComponent.h"
#include "../Widgets/foleys_MidiLearnComponent.h"
#include "../Widgets/foleys_MidiDrumpadComponent.h"
#include "../Helpers/foleys_PopupMenuHelper.h"

#pragma once

namespace foleys
{

class SliderItem : public GuiItem
                 , private juce::Timer
{

public:
    FOLEYS_DECLARE_GUI_FACTORY (SliderItem)

    static const juce::Identifier  pSliderType;
    static const juce::StringArray pSliderTypes;
    static const juce::Identifier  pSliderTextBox;
    static const juce::StringArray pTextBoxPositions;
    static const juce::Identifier  pValue;
    static const juce::Identifier  pNormalized;
    static const juce::Identifier  pValueSetsParameter;
    static const juce::Identifier  pMinValue;
    static const juce::Identifier  pMaxValue;
    static const juce::Identifier  pInterval;
    static const juce::Identifier  pModifierSnap;
    static const juce::Identifier  pSuffix;
    static const juce::Identifier  pSensitivity;
    static const juce::Identifier  pFilmStrip;
    static const juce::Identifier  pNumImages;
    static const juce::Identifier  pImage;
    static const juce::Identifier  pImageMode;
    static const juce::StringArray pImageModes;
    static const juce::Identifier  pStartAngle;
    static const juce::Identifier  pDisableScrollWheel;
    static const juce::Identifier  pPassMouseClicks;
    static const juce::Identifier  pAltKeyHides;
    static const juce::Identifier  pOverValue;
    static const juce::Identifier  pDownValue;

    SliderItem (MagicGUIBuilder& builder, const juce::ValueTree& node) : GuiItem (builder, node)
    {
        setColourTranslation (
        {
            { "slider-background", juce::Slider::backgroundColourId },
            { "slider-thumb", juce::Slider::thumbColourId },
            { "slider-track", juce::Slider::trackColourId },
            { "rotary-fill", juce::Slider::rotarySliderFillColourId },
            { "rotary-outline", juce::Slider::rotarySliderOutlineColourId },
            { "slider-text", juce::Slider::textBoxTextColourId },
            { "slider-text-background", juce::Slider::textBoxBackgroundColourId },
            { "slider-text-highlight", juce::Slider::textBoxHighlightColourId },
            { "slider-text-outline", juce::Slider::textBoxOutlineColourId }
        });

        addAndMakeVisible (slider);
        startTimer(300);
    }

    void update() override
    {
        attachment.reset();

        auto type = getProperty (pSliderType).toString();
        slider.setAutoOrientation (type.isEmpty() || type == pSliderTypes [0]);

        if (type == pSliderTypes [1])
            slider.setSliderStyle (juce::Slider::LinearHorizontal);
        else if (type == pSliderTypes [2])
            slider.setSliderStyle (juce::Slider::LinearVertical);
        else if (type == pSliderTypes [3])
            slider.setSliderStyle (juce::Slider::Rotary);
        else if (type == pSliderTypes [4])
            slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        else if (type == pSliderTypes [5])
            slider.setSliderStyle (juce::Slider::IncDecButtons);

        auto textbox = getProperty (pSliderTextBox).toString();
        if (textbox == pTextBoxPositions [0])
            slider.setTextBoxStyle (juce::Slider::NoTextBox, false, slider.getTextBoxWidth(), slider.getTextBoxHeight());
        else if (textbox == pTextBoxPositions [1])
            slider.setTextBoxStyle (juce::Slider::TextBoxAbove, false, slider.getTextBoxWidth(), slider.getTextBoxHeight());
        else if (textbox == pTextBoxPositions [3])
            slider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, slider.getTextBoxWidth(), slider.getTextBoxHeight());
        else if (textbox == pTextBoxPositions [4])
            slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, slider.getTextBoxWidth(), slider.getTextBoxHeight());
        else
            slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, slider.getTextBoxWidth(), slider.getTextBoxHeight());

        auto suffix = getProperty (pSuffix).toString();
        slider.setTextValueSuffix (suffix);

        auto valueID = configNode.getProperty (pValue, juce::String()).toString();
        
        auto paramID = getControlledParameterID ({});
        if (paramID.isNotEmpty())
            attachment = getMagicState().createAttachment (paramID, slider);
        
        if (valueID.isNotEmpty()){
            if (getProperty (pValueSetsParameter)){
                parameterValue.referTo (getMagicState().getPropertyAsValue (valueID));
                parameterValue.addListener (this);
            } else {
                if (getProperty (pNormalized)){
                    auto propertyID = getProperty (pValue).toString();
                    parameterValue.addListener (this);
                    if (propertyID.isNotEmpty())
                        getMagicState().getPropertyAsValue (propertyID);
                    
                    parameterValue.referTo (slider.getValueObject());
                } else {
                    slider.getValueObject().referTo (getMagicState().getPropertyAsValue (valueID));
                }
            }
        }

        auto filmStripName = getProperty (pFilmStrip).toString();
        if (filmStripName.isNotEmpty())
        {
            auto filmStrip = Resources::getImage (filmStripName);
            slider.setFilmStrip (filmStrip);
        }

        int numFilmImages = getProperty (pNumImages);
        slider.setNumImages (numFilmImages, false);
        
        auto imageName = getProperty (pImage).toString();
        if (imageName.isNotEmpty())
        {
            int dataSize = 0;
            const char* data = BinaryData::getNamedResource (imageName.toRawUTF8(), dataSize);
            if (data != nullptr)
                slider.createImage (data, dataSize);
        }
        
        auto mode = getProperty (pImageMode).toString();
        if (mode == "rotary")
            slider.setImageMode(0);
        if (mode == "horizontal")
            slider.setImageMode(1);
        if (mode == "vertical")
            slider.setImageMode(2);
        
        if (getProperty (pStartAngle).toString().isNotEmpty()){
            slider.setStartAngle(getProperty (pStartAngle));
        } else {
            slider.setStartAngle(-2.25);
        }

        if (getProperty (pDisableScrollWheel))
            slider.setScrollWheelEnabled(false);
        else
            slider.setScrollWheelEnabled(true);
          
        int sensitivity = getProperty (pSensitivity);
        if (sensitivity)
            slider.setMouseDragSensitivity(sensitivity);
        else
            slider.setMouseDragSensitivity(300);
        
        slider.setWantsKeyboardFocus(false);
        slider.setMouseClickGrabsKeyboardFocus(false);
        slider.setVelocityModeParameters(4, 1, 0, true, juce::ModifierKeys::shiftModifier);
        
        if (getProperty (pPassMouseClicks))
            slider.setInterceptsMouseClicks(false, false);

        if (getProperty (pAltKeyHides)){
            slider.setAltKeyHides (true);
            altKeyHides = true;
        } else {
            slider.setAltKeyHides (false);
            altKeyHides = false;
        }

        double minValue = getProperty (pMinValue);
        double maxValue = getProperty (pMaxValue);
        double interval = getProperty (pInterval);
        
        if (maxValue > minValue && !getProperty (pModifierSnap))
            slider.setRange (minValue, maxValue, interval);
        else if (paramID.isEmpty())
            slider.setRange (0.0, 1.0, 0.0);
        
        if (getProperty (pMaxValue).toString().isNotEmpty()){
            slider.setUseInterval (true);
            slider.setModifierSnap (getProperty (pModifierSnap));
            slider.setInterval (interval);
            slider.setMinValue (minValue);
            slider.setMaxValue (maxValue);   
        }
        
        juce::String overValuePropertyID = getProperty (pOverValue).toString();
        if (overValuePropertyID.isNotEmpty()) {
            getMagicState().getPropertyAsValue (overValuePropertyID);
        }
        
        juce::String downValuePropertyID = getProperty (pDownValue).toString();
        if (downValuePropertyID.isNotEmpty()){
            getMagicState().getPropertyAsValue (downValuePropertyID);
        }
        
        if (overValuePropertyID.isNotEmpty() || downValuePropertyID.isNotEmpty()){
            slider.addMouseListener (this, true);
        }
    }

    std::vector<SettableProperty> getSettableProperties() const override
    {
        std::vector<SettableProperty> props;

        props.push_back ({ configNode, IDs::parameter, SettableProperty::Choice, {}, magicBuilder.createParameterMenuLambda() , "Parameter to attach to this slider" });
        props.push_back ({ configNode, pSliderType, SettableProperty::Choice, pSliderTypes [0], magicBuilder.createChoicesMenuLambda (pSliderTypes) , "Slider style: auto, linear-horizontal, linear-vertical, rotary, rotary-horizontal-vertical, inc-dec-buttons" });
        props.push_back ({ configNode, pSliderTextBox, SettableProperty::Choice, pTextBoxPositions [2], magicBuilder.createChoicesMenuLambda (pTextBoxPositions) , "Position of the text box: no-textbox, above, below, left, right" });
        props.push_back ({ configNode, pValue, SettableProperty::Choice, 1.0f, magicBuilder.createPropertiesMenuLambda() , "Value to read or write the slider position" });
        props.push_back ({ configNode, pValueSetsParameter, SettableProperty::Toggle, {}, {} , "Use the value to set the parameter directly" });
        props.push_back ({ configNode, pNormalized, SettableProperty::Toggle, {}, {} , "Output the slider position as a normalized 0-1 value" });
        props.push_back ({ configNode, pMinValue, SettableProperty::Number, 0.0f, {} , "Minimum slider range value" });
        props.push_back ({ configNode, pMaxValue, SettableProperty::Number, 2.0f, {} , "Maximum slider range value" });
        props.push_back ({ configNode, pInterval, SettableProperty::Number, 0.0f, {} , "Step interval for the slider range" });
        props.push_back ({ configNode, pModifierSnap, SettableProperty::Toggle, {}, {} , "Snap to interval only when a modifier key is held" });
        props.push_back ({ configNode, pSuffix, SettableProperty::Text, {}, {} , "Text suffix appended to the slider value display" });
        props.push_back ({ configNode, pSensitivity, SettableProperty::Number, 200.0f, {} , "Mouse drag sensitivity in pixels per full range" });
        props.push_back ({ configNode, pFilmStrip, SettableProperty::Choice, 0.0f, magicBuilder.createChoicesMenuLambda(Resources::getResourceImageFileNames()) , "Film strip image for custom slider appearance" });
        props.push_back ({ configNode, pNumImages, SettableProperty::Number, 0.0f, {} , "Number of frames in the film strip image" });
        props.push_back ({ configNode, pImage, SettableProperty::Choice, 0.0f, magicBuilder.createChoicesMenuLambda(Resources::getResourceImageFileNames()) , "Image used for custom slider appearance (rotated or moved based on image-mode)" });
        props.push_back ({ configNode, pImageMode, SettableProperty::Choice, pImageModes [0], magicBuilder.createChoicesMenuLambda (pImageModes) , "How the image is rendered: rotary, horizontal, or vertical" });
        props.push_back ({ configNode, pStartAngle, foleys::SettableProperty::Number, {}, {} , "Start angle in radians for rotary sliders" });
        props.push_back ({ configNode, pDisableScrollWheel, SettableProperty::Toggle, {}, {} , "Disable mouse scroll wheel control of this slider" });
        props.push_back ({ configNode, pAltKeyHides, SettableProperty::Toggle, {}, {} , "Hide the slider when the Alt key is held down" });
        props.push_back ({ configNode, pPassMouseClicks, SettableProperty::Toggle, {}, {} , "Allow mouse clicks to pass through to components behind" });
        props.push_back ({ configNode, pOverValue, foleys::SettableProperty::Choice, {}, magicBuilder.createPropertiesMenuLambda() , "Value set to 1 when the mouse is over the slider" });
        props.push_back ({ configNode, pDownValue, foleys::SettableProperty::Choice, {}, magicBuilder.createPropertiesMenuLambda() , "Value set to 1 when the mouse is pressed on the slider" });

        return props;
    }

    juce::String getControlledParameterID (juce::Point<int>) override
    {
        return configNode.getProperty (IDs::parameter, juce::String()).toString();
    }

    juce::Component* getWrappedComponent() override
    {
        return &slider;
    }

private:
    AutoOrientationSlider slider;
    std::unique_ptr<juce::SliderParameterAttachment> attachment;
    juce::Value parameterValue;
    bool altKeyHides = false;

    void timerCallback() final
    {
        auto tooltipNode = getMagicState().getSettings().getChildWithName ("controls");
        auto velcityMode = tooltipNode.getProperty ("dynamic-knobs");
        if (velcityMode){
            slider.setVelocityBasedMode(true);
        }
        else{
            slider.setVelocityBasedMode(false);
        }

        if (altKeyHides){
            const auto& modifiers = juce::ModifierKeys::getCurrentModifiers();
            if (modifiers.isAltDown()){
                if (slider.isVisible())
                    slider.setVisible(false);
            } else {
                if (!slider.isVisible())
                    slider.setVisible(true);
            }
        }
    }
    
    void valueChanged (juce::Value& source) override
    {
        if (source.refersToSameSourceAs(parameterValue)){
            
            if (getProperty (pValueSetsParameter)){
                auto paramID = configNode.getProperty (IDs::parameter, juce::String()).toString();
                if (paramID.isNotEmpty())
                {
                    if (auto* parameter = getMagicState().getParameter (paramID))
                    {
                        auto normalizedValue = parameter->convertTo0to1(parameterValue.getValue());
                        parameter->setValueNotifyingHost(normalizedValue);
                    }
                }
            } else {
                auto propertyID = getProperty (pValue).toString();
                if (propertyID.isNotEmpty()){
                    getMagicState().getPropertyAsValue (propertyID).setValue(slider.valueToProportionOfLength(slider.getValue()));
                }
            }
        }
        
        handleValueChanged (source);
    }
    
    void mouseDown (const juce::MouseEvent& e) override
    {
        juce::String propertyID = getProperty (pDownValue).toString();
        if (propertyID.isNotEmpty()){
            getMagicState().getPropertyAsValue (propertyID).setValue (1.0f);
        }
#if FOLEYS_SHOW_GUI_EDITOR_PALLETTE
        foleys::GuiItem::mouseDown (e);
#endif
        return;
    }
      
    void mouseUp (const juce::MouseEvent& e) override
    {
        juce::String propertyID = getProperty (pDownValue).toString();
        if (propertyID.isNotEmpty()){
            getMagicState().getPropertyAsValue (propertyID).setValue (0.0f);
        }
#if FOLEYS_SHOW_GUI_EDITOR_PALLETTE
        foleys::GuiItem::mouseUp (e);
#endif
        return;
    }
          
    void mouseEnter (const juce::MouseEvent& e) override
    {
        juce::String propertyID = getProperty (pOverValue).toString();
        if (propertyID.isNotEmpty()){
            getMagicState().getPropertyAsValue (propertyID).setValue (1.0f);
        }
        return;
    }
              
    void mouseExit (const juce::MouseEvent& e) override
    {
        juce::String propertyID = getProperty (pOverValue).toString();
        if (propertyID.isNotEmpty()){
            getMagicState().getPropertyAsValue (propertyID).setValue (0.0f);
        }
        return;
    }
    

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SliderItem)
};
const juce::Identifier  SliderItem::pSliderType         { "slider-type" };
const juce::StringArray SliderItem::pSliderTypes        { "auto", "linear-horizontal", "linear-vertical", "rotary", "rotary-horizontal-vertical", "inc-dec-buttons" };
const juce::Identifier  SliderItem::pSliderTextBox      { "slider-textbox" };
const juce::StringArray SliderItem::pTextBoxPositions   { "no-textbox", "textbox-above", "textbox-below", "textbox-left", "textbox-right" };
const juce::Identifier  SliderItem::pValue              { "value" };
const juce::Identifier  SliderItem::pNormalized         { "normalized" };
const juce::Identifier  SliderItem::pValueSetsParameter { "value-sets-parameter" };
const juce::Identifier  SliderItem::pMinValue           { "min-value" };
const juce::Identifier  SliderItem::pMaxValue           { "max-value" };
const juce::Identifier  SliderItem::pInterval           { "interval" };
const juce::Identifier  SliderItem::pModifierSnap       { "modifier-snap" };
const juce::Identifier  SliderItem::pSuffix             { "suffix" };
const juce::Identifier  SliderItem::pSensitivity        { "sensitivity" };
const juce::Identifier  SliderItem::pFilmStrip          { "filmstrip" };
const juce::Identifier  SliderItem::pNumImages          { "num-filmstrip-images" };
const juce::Identifier  SliderItem::pImage              { "image" };
const juce::Identifier  SliderItem::pImageMode          { "image-mode" };
const juce::StringArray SliderItem::pImageModes         { "rotary", "horizontal", "vertical" };
const juce::Identifier  SliderItem::pStartAngle         { "start-angle" };
const juce::Identifier  SliderItem::pDisableScrollWheel { "disable-scroll-wheel" };
const juce::Identifier  SliderItem::pAltKeyHides        { "alt-key-hides" };
const juce::Identifier  SliderItem::pPassMouseClicks    { "pass-mouse-clicks" };
const juce::Identifier  SliderItem::pOverValue          { "over-value" };
const juce::Identifier  SliderItem::pDownValue          { "down-value" };


//==============================================================================

class ComboBoxItem : public GuiItem
{
public:
    FOLEYS_DECLARE_GUI_FACTORY (ComboBoxItem)

    ComboBoxItem (MagicGUIBuilder& builder, const juce::ValueTree& node) : GuiItem (builder, node)
    {
        setColourTranslation (
        {
            { "combo-background", juce::ComboBox::backgroundColourId },
            { "combo-text", juce::ComboBox::textColourId },
            { "combo-outline", juce::ComboBox::outlineColourId },
            { "combo-button", juce::ComboBox::buttonColourId },
            { "combo-arrow", juce::ComboBox::arrowColourId },
            { "combo-focused-outline", juce::ComboBox::focusedOutlineColourId },
            { "combo-menu-background", juce::PopupMenu::backgroundColourId },
            { "combo-menu-background-highlight", juce::PopupMenu::highlightedBackgroundColourId },
            { "combo-menu-text", juce::PopupMenu::textColourId },
            { "combo-menu-text-highlight", juce::PopupMenu::highlightedTextColourId }
        });

        addAndMakeVisible (comboBox);
    }

    void update() override
    {
        attachment.reset();

        auto paramID = configNode.getProperty (IDs::parameter, juce::String()).toString();
        if (paramID.isNotEmpty())
        {
            if (auto* parameter = getMagicState().getParameter (paramID))
            {
                comboBox.clear();
                comboBox.addItemList (parameter->getAllValueStrings(), 1);
                attachment = getMagicState().createAttachment (paramID, comboBox);
            }
        }
    }

    std::vector<SettableProperty> getSettableProperties() const override
    {
        std::vector<SettableProperty> props;
        props.push_back ({ configNode, IDs::parameter, SettableProperty::Choice, {}, magicBuilder.createParameterMenuLambda() , "Parameter to attach to this combo box" });
        return props;
    }

    juce::Component* getWrappedComponent() override
    {
        return &comboBox;
    }

private:
    juce::ComboBox comboBox;
    std::unique_ptr<juce::ComboBoxParameterAttachment> attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ComboBoxItem)
};

//==============================================================================

class TextButtonItem : public GuiItem
{
public:
    FOLEYS_DECLARE_GUI_FACTORY (TextButtonItem)

    static const juce::Identifier pText;
    static const juce::Identifier pProperty;
    static const juce::Identifier pOnClick;

    TextButtonItem (MagicGUIBuilder& builder, const juce::ValueTree& node) : GuiItem (builder, node)
    {
        setColourTranslation (
        {
            { "button-color", juce::TextButton::buttonColourId },
            { "button-on-color", juce::TextButton::buttonOnColourId },
            { "button-off-text", juce::TextButton::textColourOffId },
            { "button-on-text", juce::TextButton::textColourOnId }
        });

        addAndMakeVisible (button);
    }

    void update() override
    {
        attachment.reset();

        auto parameterName = configNode.getProperty (IDs::parameter, juce::String()).toString();
        auto radioValue    = getProperty (IDs::buttonRadioValue);
        auto propertyName  = getProperty (pProperty).toString();

        if (parameterName.isNotEmpty() && radioValue.isVoid())
            attachment = getMagicState().createAttachment (parameterName, button);
        else
            attachment.reset();

        if (propertyName.isNotEmpty())
            property.referTo (getMagicState().getPropertyAsValue (propertyName));

        auto groupID = static_cast<int>(getProperty (IDs::buttonRadioGroup));
        if (groupID > 0)
        {
            button.setRadioGroupId (groupID);
        }

        button.setClickingTogglesState (parameterName.isNotEmpty() || groupID > 0);
        button.setButtonText (magicBuilder.getStyleProperty (pText, configNode));

        auto triggerID = getProperty (pOnClick).toString();
        triggerToCall = triggerID.isNotEmpty() ? getMagicState().getTrigger (triggerID) : nullptr;

        if (propertyName.isNotEmpty())
        {
            button.onClick = [this, radioValue]
            {
                property.setValue (radioValue);

                if (triggerToCall)
                    triggerToCall();
            };
        }
        else
        {
            button.onClick = triggerToCall;
        }

        handler.setRadioGroupValue(radioValue, getMagicState().getParameter(parameterName));
    }

    std::vector<SettableProperty> getSettableProperties() const override
    {
        std::vector<SettableProperty> props;

        props.push_back ({ configNode, IDs::parameter, SettableProperty::Choice, {}, magicBuilder.createParameterMenuLambda() , "Parameter to attach to this button" });
        props.push_back ({ configNode, pText, SettableProperty::Text, {}, {} , "Text label displayed on the button" });
        props.push_back ({ configNode, pProperty, SettableProperty::Choice, {}, magicBuilder.createPropertiesMenuLambda() , "Value to write to when the button is clicked" });
        props.push_back ({ configNode, pOnClick, SettableProperty::Choice, {}, magicBuilder.createTriggerMenuLambda() , "Trigger to fire when the button is clicked" });
        props.push_back ({ configNode, IDs::buttonRadioGroup, SettableProperty::Number, {}, {} , "Radio group ID so only one button in the group is active" });
        props.push_back ({ configNode, IDs::buttonRadioValue, SettableProperty::Number, {}, {} , "Number written to the value output when this radio button is selected" });

        return props;
    }

    juce::Component* getWrappedComponent() override
    {
        return &button;
    }

private:
    juce::TextButton button;
    RadioButtonHandler handler {button, magicBuilder.getRadioButtonManager()};
    std::unique_ptr<juce::ButtonParameterAttachment> attachment;
    std::function<void()> triggerToCall;
    juce::Value property;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TextButtonItem)
};
const juce::Identifier TextButtonItem::pText     { "text" };
const juce::Identifier TextButtonItem::pOnClick  { "onClick" };
const juce::Identifier TextButtonItem::pProperty { "value" };


//==============================================================================

class ToggleButtonItem : public GuiItem
{
public:
    FOLEYS_DECLARE_GUI_FACTORY (ToggleButtonItem)

    static const juce::Identifier pText;
    static const juce::Identifier pProperty;

    ToggleButtonItem (MagicGUIBuilder& builder, const juce::ValueTree& node) : GuiItem (builder, node)
    {
        setColourTranslation (
        {
            { "toggle-text", juce::ToggleButton::textColourId },
            { "toggle-tick", juce::ToggleButton::tickColourId },
            { "toggle-tick-disabled", juce::ToggleButton::tickDisabledColourId }
        });

        addAndMakeVisible (button);
    }

    void update() override
    {
        attachment.reset();

        auto parameterName = configNode.getProperty (IDs::parameter, juce::String()).toString();
        auto radioValue = getProperty (IDs::buttonRadioValue);
        if (parameterName.isNotEmpty() && radioValue.isVoid())
            attachment = getMagicState().createAttachment (parameterName, button);
        else
            attachment.reset();

        button.setButtonText (magicBuilder.getStyleProperty (pText, configNode));

        auto propertyID = getProperty (pProperty).toString();
        if (propertyID.isNotEmpty())
            button.getToggleStateValue().referTo (getMagicState().getPropertyAsValue (propertyID));

        auto groupID = static_cast<int>(getProperty (IDs::buttonRadioGroup));
        if (groupID > 0)
        {
            button.setRadioGroupId (groupID);
            button.setClickingTogglesState (true);
        }
        
        if (getProperty (IDs::buttonRadioValue).toString().isNotEmpty()){
            handler.setRadioGroupValue(radioValue, getMagicState().getParameter(parameterName));
        }
    }

    std::vector<SettableProperty> getSettableProperties() const override
    {
        std::vector<SettableProperty> props;
        props.push_back ({ configNode, pText, SettableProperty::Text, {}, {} , "Text label displayed next to the toggle" });
        props.push_back ({ configNode, IDs::parameter, SettableProperty::Choice, {}, magicBuilder.createParameterMenuLambda() , "Parameter to attach to this toggle button" });
        props.push_back ({ configNode, pProperty, SettableProperty::Choice, {}, magicBuilder.createPropertiesMenuLambda() , "Value to read or write the toggle state" });
        props.push_back ({ configNode, IDs::buttonRadioGroup, SettableProperty::Number, {}, {} , "Radio group ID so only one button in the group is active" });
        props.push_back ({ configNode, IDs::buttonRadioValue, SettableProperty::Number, {}, {} , "Number written to the value output when this radio button is selected" });
        return props;
    }

    juce::Component* getWrappedComponent() override
    {
        return &button;
    }

private:
    juce::ToggleButton button;
    RadioButtonHandler handler {button, magicBuilder.getRadioButtonManager()};
    std::unique_ptr<juce::ButtonParameterAttachment> attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ToggleButtonItem)
};
const juce::Identifier ToggleButtonItem::pText     { "text" };
const juce::Identifier ToggleButtonItem::pProperty { "vaue" };


//==============================================================================

class LabelItem : public GuiItem
{
public:
    FOLEYS_DECLARE_GUI_FACTORY (LabelItem)

    static const juce::Identifier  pText;
    static const juce::Identifier  pJustification;
    static const juce::Identifier  pFontSize;
    static const juce::Identifier  pEditable;
    static const juce::Identifier  pValue;

    LabelItem (MagicGUIBuilder& builder, const juce::ValueTree& node) : GuiItem (builder, node)
    {
        setColourTranslation (
        {
            { "label-background",         juce::Label::backgroundColourId },
            { "label-outline",            juce::Label::outlineColourId },
            { "label-text",               juce::Label::textColourId },
            { "label-editing-background", juce::Label::backgroundWhenEditingColourId },
            { "label-editing-outline",    juce::Label::outlineWhenEditingColourId },
            { "label-editing-text",       juce::Label::textWhenEditingColourId }
        });

        addAndMakeVisible (label);
    }

    void update() override
    {
        label.setText (magicBuilder.getStyleProperty (pText, configNode), juce::dontSendNotification);

        auto justifications = makeJustificationsChoices();
        auto justification = getProperty (pJustification).toString();
        if (justification.isNotEmpty())
            label.setJustificationType (juce::Justification (justifications.getWithDefault (justification, juce::Justification::centredLeft)));
        else
            label.setJustificationType (juce::Justification::centredLeft);

        label.setFont (juce::FontOptions().withHeight (getProperty (pFontSize)));;

        label.setEditable (getProperty (pEditable));

        auto parameterName = getProperty (IDs::parameter).toString();
        if (parameterName.isNotEmpty())
        {
            auto* parameter = getMagicState().getParameter (parameterName);
            if (parameter)
            {
                label.setEditable (true);
                attachment = std::make_unique<juce::ParameterAttachment>(
                    *parameter,
                    [&, parameter](float value)
                    {
                        auto normalised = parameter->convertTo0to1 (value);
                        label.setText (parameter->getText (normalised, 0),
                                       juce::dontSendNotification);
                    });
                label.onTextChange = [&, parameter]
                {
                    auto denormalised = parameter->convertFrom0to1 (parameter->getValueForText (label.getText()));
                    attachment->setValueAsCompleteGesture (denormalised);
                };
                attachment->sendInitialUpdate();
            }
        }

        auto propertyPath = getProperty (pValue).toString();
        if (propertyPath.isNotEmpty())
            label.getTextValue().referTo (getMagicState().getPropertyAsValue (propertyPath));
    }

    std::vector<SettableProperty> getSettableProperties() const override
    {
        std::vector<SettableProperty> props;
        props.push_back ({ configNode, pText, SettableProperty::Text, {}, {} , "Static text content displayed in the label" });
        props.push_back ({ configNode, pJustification, SettableProperty::Choice, {}, magicBuilder.createChoicesMenuLambda (getAllKeyNames (makeJustificationsChoices())) , "Text alignment within the label" });
        props.push_back ({ configNode, pFontSize, SettableProperty::Number, {}, {} , "Font size in pixels" });
        props.push_back ({ configNode, pEditable, SettableProperty::Toggle, {}, {} , "Allow the user to edit the label text by clicking" });
        props.push_back ({ configNode, IDs::parameter, SettableProperty::Choice, {}, magicBuilder.createParameterMenuLambda() , "Parameter to display and edit as text" });
        props.push_back ({ configNode, pValue, SettableProperty::Choice, {}, magicBuilder.createPropertiesMenuLambda() , "Value to read or write the label text" });
        return props;
    }

    juce::Component* getWrappedComponent() override
    {
        return &label;
    }

private:
    juce::Label                                label;
    std::unique_ptr<juce::ParameterAttachment> attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LabelItem)
};
const juce::Identifier  LabelItem::pText            { "text" };
const juce::Identifier  LabelItem::pJustification   { "justification" };
const juce::Identifier  LabelItem::pFontSize        { "font-size" };
const juce::Identifier  LabelItem::pEditable        { "editable" };
const juce::Identifier  LabelItem::pValue           { "value" };

//==============================================================================

class PlotItem : public GuiItem
{
public:
    FOLEYS_DECLARE_GUI_FACTORY (PlotItem)

    static const juce::Identifier  pDecay;
    static const juce::String      pLineWidth;
    static const juce::Identifier  pRoundedCorners;
    static const juce::Identifier  pGradient;
    static const juce::Identifier  pScaled;
    static const juce::Identifier  pAlwaysPlot;
    static const juce::Identifier  pAlwaysPlotHz;
    static const juce::Identifier  pFillUpwards;
    static const juce::Identifier  pFillStyle;
    static const juce::StringArray pFillStyleTypes;

    PlotItem (MagicGUIBuilder& builder, const juce::ValueTree& node) : GuiItem (builder, node)
    {
        setColourTranslation (
        {
            { "plot-color", MagicPlotComponent::plotColourId },
            { "plot-fill-color", MagicPlotComponent::plotFillColourId },
            { "plot-inactive-color", MagicPlotComponent::plotInactiveColourId },
            { "plot-inactive-fill-color", MagicPlotComponent::plotInactiveFillColourId }
        });

        addAndMakeVisible (plot);
    }

    void update() override
    {
        auto sourceID = configNode.getProperty (IDs::source, juce::String()).toString();
        if (sourceID.isNotEmpty())
            plot.setPlotSource (getMagicState().getObjectWithType<MagicPlotSource>(sourceID));

        auto decay = float (getProperty (pDecay));
        plot.setDecayFactor (decay);
        
        juce::String lineWidthString = (getProperty (pLineWidth));
        if (lineWidthString.isNotEmpty()){
            plot.setLineWidth (lineWidthString);
        }
        else{
            plot.setLineWidth ("1.0");
        }

        auto gradient = configNode.getProperty (pGradient, juce::String()).toString();
        plot.setGradientFromString (gradient, magicBuilder.getStylesheet());
        
        // continuously draw the plot?
        
        if (getProperty (pAlwaysPlotHz))
            plot.setAlwaysPlotHz (getProperty (pAlwaysPlotHz));
        else
            plot.setAlwaysPlotHz (30);
        
        if (getProperty (pAlwaysPlot))
            plot.setAlwaysPlot (true);
        else
            plot.setAlwaysPlot (false);
            
        // scale the plot by the width of the line to avoid clipping when using very thick lines?
        
        if (getProperty (pScaled))
            plot.setScaled (true);
        else
            plot.setScaled (false);
        
        if (getProperty (pRoundedCorners))
            plot.setCornerRadius (1.0f);
        else
            plot.setCornerRadius (0.0f);
        
        if (getProperty (IDs::bufferToImage))
            plot.setBufferedToImage (true);
        else
            plot.setBufferedToImage (false);
        
        auto fillStyle = getProperty (pFillStyle).toString();
        if (fillStyle == "downwards")
            plot.setFillStyle(MagicPlotSource::FillStyle::downwards);
        if (fillStyle == "upwards")
            plot.setFillStyle(MagicPlotSource::FillStyle::upwards);
        if (fillStyle == "centre")
            plot.setFillStyle(MagicPlotSource::FillStyle::centre);
    }

    std::vector<SettableProperty> getSettableProperties() const override
    {
        std::vector<SettableProperty> props;
        props.push_back ({ configNode, IDs::source,     SettableProperty::Choice, {}, magicBuilder.createObjectsMenuLambda<MagicPlotSource>() , "Plot data source to visualise" });
        props.push_back ({ configNode, pDecay,          SettableProperty::Number, {}, {} , "Decay factor for smoothing the plot over time" });
        props.push_back ({ configNode, pLineWidth,      SettableProperty::Number, {}, {} , "Width of the plot line in pixels" });
        props.push_back ({ configNode, pRoundedCorners, SettableProperty::Toggle, {}, {} , "Round the corners of the plot path" });
        props.push_back ({ configNode, pGradient,       SettableProperty::Gradient, {}, {} , "Gradient applied to the plot fill area" });
        props.push_back ({ configNode, pScaled,         SettableProperty::Toggle, {}, {}, "Scale the plot inward to prevent thick lines from clipping" });
        props.push_back ({ configNode, pAlwaysPlot,     SettableProperty::Toggle, {}, {}, "Continuously redraw the plot even when data hasn't changed" });
        props.push_back ({ configNode, pAlwaysPlotHz,   SettableProperty::Number, {}, {}, "Refresh rate in Hz for continuous redrawing" });
        props.push_back ({ configNode, IDs::bufferToImage,   SettableProperty::Toggle, {}, {}, "Render the plot to an offscreen image buffer for performance" });
        props.push_back ({ configNode, pFillUpwards,   SettableProperty::Toggle, {}, {}, "Fill the area above the plot line instead of below" });
        props.push_back ({ configNode, pFillStyle, SettableProperty::Choice, {}, magicBuilder.createChoicesMenuLambda (pFillStyleTypes) , "Fill direction: downwards, upwards, or centre" });
        
        return props;
    }

    juce::Component* getWrappedComponent() override
    {
        return &plot;
    }

private:
    MagicPlotComponent plot;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlotItem)
};
const juce::Identifier  PlotItem::pDecay                 {"plot-decay"};
const juce::String      PlotItem::pLineWidth             {"line-width"};
const juce::Identifier  PlotItem::pRoundedCorners        { "rounded-corners" };
const juce::Identifier  PlotItem::pGradient              {"plot-gradient"};
const juce::Identifier  PlotItem::pScaled                { "scaled" };
const juce::Identifier  PlotItem::pAlwaysPlot            { "always-plot" };
const juce::Identifier  PlotItem::pAlwaysPlotHz          { "always-plot-Hz" };
const juce::Identifier  PlotItem::pFillUpwards           { "fill-upwards" };
const juce::Identifier  PlotItem::pFillStyle              { "fill-type" };
const juce::StringArray PlotItem::pFillStyleTypes         { "downwards", "upwards", "centre"};

//==============================================================================

class XYDraggerItem : public GuiItem
{
public:
    FOLEYS_DECLARE_GUI_FACTORY (XYDraggerItem)
    
    static const juce::Identifier  pOutputValueX;
    static const juce::Identifier  pOutputValueXInverted;
    static const juce::Identifier  pOutputValueY;
    static const juce::Identifier  pOutputValueYInverted;
    static const juce::Identifier  pOutputValueZ;
    static const juce::Identifier  pOutputValueZInverted;
    static const juce::Identifier  pTouchedValue;
    static const juce::Identifier  pTouchedIndex;
    static const juce::Identifier  pCrosshair;
    static const juce::StringArray pCrosshairTypes;
    static const juce::Identifier  pRadius;
    static const juce::Identifier  pWheelParameter;
    static const juce::Identifier  pContextParameter;
    static const juce::Identifier  pSenseFactor;
    static const juce::Identifier  pJumpToClick;
    static const juce::Identifier  pDoubleClickResets;
    static const juce::Identifier  pMenuItemHeight;

    XYDraggerItem (MagicGUIBuilder& builder, const juce::ValueTree& node)
      : GuiItem (builder, node)
    {
        setColourTranslation (
        {
            { "xy-drag-handle",                 XYDragComponent::xyDotColourId },
            { "xy-drag-handle-ring",            XYDragComponent::xyDotRingColourId },
            { "xy-drag-handle-outer-ring",      XYDragComponent::xyDotOuterRingColourId },
            { "xy-drag-handle-selected",        XYDragComponent::xyDotSelectedColourId },
            { "xy-drag-handle-over",            XYDragComponent::xyDotOverColourId },
            { "xy-horizontal",                  XYDragComponent::xyHorizontalColourId },
            { "xy-horizontal-over",             XYDragComponent::xyHorizontalOverColourId },
            { "xy-vertical",                    XYDragComponent::xyVerticalColourId },
            { "xy-vertical-over",               XYDragComponent::xyVerticalOverColourId }
        });

        addAndMakeVisible (dragger);
        valueX.addListener (this);
        valueY.addListener (this);
        valueZ.addListener (this);
    }

    void update() override
    {
        auto xParamID = configNode.getProperty (IDs::parameterX, juce::String()).toString();
        if (xParamID.isNotEmpty())
            dragger.setParameterX (dynamic_cast<juce::RangedAudioParameter*>(getMagicState().getParameter (xParamID)));
        else
            dragger.setParameterX (nullptr);

        auto yParamID = configNode.getProperty (IDs::parameterY, juce::String()).toString();
        if (yParamID.isNotEmpty())
            dragger.setParameterY (dynamic_cast<juce::RangedAudioParameter*>(getMagicState().getParameter (yParamID)));
        else
            dragger.setParameterY (nullptr);

        auto zParamID = configNode.getProperty (IDs::parameterZ, juce::String()).toString();
        if (zParamID.isNotEmpty())
            dragger.setParameterZ (dynamic_cast<juce::RangedAudioParameter*>(getMagicState().getParameter (zParamID)));
        else
            dragger.setParameterZ (nullptr);

        auto rightParamID = configNode.getProperty (pContextParameter, juce::String()).toString();
        if (rightParamID.isNotEmpty())
            dragger.setRightClickParameter (dynamic_cast<juce::RangedAudioParameter*>(getMagicState().getParameter (rightParamID)));

        auto wheelParamID = configNode.getProperty (pWheelParameter, juce::String()).toString();
        if (wheelParamID.isNotEmpty())
            dragger.setWheelParameter (dynamic_cast<juce::RangedAudioParameter*>(getMagicState().getParameter (wheelParamID)));

        auto crosshair = getProperty (pCrosshair);
        if (crosshair == pCrosshairTypes [0])
            dragger.setCrossHair (false, false);
        else if (crosshair == pCrosshairTypes [1])
            dragger.setCrossHair (true, false);
        else if (crosshair == pCrosshairTypes [2])
            dragger.setCrossHair (false, true);
        else
            dragger.setCrossHair (true, true);

        auto radius = getProperty (pRadius);
        if (! radius.isVoid())
            dragger.setRadius (radius);

        auto itemHeight = getProperty (pMenuItemHeight);
        if (! itemHeight.isVoid())
            dragger.setMenuItemHeight (itemHeight);

        auto factor = getProperty (pSenseFactor);
        if (! factor.isVoid())
            dragger.setSenseFactor (factor);

        auto jumpToClick = getProperty (pJumpToClick);
        if (! jumpToClick.isVoid())
            dragger.setJumpToClick (jumpToClick);
        
        auto doubleClickResets = getProperty (pDoubleClickResets);
        if (! doubleClickResets.isVoid())
            dragger.setDoubleClickResets (doubleClickResets);
        
        // register the properties
        
        juce::String propertyID;
        
        propertyID = getProperty (pOutputValueX).toString();
        if (propertyID.isNotEmpty())
            getMagicState().getPropertyAsValue (propertyID);
        
        propertyID = getProperty (pOutputValueXInverted).toString();
        if (propertyID.isNotEmpty())
            getMagicState().getPropertyAsValue (propertyID);
        
        propertyID = getProperty (pOutputValueY).toString();
        if (propertyID.isNotEmpty())
            getMagicState().getPropertyAsValue (propertyID);
        
        propertyID = getProperty (pOutputValueYInverted).toString();
        if (propertyID.isNotEmpty())
            getMagicState().getPropertyAsValue (propertyID);
        
        propertyID = getProperty (pOutputValueZ).toString();
        if (propertyID.isNotEmpty())
            getMagicState().getPropertyAsValue (propertyID);
        
        propertyID = getProperty (pOutputValueZInverted).toString();
        if (propertyID.isNotEmpty())
            getMagicState().getPropertyAsValue (propertyID);
        
        dragger.referValueX (valueX);
        dragger.referValueY (valueY);
        dragger.referValueZ (valueZ);
        
        propertyID = getProperty (pTouchedValue).toString();
        if (propertyID.isNotEmpty()){
            dragger.setTouchedIndex(getProperty (pTouchedIndex));
            valueTouched.referTo(getMagicState().getPropertyAsValue (propertyID));
            dragger.referTouched(valueTouched);
        }
    }

    std::vector<SettableProperty> getSettableProperties() const override
    {
        std::vector<SettableProperty> props;

        props.push_back ({ configNode, IDs::parameterX, SettableProperty::Choice, {}, magicBuilder.createParameterMenuLambda() , "Parameter controlled by horizontal drag" });
        props.push_back ({ configNode, IDs::parameterY, SettableProperty::Choice, {}, magicBuilder.createParameterMenuLambda() , "Parameter controlled by vertical drag" });
        props.push_back ({ configNode, IDs::parameterZ, SettableProperty::Choice, {}, magicBuilder.createParameterMenuLambda() , "Parameter controlled by the Z axis (mouse wheel)" });
        props.push_back ({ configNode, pOutputValueX, foleys::SettableProperty::Choice, {}, magicBuilder.createPropertiesMenuLambda() , "Value to write the normalised X position to" });
        props.push_back ({ configNode, pOutputValueXInverted, foleys::SettableProperty::Choice, {}, magicBuilder.createPropertiesMenuLambda() , "Value to write the inverted X position to" });
        props.push_back ({ configNode, pOutputValueY, foleys::SettableProperty::Choice, {}, magicBuilder.createPropertiesMenuLambda() , "Value to write the normalised Y position to" });
        props.push_back ({ configNode, pOutputValueYInverted, foleys::SettableProperty::Choice, {}, magicBuilder.createPropertiesMenuLambda() , "Value to write the inverted Y position to" });
        props.push_back ({ configNode, pOutputValueZ, foleys::SettableProperty::Choice, {}, magicBuilder.createPropertiesMenuLambda() , "Value to write the normalised Z position to" });
        props.push_back ({ configNode, pOutputValueZInverted, foleys::SettableProperty::Choice, {}, magicBuilder.createPropertiesMenuLambda() , "Value to write the inverted Z position to" });
        props.push_back ({ configNode, pTouchedValue, foleys::SettableProperty::Choice, {}, magicBuilder.createPropertiesMenuLambda() , "Value set to 1 while a drag handle is being touched" });
        props.push_back ({ configNode, pTouchedIndex, SettableProperty::Number, {}, {}, "Index of the drag handle that reports touch state" });
        props.push_back ({ configNode, pContextParameter, SettableProperty::Choice, {}, magicBuilder.createParameterMenuLambda() , "Parameter controlled by right-click drag" });
        props.push_back ({ configNode, pWheelParameter, SettableProperty::Choice, {}, magicBuilder.createParameterMenuLambda() , "Parameter controlled by mouse wheel" });
        props.push_back ({ configNode, pCrosshair, SettableProperty::Choice, {}, magicBuilder.createChoicesMenuLambda (pCrosshairTypes) , "Crosshair style: none, vertical, horizontal, or both" });
        props.push_back ({ configNode, pRadius, SettableProperty::Number, {}, {}, "Radius of the drag handle in pixels" });
        props.push_back ({ configNode, pSenseFactor, SettableProperty::Number, {}, {}, "Drag sensitivity multiplier" });
        props.push_back ({ configNode, pJumpToClick, SettableProperty::Toggle, {}, {}, "Handle jumps directly to the click position" });
        props.push_back ({ configNode, pDoubleClickResets, SettableProperty::Toggle, {}, {}, "Double-click resets the handle to its default position" });
        props.push_back ({ configNode, pMenuItemHeight, SettableProperty::Number, {}, {}, "Height of items in the right-click context menu" });

        return props;
    }

    juce::Component* getWrappedComponent() override
    {
        return &dragger;
    }

private:
    XYDragComponent dragger;
    juce::Value valueX;
    juce::Value valueY;
    juce::Value valueZ;
    juce::Value valueTouched;
    
    void valueChanged (juce::Value& source) override
    {
        if (source == valueX){
            auto outputValueID = getProperty (pOutputValueX).toString();
            float value = source.getValue();
            value = juce::jlimit (0.0f, 1.0f, value);
            if (outputValueID.isNotEmpty())
                getMagicState().getPropertyAsValue (outputValueID).setValue (value);
            
            auto outputValueInvertedID = getProperty (pOutputValueXInverted).toString();
            if (outputValueInvertedID.isNotEmpty())
                getMagicState().getPropertyAsValue (outputValueInvertedID).setValue (1.0f - value);
        }
        
        if (source == valueY){
            auto outputValueID = getProperty (pOutputValueY).toString();
            float value = source.getValue();
            value = juce::jlimit (0.0f, 1.0f, value);
            if (outputValueID.isNotEmpty())
                getMagicState().getPropertyAsValue (outputValueID).setValue (value);
            
            auto outputValueInvertedID = getProperty (pOutputValueYInverted).toString();
            if (outputValueInvertedID.isNotEmpty())
                getMagicState().getPropertyAsValue (outputValueInvertedID).setValue (1.0f - value);
        }
        
        if (source == valueZ){
            auto outputValueID = getProperty (pOutputValueZ).toString();
            float value = source.getValue();
            value = juce::jlimit (0.0f, 1.0f, value);
            if (outputValueID.isNotEmpty())
                getMagicState().getPropertyAsValue (outputValueID).setValue (value);
            
            auto outputValueInvertedID = getProperty (pOutputValueZInverted).toString();
            if (outputValueInvertedID.isNotEmpty())
                getMagicState().getPropertyAsValue (outputValueInvertedID).setValue (1.0f - value);
        }
        
        handleValueChanged (source);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (XYDraggerItem)
};

const juce::Identifier  XYDraggerItem::pOutputValueX     { "output-value-x" };
const juce::Identifier  XYDraggerItem::pOutputValueXInverted  { "output-value-inverted-x" };
const juce::Identifier  XYDraggerItem::pOutputValueY     { "output-value-y" };
const juce::Identifier  XYDraggerItem::pOutputValueYInverted  { "output-value-inverted-y" };
const juce::Identifier  XYDraggerItem::pOutputValueZ     { "output-value-z" };
const juce::Identifier  XYDraggerItem::pOutputValueZInverted  { "output-value-inverted-z" };
const juce::Identifier  XYDraggerItem::pCrosshair       { "xy-crosshair" };
const juce::StringArray XYDraggerItem::pCrosshairTypes  { "no-crosshair", "vertical", "horizontal", "crosshair" };
const juce::Identifier  XYDraggerItem::pRadius          { "xy-radius" };
const juce::Identifier  XYDraggerItem::pWheelParameter  { "wheel-parameter" };
const juce::Identifier  XYDraggerItem::pContextParameter { "right-click" };
const juce::Identifier  XYDraggerItem::pSenseFactor     { "xy-sense-factor" };
const juce::Identifier  XYDraggerItem::pJumpToClick     { "xy-jump-to-click" };
const juce::Identifier  XYDraggerItem::pDoubleClickResets     { "double-click-resets" };
const juce::Identifier  XYDraggerItem::pMenuItemHeight        { "menu-item-height" };
const juce::Identifier  XYDraggerItem::pTouchedValue          { "touched-value" };
const juce::Identifier  XYDraggerItem::pTouchedIndex          { "touched-index" };

//==============================================================================

class KeyboardItem : public GuiItem
{
public:
    FOLEYS_DECLARE_GUI_FACTORY (KeyboardItem)
    
    static const juce::String      pKeyWidth;

    KeyboardItem (MagicGUIBuilder& builder, const juce::ValueTree& node)
      : GuiItem (builder, node),
        keyboard (getMagicState().getKeyboardState(), juce::MidiKeyboardComponent::horizontalKeyboard)
    {
        setColourTranslation (
        {
            { "white-note-color",      juce::MidiKeyboardComponent::whiteNoteColourId },
            { "black-note-color",      juce::MidiKeyboardComponent::blackNoteColourId },
            { "key-separator-line-color", juce::MidiKeyboardComponent::keySeparatorLineColourId },
            { "mouse-over-color",      juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId },
            { "key-down-color",        juce::MidiKeyboardComponent::keyDownOverlayColourId },
            { "text-label-color",      juce::MidiKeyboardComponent::textLabelColourId },
        });

        addAndMakeVisible (keyboard);
    }

    void update() override
    {
        auto size = float (getProperty (pKeyWidth));
        if (!size)
            size = 50.0f;
        keyboard.setKeyWidth (size);
        
#if JUCE_IOS
        keyboard.setScrollButtonWidth(26);
#else
        keyboard.setScrollButtonWidth(12);
#endif

        auto orientation = getProperty ("orientation").toString();
        if (orientation == "vertical-left")
            keyboard.setOrientation (juce::MidiKeyboardComponent::verticalKeyboardFacingLeft);
        else if (orientation == "vertical-right")
            keyboard.setOrientation (juce::MidiKeyboardComponent::verticalKeyboardFacingRight);
        else
            keyboard.setOrientation (juce::MidiKeyboardComponent::horizontalKeyboard);
    }
    
    std::vector<SettableProperty> getSettableProperties() const override
    {
        std::vector<SettableProperty> props;
        props.push_back ({ configNode, pKeyWidth,      SettableProperty::Number, {}, {} , "Width of each white key in pixels" });
        
        return props;
    }

    juce::Component* getWrappedComponent() override
    {
        return &keyboard;
    }

private:
    juce::MidiKeyboardComponent keyboard;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KeyboardItem)
};
const juce::String      KeyboardItem::pKeyWidth             {"key-width"};

//==============================================================================

class DrumpadItem : public GuiItem
                  , private juce::Timer
{
public:
    static const juce::Identifier  pColumns;
    static const juce::Identifier  pRows;
    static const juce::Identifier  pRootNote;
    static const juce::Identifier  pDownValue_1;
    static const juce::Identifier  pDownValue_2;
    static const juce::Identifier  pDownValue_3;
    static const juce::Identifier  pDownValue_4;
    static const juce::Identifier  pDownValue_5;
    static const juce::Identifier  pDownValue_6;
    static const juce::Identifier  pDownValue_7;
    static const juce::Identifier  pDownValue_8;
    static const juce::Identifier  pDownValue_9;
    static const juce::Identifier  pDownValue_10;
    static const juce::Identifier  pDownValue_11;
    static const juce::Identifier  pDownValue_12;
    static const juce::Identifier  pDownValue_13;
    static const juce::Identifier  pDownValue_14;
    static const juce::Identifier  pDownValue_15;
    static const juce::Identifier  pDownValue_16;
    static const juce::Identifier  pLastPadValue;
    static const juce::Identifier  pFollowsClicked;
    static const juce::Identifier  pFollowsPlayed;
    static const juce::Identifier  pMPCStylePads; // When enabled MIDI Notes are ordered starting from the lowest row of pads
    static const juce::Identifier  pInvisible; // When enabled MIDI Notes are ordered starting from the lowest row of pads

    FOLEYS_DECLARE_GUI_FACTORY (DrumpadItem)

    DrumpadItem (MagicGUIBuilder& builder, const juce::ValueTree& node)
      : GuiItem (builder, node),
        drumpad (getMagicState().getKeyboardState())
    {
        setColourTranslation (
        {
            { "drumpad-background",   MidiDrumpadComponent::background },
            { "drumpad-fill",         MidiDrumpadComponent::padFill },
            { "drumpad-outline",      MidiDrumpadComponent::padOutline },
            { "drumpad-down-fill",    MidiDrumpadComponent::padDownFill },
            { "drumpad-down-outline", MidiDrumpadComponent::padDownOutline },
            { "drumpad-touch",        MidiDrumpadComponent::touch },
        });

        addAndMakeVisible (drumpad);
    }

    void update() override
    {
        auto rowsVar    = getProperty (pRows);
        auto columnsVar = getProperty (pColumns);

        auto rows = rowsVar.isVoid() ? 3 : int (rowsVar);
        auto columns = columnsVar.isVoid() ? 3 : int (columnsVar);
        
        if (getProperty (pMPCStylePads))
            drumpad.setMPCStylePads(true);
        else
            drumpad.setMPCStylePads(false);
           
        if (getProperty (pInvisible))
            drumpad.setInvisible(true);
        else
            drumpad.setInvisible(false);
        
        drumpad.setMatrix (rows, columns);

        auto rootNote = getProperty (pRootNote);
        if (!rootNote.isVoid())
            drumpad.setRootNote (rootNote);
        
        juce::String valueID;
        
        valueID = configNode.getProperty (pDownValue_1, juce::String()).toString();
        if (valueID.isNotEmpty())
            downValue_1.referTo (getMagicState().getPropertyAsValue (valueID));
        
        valueID = configNode.getProperty (pDownValue_2, juce::String()).toString();
        if (valueID.isNotEmpty())
            downValue_2.referTo (getMagicState().getPropertyAsValue (valueID));
        
        valueID = configNode.getProperty (pDownValue_3, juce::String()).toString();
        if (valueID.isNotEmpty())
            downValue_3.referTo (getMagicState().getPropertyAsValue (valueID));
        
        valueID = configNode.getProperty (pDownValue_4, juce::String()).toString();
        if (valueID.isNotEmpty())
            downValue_4.referTo (getMagicState().getPropertyAsValue (valueID));
                
        valueID = configNode.getProperty (pDownValue_5, juce::String()).toString();
        if (valueID.isNotEmpty())
            downValue_5.referTo (getMagicState().getPropertyAsValue (valueID));
        
        valueID = configNode.getProperty (pDownValue_6, juce::String()).toString();
        if (valueID.isNotEmpty())
            downValue_6.referTo (getMagicState().getPropertyAsValue (valueID));
        
        valueID = configNode.getProperty (pDownValue_7, juce::String()).toString();
        if (valueID.isNotEmpty())
            downValue_7.referTo (getMagicState().getPropertyAsValue (valueID));
        
        valueID = configNode.getProperty (pDownValue_8, juce::String()).toString();
        if (valueID.isNotEmpty())
            downValue_8.referTo (getMagicState().getPropertyAsValue (valueID));
                
        valueID = configNode.getProperty (pDownValue_9, juce::String()).toString();
        if (valueID.isNotEmpty())
            downValue_9.referTo (getMagicState().getPropertyAsValue (valueID));
        
        valueID = configNode.getProperty (pDownValue_10, juce::String()).toString();
        if (valueID.isNotEmpty())
            downValue_10.referTo (getMagicState().getPropertyAsValue (valueID));
        
        valueID = configNode.getProperty (pDownValue_11, juce::String()).toString();
        if (valueID.isNotEmpty())
            downValue_11.referTo (getMagicState().getPropertyAsValue (valueID));
        
        valueID = configNode.getProperty (pDownValue_12, juce::String()).toString();
        if (valueID.isNotEmpty())
            downValue_12.referTo (getMagicState().getPropertyAsValue (valueID));
                
        valueID = configNode.getProperty (pDownValue_13, juce::String()).toString();
        if (valueID.isNotEmpty())
            downValue_13.referTo (getMagicState().getPropertyAsValue (valueID));
        
        valueID = configNode.getProperty (pDownValue_14, juce::String()).toString();
        if (valueID.isNotEmpty())
            downValue_14.referTo (getMagicState().getPropertyAsValue (valueID));
        
        valueID = configNode.getProperty (pDownValue_15, juce::String()).toString();
        if (valueID.isNotEmpty())
            downValue_15.referTo (getMagicState().getPropertyAsValue (valueID));
        
        valueID = configNode.getProperty (pDownValue_16, juce::String()).toString();
        if (valueID.isNotEmpty())
            downValue_16.referTo (getMagicState().getPropertyAsValue (valueID));
        
        
        
        valueID = configNode.getProperty (pLastPadValue, juce::String()).toString();
        if (valueID.isNotEmpty())
            lastPadValue.referTo (getMagicState().getPropertyAsValue (valueID));
        
        valueID = configNode.getProperty (pFollowsClicked, juce::String()).toString();
        if (valueID.isNotEmpty())
            followsClickedValue.referTo (getMagicState().getPropertyAsValue (valueID));
        else
            followsClickedValue = 1;
        
        valueID = configNode.getProperty (pFollowsPlayed, juce::String()).toString();
        if (valueID.isNotEmpty())
            followsPlayedValue.referTo (getMagicState().getPropertyAsValue (valueID));
        else
            followsPlayedValue = 0;
        
        drumpad.setWantsKeyboardFocus(false);
        drumpad.setMouseClickGrabsKeyboardFocus(false);

        startTimerHz(60);
    }

    std::vector<SettableProperty> getSettableProperties() const override
    {
        std::vector<SettableProperty> props;
        props.push_back ({ configNode, pColumns,  SettableProperty::Number,  3, {}, "Number of pad columns in the grid" });
        props.push_back ({ configNode, pRows,     SettableProperty::Number,  3, {}, "Number of pad rows in the grid" });
        props.push_back ({ configNode, pRootNote, SettableProperty::Number, 64, {}, "MIDI note number for the first pad" });
        props.push_back ({ configNode, pDownValue_1, SettableProperty::Choice, 1.0f, magicBuilder.createPropertiesMenuLambda() , "Value set to 1 while pad 1 is held down" });
        props.push_back ({ configNode, pDownValue_2, SettableProperty::Choice, 1.0f, magicBuilder.createPropertiesMenuLambda() , "Value set to 1 while pad 2 is held down" });
        props.push_back ({ configNode, pDownValue_3, SettableProperty::Choice, 1.0f, magicBuilder.createPropertiesMenuLambda() , "Value set to 1 while pad 3 is held down" });
        props.push_back ({ configNode, pDownValue_4, SettableProperty::Choice, 1.0f, magicBuilder.createPropertiesMenuLambda() , "Value set to 1 while pad 4 is held down" });
        props.push_back ({ configNode, pDownValue_5, SettableProperty::Choice, 1.0f, magicBuilder.createPropertiesMenuLambda() , "Value set to 1 while pad 5 is held down" });
        props.push_back ({ configNode, pDownValue_6, SettableProperty::Choice, 1.0f, magicBuilder.createPropertiesMenuLambda() , "Value set to 1 while pad 6 is held down" });
        props.push_back ({ configNode, pDownValue_7, SettableProperty::Choice, 1.0f, magicBuilder.createPropertiesMenuLambda() , "Value set to 1 while pad 7 is held down" });
        props.push_back ({ configNode, pDownValue_8, SettableProperty::Choice, 1.0f, magicBuilder.createPropertiesMenuLambda() , "Value set to 1 while pad 8 is held down" });
        props.push_back ({ configNode, pDownValue_9, SettableProperty::Choice, 1.0f, magicBuilder.createPropertiesMenuLambda() , "Value set to 1 while pad 9 is held down" });
        props.push_back ({ configNode, pDownValue_10, SettableProperty::Choice, 1.0f, magicBuilder.createPropertiesMenuLambda() , "Value set to 1 while pad 10 is held down" });
        props.push_back ({ configNode, pDownValue_11, SettableProperty::Choice, 1.0f, magicBuilder.createPropertiesMenuLambda() , "Value set to 1 while pad 11 is held down" });
        props.push_back ({ configNode, pDownValue_12, SettableProperty::Choice, 1.0f, magicBuilder.createPropertiesMenuLambda() , "Value set to 1 while pad 12 is held down" });
        props.push_back ({ configNode, pDownValue_13, SettableProperty::Choice, 1.0f, magicBuilder.createPropertiesMenuLambda() , "Value set to 1 while pad 13 is held down" });
        props.push_back ({ configNode, pDownValue_14, SettableProperty::Choice, 1.0f, magicBuilder.createPropertiesMenuLambda() , "Value set to 1 while pad 14 is held down" });
        props.push_back ({ configNode, pDownValue_15, SettableProperty::Choice, 1.0f, magicBuilder.createPropertiesMenuLambda() , "Value set to 1 while pad 15 is held down" });
        props.push_back ({ configNode, pDownValue_16, SettableProperty::Choice, 1.0f, magicBuilder.createPropertiesMenuLambda() , "Value set to 1 while pad 16 is held down" });
        props.push_back ({ configNode, pLastPadValue, SettableProperty::Choice, 1.0f, magicBuilder.createPropertiesMenuLambda() , "Value updated with the note number of the last pad triggered" });
        props.push_back ({ configNode, pFollowsClicked, SettableProperty::Choice, 1.0f, magicBuilder.createPropertiesMenuLambda() , "Value that enables tracking of mouse-clicked pads" });
        props.push_back ({ configNode, pFollowsPlayed, SettableProperty::Choice, 1.0f, magicBuilder.createPropertiesMenuLambda() , "Value that enables tracking of MIDI-played pads" });
        props.push_back ({ configNode, pMPCStylePads, foleys::SettableProperty::Toggle, {}, {}, "Order MIDI notes starting from the bottom row (MPC layout)" });
        props.push_back ({ configNode, pInvisible, foleys::SettableProperty::Toggle, {}, {}, "Hide the drum pads visually while keeping them functional" });
        
        return props;
    }

    juce::Component* getWrappedComponent() override
    {
        return &drumpad;
    }

private:
    MidiDrumpadComponent drumpad;
    juce::Value downValue_1, downValue_2, downValue_3, downValue_4, downValue_5, downValue_6, downValue_7, downValue_8, downValue_9, downValue_10, downValue_11, downValue_12, downValue_13, downValue_14, downValue_15, downValue_16, lastPadValue, followsClickedValue, followsPlayedValue;
    
    void timerCallback() final
    {
        if (followsClickedValue.getValue() && drumpad.getClickedFlag())
            lastPadValue = drumpad.getLastClicked();
        
        if (followsPlayedValue.getValue() && drumpad.getPlayedFlag())
            lastPadValue = drumpad.getLastPlayed();
        
        downValue_1 = drumpad.getPadDownStatus(0);
        downValue_2 = drumpad.getPadDownStatus(1);
        downValue_3 = drumpad.getPadDownStatus(2);
        downValue_4 = drumpad.getPadDownStatus(3);
        downValue_5 = drumpad.getPadDownStatus(4);
        downValue_6 = drumpad.getPadDownStatus(5);
        downValue_7 = drumpad.getPadDownStatus(6);
        downValue_8 = drumpad.getPadDownStatus(7);
        downValue_9 = drumpad.getPadDownStatus(8);
        downValue_10 = drumpad.getPadDownStatus(9);
        downValue_11 = drumpad.getPadDownStatus(10);
        downValue_12 = drumpad.getPadDownStatus(11);
        downValue_13 = drumpad.getPadDownStatus(12);
        downValue_14 = drumpad.getPadDownStatus(13);
        downValue_15 = drumpad.getPadDownStatus(14);
        downValue_16 = drumpad.getPadDownStatus(15);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrumpadItem)
};
const juce::Identifier  DrumpadItem::pColumns       { "pad-columns" };
const juce::Identifier  DrumpadItem::pRows          { "pad-rows" };
const juce::Identifier  DrumpadItem::pRootNote      { "pad-root-note" };
const juce::Identifier  DrumpadItem::pDownValue_1   { "value-1" };
const juce::Identifier  DrumpadItem::pDownValue_2   { "value-2" };
const juce::Identifier  DrumpadItem::pDownValue_3   { "value-3" };
const juce::Identifier  DrumpadItem::pDownValue_4   { "value-4" };
const juce::Identifier  DrumpadItem::pDownValue_5   { "value-5" };
const juce::Identifier  DrumpadItem::pDownValue_6   { "value-6" };
const juce::Identifier  DrumpadItem::pDownValue_7   { "value-7" };
const juce::Identifier  DrumpadItem::pDownValue_8   { "value-8" };
const juce::Identifier  DrumpadItem::pDownValue_9   { "value-9" };
const juce::Identifier  DrumpadItem::pDownValue_10   { "value-10" };
const juce::Identifier  DrumpadItem::pDownValue_11   { "value-11" };
const juce::Identifier  DrumpadItem::pDownValue_12   { "value-12" };
const juce::Identifier  DrumpadItem::pDownValue_13   { "value-13" };
const juce::Identifier  DrumpadItem::pDownValue_14   { "value-14" };
const juce::Identifier  DrumpadItem::pDownValue_15   { "value-15" };
const juce::Identifier  DrumpadItem::pDownValue_16   { "value-16" };
const juce::Identifier  DrumpadItem::pLastPadValue   { "last-pad-value" };
const juce::Identifier  DrumpadItem::pFollowsClicked    { "follows-clicked-value" };
const juce::Identifier  DrumpadItem::pFollowsPlayed     { "follows-played-value" };
const juce::Identifier  DrumpadItem::pMPCStylePads      { "mpc-style-pads" };
const juce::Identifier  DrumpadItem::pInvisible         { "invisible" };


//==============================================================================

class LevelMeterItem : public GuiItem
{
public:
    FOLEYS_DECLARE_GUI_FACTORY (LevelMeterItem)
    
    static const juce::Identifier  pBarCorner;
    static const juce::Identifier  pPeakLineThickness;
    static const juce::Identifier  pHorizontalFlip;
    static const juce::Identifier  pVerticalFlip;
    static const juce::Identifier  pRefreshRateHz;

    LevelMeterItem (MagicGUIBuilder& builder, const juce::ValueTree& node) : GuiItem (builder, node)
    {
        setColourTranslation (
        {
            { "background-color", MagicLevelMeter::backgroundColourId },
            { "bar-background-color", MagicLevelMeter::barBackgroundColourId },
            { "outline-color", MagicLevelMeter::outlineColourId },
            { "bar-fill-color", MagicLevelMeter::barFillColourId },
            { "tickmark-color", MagicLevelMeter::tickmarkColourId },
        });

        addAndMakeVisible (meter);
    }

    void update() override
    {
        auto sourceID = configNode.getProperty (IDs::source, juce::String()).toString();
        if (sourceID.isNotEmpty())
            meter.setLevelSource (getMagicState().getObjectWithType<MagicLevelSource>(sourceID));
        
        if (float corner = getProperty (pBarCorner))
            meter.setBarCorner(corner);
        else
            meter.setBarCorner(0);
        
        if (float thickness = getProperty (pPeakLineThickness))
            meter.setPeakLineThickness(thickness);
        else
            meter.setPeakLineThickness(1);
        
        if (getProperty (pHorizontalFlip))
            meter.setHorizontalFlip(true);
        else
            meter.setHorizontalFlip(false);
        
        if (getProperty (pVerticalFlip))
            meter.setVerticalFlip(true);
        else
            meter.setVerticalFlip(false);
        
        if (getProperty (pRefreshRateHz).toString().isNotEmpty())
            meter.setRefreshRateHz (getProperty (pRefreshRateHz));
        else
            meter.setRefreshRateHz (60);
        
        meter.setInterceptsMouseClicks(false, false);
    }

    std::vector<SettableProperty> getSettableProperties() const override
    {
        std::vector<SettableProperty> props;
        props.push_back ({ configNode, IDs::source, SettableProperty::Choice, {}, magicBuilder.createObjectsMenuLambda<MagicLevelSource>() , "Level meter data source to display" });
        props.push_back ({ configNode, pBarCorner, foleys::SettableProperty::Number, {}, {} , "Corner radius for the meter bars in pixels" });
        props.push_back ({ configNode, pPeakLineThickness, foleys::SettableProperty::Number, {}, {} , "Thickness of the peak level indicator line" });
        props.push_back ({ configNode, pHorizontalFlip, foleys::SettableProperty::Toggle, {}, {}, "Mirror the meter display horizontally" });
        props.push_back ({ configNode, pVerticalFlip, foleys::SettableProperty::Toggle, {}, {}, "Mirror the meter display vertically" });
        props.push_back ({ configNode, pRefreshRateHz,   SettableProperty::Number, {}, {}, "Refresh rate for the meter display in Hz" });
        return props;
    }

    juce::Component* getWrappedComponent() override
    {
        return &meter;
    }

private:
    MagicLevelMeter meter;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LevelMeterItem)
};
const juce::Identifier  LevelMeterItem::pBarCorner         { "bar-corner" };
const juce::Identifier  LevelMeterItem::pPeakLineThickness { "peak-line-thickness" };
const juce::Identifier  LevelMeterItem::pHorizontalFlip    { "horizontal-flip" };
const juce::Identifier  LevelMeterItem::pVerticalFlip      { "vertical-flip" };
const juce::Identifier  LevelMeterItem::pRefreshRateHz     { "refresh-rate-Hz" };

//==============================================================================

class MidiLearnItem : public GuiItem
{
public:
    FOLEYS_DECLARE_GUI_FACTORY (MidiLearnItem)

    MidiLearnItem (MagicGUIBuilder& builder, const juce::ValueTree& node) : GuiItem (builder, node)
    {
        if (auto* state = dynamic_cast<MagicProcessorState*>(&builder.getMagicState()))
            midiLearn.setMagicProcessorState (state);

        addAndMakeVisible (midiLearn);
    }

    void update() override {}

    juce::Component* getWrappedComponent() override
    {
        return &midiLearn;
    }

private:
    MidiLearnComponent midiLearn;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiLearnItem)
};

//==============================================================================

class ListBoxItem : public GuiItem,
                    public juce::ChangeListener
{
public:
    FOLEYS_DECLARE_GUI_FACTORY (ListBoxItem)

    ListBoxItem (MagicGUIBuilder& builder, const juce::ValueTree& node) : GuiItem (builder, node)
    {
        addAndMakeVisible (listBox);
    }

    ~ListBoxItem() override
    {
        if (auto* m = dynamic_cast<juce::ChangeBroadcaster*>(listBox.getModel()))
            m->removeChangeListener (this);
    }

    void update() override
    {
        if (auto* m = dynamic_cast<juce::ChangeBroadcaster*>(listBox.getModel()))
            m->removeChangeListener (this);

        auto modelID = configNode.getProperty ("list-box-model", juce::String()).toString();
        if (modelID.isNotEmpty())
        {
            if (auto* model = getMagicState().getObjectWithType<juce::ListBoxModel>(modelID))
            {
                listBox.setModel (model);
                if (auto* m = dynamic_cast<juce::ChangeBroadcaster*>(model))
                    m->addChangeListener (this);
            }
        }
        else
        {
            listBox.setModel (nullptr);
        }
    }

    std::vector<SettableProperty> getSettableProperties() const override
    {
        std::vector<SettableProperty> props;
        props.push_back ({ configNode, "list-box-model", SettableProperty::Choice, {}, magicBuilder.createObjectsMenuLambda<juce::ListBoxModel>() , "Data model that provides the list box content" });
        return props;
    }

    juce::Component* getWrappedComponent() override
    {
        return &listBox;
    }

    void changeListenerCallback (juce::ChangeBroadcaster*) override
    {
        listBox.updateContent();
    }

private:
    juce::ListBox listBox;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ListBoxItem)
};

//==============================================================================

#if JUCE_MODULE_AVAILABLE_juce_gui_extra && JUCE_WEB_BROWSER
class WebBrowserItem : public GuiItem
{
public:
    FOLEYS_DECLARE_GUI_FACTORY (WebBrowserItem)
    
    static const juce::Identifier  pUrl;

    WebBrowserItem (MagicGUIBuilder& builder, const juce::ValueTree& node) : GuiItem (builder, node)
    {
        addAndMakeVisible (browser);
    }

    void update() override
    {
        browser.goToURL (getProperty (pUrl).toString());
    }

    std::vector<SettableProperty> getSettableProperties() const override
    {
        std::vector<SettableProperty> props;
        props.push_back ({ configNode, pUrl, SettableProperty::Text, {}, {} , "URL to load in the embedded web browser" });
        return props;
    }

    juce::Component* getWrappedComponent() override
    {
        return &browser;
    }

private:
    juce::WebBrowserComponent browser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WebBrowserItem)
};
const juce::Identifier  WebBrowserItem::pUrl            { "url" };

#endif // JUCE_WEB_BROWSER

void MagicGUIBuilder::registerJUCEFactories()
{
    
    //    "Favourites:",
    //    IDs::view.toString(),
    //    IDs::slider.toString(),
    //    "ImageButton",
    //    "PopupMenu",
    //    "ParameterLabel",
    //    "Image",
    //    "ImageMeter",
    //    "Text",
    //    "Rectangle",
    //    "Evaluate",
    //    "Trigger",
    //    "GuiProperty",
    
    bool isFavourite = true;

    registerFactory (IDs::slider,            &SliderItem::factory,            "Controls", isFavourite);
    registerFactory (IDs::comboBox,          &ComboBoxItem::factory,          "Controls");
    registerFactory (IDs::textButton,        &TextButtonItem::factory,        "Controls");
    registerFactory (IDs::toggleButton,      &ToggleButtonItem::factory,      "Controls");
    registerFactory (IDs::label,             &LabelItem::factory,             "Controls");
    registerFactory (IDs::plot,              &PlotItem::factory,              "Meters & Plots");
    registerFactory (IDs::xyDragComponent,   &XYDraggerItem::factory,        "Meters & Plots");
    registerFactory (IDs::keyboardComponent, &KeyboardItem::factory,          "Controls");
    registerFactory (IDs::drumpadComponent,  &DrumpadItem::factory,          "Controls");
    registerFactory (IDs::meter,             &LevelMeterItem::factory,        "Meters & Plots");
    registerFactory ("MidiLearn",            &MidiLearnItem::factory,         "MIDI & Sequencing");
    registerFactory (IDs::listBox,           &ListBoxItem::factory,           "Controls");

#if JUCE_MODULE_AVAILABLE_juce_gui_extra && JUCE_WEB_BROWSER
    registerFactory (IDs::webBrowser, &WebBrowserItem::factory);
#endif // JUCE_WEB_BROWSER
}

} // namespace foleys
