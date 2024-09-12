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

#include "foleys_GuiItem.h"

namespace foleys
{

GuiItem::GuiItem (MagicGUIBuilder& builder, juce::ValueTree node)
  : magicBuilder (builder),
    configNode (node)
{
    setOpaque (false);
    setInterceptsMouseClicks (false, true);

    visibility.addListener (this);
    scaleValue.addListener (this);
    widthScaleValue.addListener (this);
    heightScaleValue.addListener (this);
    horizontalValue.addListener (this);
    verticalValue.addListener (this);
    rotateValue.addListener (this);
    opacityValue.addListener (this);
    
    configNode.addListener (this);
    magicBuilder.getStylesheet().addListener (this);
}

GuiItem::~GuiItem()
{
    magicBuilder.getStylesheet().removeListener (this);
}

void GuiItem::setColourTranslation (std::vector<std::pair<juce::String, int>> mapping)
{
    colourTranslation = mapping;
}

juce::StringArray GuiItem::getColourNames() const
{
    juce::StringArray names;

    for (const auto& pair : colourTranslation)
        names.addIfNotAlreadyThere (pair.first);

    return names;
}

juce::var GuiItem::getProperty (const juce::Identifier& property)
{
    return magicBuilder.getStyleProperty (property, configNode);
}

MagicGUIState& GuiItem::getMagicState()
{
    return magicBuilder.getMagicState();
}

GuiItem* GuiItem::findGuiItemWithId (const juce::String& name)
{
    if (configNode.getProperty (IDs::id, juce::String()).toString() == name)
        return this;

    return nullptr;
}

void GuiItem::updateInternal()
{
    auto& stylesheet = magicBuilder.getStylesheet();

    if (auto* newLookAndFeel = stylesheet.getLookAndFeel (configNode))
        setLookAndFeel (newLookAndFeel);

    decorator.configure (magicBuilder, configNode);
    configureComponent();
    configureFlexBoxItem (configNode);
    configurePosition (configNode);

    updateColours();

    update();

#if FOLEYS_SHOW_GUI_EDITOR_PALLETTE
    setEditMode (magicBuilder.isEditModeOn());
#endif

    repaint();
}

void GuiItem::updateColours()
{
    decorator.updateColours (magicBuilder, configNode);

    auto* component = getWrappedComponent();
    if (component == nullptr)
        return;

    for (auto& pair : colourTranslation)
    {
        auto colour = magicBuilder.getStyleProperty (pair.first, configNode).toString();
        if (colour.isNotEmpty())
            component->setColour (pair.second, magicBuilder.getStylesheet().getColour (colour));
    }
}

void GuiItem::configureComponent()
{
    auto* component = getWrappedComponent();
    if (component == nullptr)
        return;

    component->setComponentID (configNode.getProperty (IDs::id, juce::String()).toString());

    if (auto* tooltipClient = dynamic_cast<juce::SettableTooltipClient*>(component))
    {
        auto tooltip = magicBuilder.getStyleProperty (IDs::tooltip, configNode).toString();
        if (tooltip.isNotEmpty())
            tooltipClient->setTooltip (tooltip);
    }

    component->setAccessible (magicBuilder.getStyleProperty (IDs::accessibility, configNode));
    component->setTitle (magicBuilder.getStyleProperty (IDs::accessibilityTitle, configNode));
    component->setDescription (magicBuilder.getStyleProperty (IDs::accessibilityDescription, configNode).toString());
    component->setHelpText (magicBuilder.getStyleProperty (IDs::accessibilityHelpText, configNode).toString());
    component->setExplicitFocusOrder (magicBuilder.getStyleProperty (IDs::accessibilityFocusOrder, configNode));
    
    referValues();
    componentTransform();
}

void GuiItem::configureFlexBoxItem (const juce::ValueTree& node)
{
    flexItem = juce::FlexItem (*this).withFlex (1.0f);

    const auto minWidth = magicBuilder.getStyleProperty (IDs::minWidth, node);
    if (! minWidth.isVoid())
        flexItem.minWidth = minWidth;

    const auto maxWidth = magicBuilder.getStyleProperty (IDs::maxWidth, node);
    if (! maxWidth.isVoid())
        flexItem.maxWidth = maxWidth;

    const auto minHeight = magicBuilder.getStyleProperty (IDs::minHeight, node);
    if (! minHeight.isVoid())
        flexItem.minHeight = minHeight;

    const auto maxHeight = magicBuilder.getStyleProperty (IDs::maxHeight, node);
    if (! maxHeight.isVoid())
        flexItem.maxHeight = maxHeight;

    const auto width = magicBuilder.getStyleProperty (IDs::width, node);
    if (! width.isVoid())
        flexItem.width = width;

    const auto height = magicBuilder.getStyleProperty (IDs::height, node);
    if (! height.isVoid())
        flexItem.height = height;

    auto grow = magicBuilder.getStyleProperty (IDs::flexGrow, node);
    if (! grow.isVoid())
        flexItem.flexGrow = grow;

    const auto flexShrink = magicBuilder.getStyleProperty (IDs::flexShrink, node);
    if (! flexShrink.isVoid())
        flexItem.flexShrink = flexShrink;

    const auto flexOrder = magicBuilder.getStyleProperty (IDs::flexOrder, node);
    if (! flexOrder.isVoid())
        flexItem.order = flexOrder;

    const auto alignSelf = magicBuilder.getStyleProperty (IDs::flexAlignSelf, node).toString();
    if (alignSelf == IDs::flexStart)
        flexItem.alignSelf = juce::FlexItem::AlignSelf::flexStart;
    else if (alignSelf == IDs::flexEnd)
        flexItem.alignSelf = juce::FlexItem::AlignSelf::flexEnd;
    else if (alignSelf == IDs::flexCenter)
        flexItem.alignSelf = juce::FlexItem::AlignSelf::center;
    else if (alignSelf == IDs::flexAuto)
        flexItem.alignSelf = juce::FlexItem::AlignSelf::autoAlign;
    else
        flexItem.alignSelf = juce::FlexItem::AlignSelf::stretch;
}

void GuiItem::configurePosition (const juce::ValueTree& node)
{
    configurePosition (magicBuilder.getStyleProperty (IDs::posX, node), posX, 0.0);
    configurePosition (magicBuilder.getStyleProperty (IDs::posY, node), posY, 0.0);
    configurePosition (magicBuilder.getStyleProperty (IDs::posWidth, node), posWidth, 100.0);
    configurePosition (magicBuilder.getStyleProperty (IDs::posHeight, node), posHeight, 100.0);
}

void GuiItem::configurePosition (const juce::var& v, Position& p, double d)
{
    if (v.isVoid())
    {
        p.absolute = false;
        p.value = d;
    }
    else
    {
        auto const s = v.toString();
        p.absolute = ! s.endsWith ("%");
        p.value = s.getDoubleValue();
    }
}

juce::Rectangle<int> GuiItem::resolvePosition (juce::Rectangle<int> parent)
{
    juce::Rectangle<int> intBounds = juce::Rectangle(
                                          parent.getX() + juce::roundToInt (posX.absolute ? posX.value : posX.value * parent.getWidth() * 0.01),
                                          parent.getY() + juce::roundToInt (posY.absolute ? posY.value : posY.value * parent.getHeight() * 0.01),
                                          juce::roundToInt (posWidth.absolute ? posWidth.value : posWidth.value * parent.getWidth() * 0.01),
                                          juce::roundToInt (posHeight.absolute ? posHeight.value : posHeight.value * parent.getHeight() * 0.01)
                                      );
    
    bool dontSnap = magicBuilder.getStyleProperty (IDs::dontSnapToPixels, configNode);
    
    if (dontSnap){
        juce::Rectangle<double> doubleBounds = juce::Rectangle(
                                              static_cast<float>(parent.getX()) + (posX.absolute ? posX.value : posX.value * static_cast<float>(parent.getWidth()) * 0.01f),
                                              static_cast<float>(parent.getY()) + (posY.absolute ? posY.value : posY.value * static_cast<float>(parent.getHeight()) * 0.01f),
                                              (posWidth.absolute ? posWidth.value : posWidth.value * static_cast<float>(parent.getWidth()) * 0.01f),
                                              (posHeight.absolute ? posHeight.value : posHeight.value * static_cast<float>(parent.getHeight()) * 0.01f)
                                          );
        
        diffX = doubleBounds.toFloat().getX() - intBounds.getX();
        diffY = doubleBounds.toFloat().getY() - intBounds.getY();
        diffWidth = doubleBounds.toFloat().getWidth() - intBounds.getWidth();
        diffHeight = doubleBounds.toFloat().getHeight() - intBounds.getHeight();
    }
    
    return intBounds;
}

void GuiItem::componentTransform()
{
    float scale = magicBuilder.getStyleProperty (IDs::scale, configNode);
    float widthScale = magicBuilder.getStyleProperty (IDs::widthScale, configNode);
    float heightScale = magicBuilder.getStyleProperty (IDs::heightScale, configNode);
    float horizontal = magicBuilder.getStyleProperty (IDs::horizontal, configNode);
    float vertical = magicBuilder.getStyleProperty (IDs::vertical, configNode);
    float rotate = magicBuilder.getStyleProperty (IDs::rotate, configNode);
    float opacity = magicBuilder.getStyleProperty (IDs::opacity, configNode);
    bool dontSnap = magicBuilder.getStyleProperty (IDs::dontSnapToPixels, configNode);
    
    if (scale == 0.0f)
        scale = 1.0f;
    
    if (widthScale == 0.0f)
        widthScale = 1.0f;
    
    if (heightScale == 0.0f)
        heightScale = 1.0f;
    
    if (magicBuilder.getStyleProperty (IDs::opacity, configNode).toString().isEmpty())
        opacity = 1.0f;
    
    scale = scale * static_cast<float>(scaleValue.getValue());
    widthScale = widthScale * static_cast<float>(widthScaleValue.getValue());
    heightScale = heightScale * static_cast<float>(heightScaleValue.getValue());
    horizontal = horizontal + static_cast<float>(horizontalValue.getValue());
    vertical = vertical + static_cast<float>(verticalValue.getValue());
    rotate = rotate + static_cast<float>(rotateValue.getValue());
    opacity = opacity * static_cast<float>(opacityValue.getValue());
    
    juce::AffineTransform transform;
        
    if (dontSnap){
        transform = transform.scaled ((getBounds().toFloat().getWidth() + diffWidth) / getBounds().toFloat().getWidth(), (getBounds().toFloat().getHeight() + diffHeight) / getBounds().toFloat().getHeight(), getBounds().getX(), getBounds().getY());
        transform = transform.translated (diffX, diffY);
    }

    if (scale != 1.0f || widthScale != 1.0f || heightScale != 1.0f || horizontal != 0.0f || vertical != 0.0f || rotate != 0.0f){
        scale = juce::jmax (scale, 0.00001f);
        widthScale = juce::jmax (widthScale, 0.00001f);
        heightScale = juce::jmax (heightScale, 0.00001f);

        transform = transform.rotated((juce::MathConstants<float>::pi * 2.0f) * (rotate), getBounds().getCentreX(), getBounds().getCentreY());
        transform = transform.scaled (scale * widthScale, scale * heightScale, getBounds().getCentreX(), getBounds().getCentreY());
        transform = transform.translated (horizontal * getWidth(), vertical * getHeight());
    }
    
    if (scale != 1.0f || widthScale != 1.0f || heightScale != 1.0f || horizontal != 0.0f || vertical != 0.0f || rotate != 0.0f || dontSnap){
        setTransform (transform);
    }
    
    setAlpha (opacity);
}

void GuiItem::referValues()
{
    juce::String propertyID;

    propertyID = magicBuilder.getStyleProperty (IDs::visibility, configNode).toString();
    if (propertyID.isNotEmpty()){
        visibility.referTo (magicBuilder.getMagicState().getPropertyAsValue (propertyID));
        hasVisibilityProperty = true;
    } else {
        hasVisibilityProperty = false;
    }
    
    propertyID = magicBuilder.getStyleProperty (IDs::scaleValue, configNode).toString();
    if (propertyID.isNotEmpty())
        scaleValue.referTo (getMagicState().getPropertyAsValue (propertyID));
    
    propertyID = magicBuilder.getStyleProperty (IDs::widthScaleValue, configNode).toString();
    if (propertyID.isNotEmpty())
        widthScaleValue.referTo (getMagicState().getPropertyAsValue (propertyID));
    
    propertyID = magicBuilder.getStyleProperty (IDs::heightScaleValue, configNode).toString();
    if (propertyID.isNotEmpty())
        heightScaleValue.referTo (getMagicState().getPropertyAsValue (propertyID));
    
    propertyID = magicBuilder.getStyleProperty (IDs::horizontalValue, configNode).toString();
    if (propertyID.isNotEmpty())
        horizontalValue.referTo (getMagicState().getPropertyAsValue (propertyID));
    
    propertyID = magicBuilder.getStyleProperty (IDs::verticalValue, configNode).toString();
    if (propertyID.isNotEmpty())
        verticalValue.referTo (getMagicState().getPropertyAsValue (propertyID));
    
    propertyID = magicBuilder.getStyleProperty (IDs::rotateValue, configNode).toString();
    if (propertyID.isNotEmpty())
        rotateValue.referTo (getMagicState().getPropertyAsValue (propertyID));
    
    propertyID = magicBuilder.getStyleProperty (IDs::opacityValue, configNode).toString();
    if (propertyID.isNotEmpty())
        opacityValue.referTo (getMagicState().getPropertyAsValue (propertyID));
}

void GuiItem::paint (juce::Graphics& g)
{
    decorator.drawDecorator (g, getLocalBounds());
}

juce::Rectangle<int> GuiItem::getClientBounds() const
{
    return decorator.getClientBounds (getLocalBounds()).client;
}

void GuiItem::resized()
{
#if FOLEYS_SHOW_GUI_EDITOR_PALLETTE
    if (borderDragger)
        borderDragger->setBounds (getLocalBounds());
#endif

    if (auto* component = getWrappedComponent())
    {
        auto b = getClientBounds();
        component->setVisible (b.getWidth() > 2 && b.getHeight() > 2);
        component->setBounds (b);
    }
    if (hasVisibilityProperty)
        setVisible (visibility.getValue());
    
    componentTransform();
}

void GuiItem::updateLayout()
{
    resized();
}

LayoutType GuiItem::getParentsLayoutType() const
{
    if (auto* container = dynamic_cast<Container*>(getParentComponent()))
        return container->getLayoutMode();

    return LayoutType::Contents;
}

juce::String GuiItem::getTabCaption (const juce::String& defaultName) const
{
    return decorator.getTabCaption (defaultName);
}

juce::Colour GuiItem::getTabColour() const
{
    return decorator.getTabColour();
}

void GuiItem::handleValueChanged (juce::Value& source)
{
    if (source.refersToSameSourceAs(visibility))
        setVisible (source.getValue());
    
    if (source.refersToSameSourceAs(scaleValue) || source.refersToSameSourceAs(widthScaleValue) || source.refersToSameSourceAs(heightScaleValue) || source.refersToSameSourceAs(horizontalValue) || source.refersToSameSourceAs(verticalValue) || source.refersToSameSourceAs(rotateValue) || source.refersToSameSourceAs(opacityValue)){
        componentTransform();
        repaint();
    }
}

void GuiItem::valueChanged (juce::Value& source)
{
    handleValueChanged (source);
}

void GuiItem::valueTreePropertyChanged (juce::ValueTree& treeThatChanged, const juce::Identifier&)
{
    if (treeThatChanged == configNode)
    {
        if (auto* parent = findParentComponentOfClass<GuiItem>())
            parent->updateInternal();
        else
            updateInternal();

        return;
    }

    auto& stylesheet = magicBuilder.getStylesheet();
    if (stylesheet.isClassNode (treeThatChanged))
    {
        auto name = treeThatChanged.getType().toString();
        auto classes = configNode.getProperty (IDs::styleClass, juce::String()).toString();
        if (classes.contains (name))
            updateInternal();
    }
}

void GuiItem::valueTreeChildAdded (juce::ValueTree& treeThatChanged, juce::ValueTree&)
{
    if (treeThatChanged == configNode)
        createSubComponents();
}

void GuiItem::valueTreeChildRemoved (juce::ValueTree& treeThatChanged, juce::ValueTree&, int)
{
    if (treeThatChanged == configNode)
        createSubComponents();
}

void GuiItem::valueTreeChildOrderChanged (juce::ValueTree& treeThatChanged, int, int)
{
    if (treeThatChanged == configNode)
        createSubComponents();
}

void GuiItem::valueTreeParentChanged (juce::ValueTree& treeThatChanged)
{
    if (treeThatChanged == configNode)
    {
        if (auto* parent = dynamic_cast<GuiItem*>(getParentComponent()))
            parent->updateInternal();
        else
            updateInternal();
    }
}

void GuiItem::itemDragEnter (const juce::DragAndDropTarget::SourceDetails& details)
{
    if (details.description.toString().startsWith (IDs::dragCC))
    {
        auto paramID = getControlledParameterID (details.localPosition);
        if (paramID.isNotEmpty())
            if (auto* parameter = magicBuilder.getMagicState().getParameter (paramID))
                highlight = parameter->getName (64);

        repaint();
    }
}

void GuiItem::itemDragExit (const juce::DragAndDropTarget::SourceDetails& details)
{
    juce::ignoreUnused (details);
    highlight.clear();
    repaint();
}

GuiItem* GuiItem::findGuiItem (const juce::ValueTree& node)
{
    if (node == configNode)
        return this;

    return nullptr;
}

void GuiItem::paintOverChildren (juce::Graphics& g)
{
#if FOLEYS_SHOW_GUI_EDITOR_PALLETTE
    if (magicBuilder.isEditModeOn() && magicBuilder.getSelectedNode() == configNode)
    {
        g.setColour (juce::Colours::orange.withAlpha (0.5f));
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 5.0f);
    }
#endif

    if (highlight.isNotEmpty())
    {
        g.setColour (juce::Colour(1.0f, 0.0f, 0.0f, 0.3f));
        g.fillRoundedRectangle((getWidth()-getHeight())/2,0,getHeight(),getHeight(),getHeight());
    }
}

#if FOLEYS_SHOW_GUI_EDITOR_PALLETTE

void GuiItem::setEditMode (bool shouldEdit)
{
    setInterceptsMouseClicks (shouldEdit, true);

    if (auto* component = getWrappedComponent())
        component->setInterceptsMouseClicks (!shouldEdit, !shouldEdit);
}

void GuiItem::setDraggable (bool selected)
{
    if (selected &&
        getParentsLayoutType() == LayoutType::Contents &&
        configNode != magicBuilder.getGuiRootNode())
    {
        toFront (false);
        borderDragger = std::make_unique<BorderDragger>(this, nullptr);
        componentDragger = std::make_unique<juce::ComponentDragger>();

        borderDragger->onDragStart = [&]
        {
            magicBuilder.getUndoManager().beginNewTransaction ("Drag component position");
        };
        borderDragger->onDragging = [&]
        {
            savePosition();
        };
        borderDragger->onDragEnd = [&]
        {
            savePosition();
        };

        borderDragger->setBounds (getLocalBounds());
        addAndMakeVisible (*borderDragger);
    }
    else
    {
        borderDragger.reset();
        componentDragger.reset();
    }
}

void GuiItem::nudgeLeft ()
{
    auto itemBounds = getBounds();
    int distance = 1;
    const auto& modifiers = juce::ModifierKeys::getCurrentModifiers();
    if (modifiers.isCtrlDown())
        distance = 4;
    itemBounds.setX(itemBounds.getX() - distance);
    setBounds(itemBounds);
    savePosition();
}

void GuiItem::nudgeRight ()
{
    auto itemBounds = getBounds();
    int distance = 1;
    const auto& modifiers = juce::ModifierKeys::getCurrentModifiers();
    if (modifiers.isCtrlDown())
        distance = 4;
    itemBounds.setX(itemBounds.getX() + distance);
    setBounds(itemBounds);
    savePosition();
}

void GuiItem::nudgeUp ()
{
    auto itemBounds = getBounds();
    int distance = 1;
    const auto& modifiers = juce::ModifierKeys::getCurrentModifiers();
    if (modifiers.isCtrlDown())
        distance = 4;
    itemBounds.setY(itemBounds.getY() - distance);
    setBounds(itemBounds);
    savePosition();
}

void GuiItem::nudgeDown ()
{
    auto itemBounds = getBounds();
    int distance = 1;
    const auto& modifiers = juce::ModifierKeys::getCurrentModifiers();
    if (modifiers.isCtrlDown())
        distance = 4;
    itemBounds.setY(itemBounds.getY() + distance);
    setBounds(itemBounds);
    savePosition();
}

void GuiItem::savePosition ()
{
    auto* container = findParentComponentOfClass<Container>();

    if (container == nullptr)
        return;

    auto parent = container->getClientBounds();

    auto px = posX.absolute ? juce::String (getX() - parent.getX()) : juce::String (100.0 * (getX() - parent.getX()) / parent.getWidth()) + "%";
    auto py = posY.absolute ? juce::String (getY() - parent.getY()) : juce::String (100.0 * (getY() - parent.getY()) / parent.getHeight()) + "%";
    auto pw = posWidth.absolute ? juce::String (getWidth()) : juce::String (100.0 * getWidth() / parent.getWidth()) + "%";
    auto ph = posHeight.absolute ? juce::String (getHeight()) : juce::String (100.0 * getHeight() / parent.getHeight()) + "%";

    auto* undo = &magicBuilder.getUndoManager();
    configNode.setProperty (IDs::posX, px, undo);
    configNode.setProperty (IDs::posY, py, undo);
    configNode.setProperty (IDs::posWidth, pw, undo);
    configNode.setProperty (IDs::posHeight, ph, undo);
}

void GuiItem::mouseDown (const juce::MouseEvent& event)
{
    if (componentDragger)
    {
        magicBuilder.getUndoManager().beginNewTransaction ("Drag component position");
        componentDragger->startDraggingComponent (this, event);
    }
}

void GuiItem::mouseDrag (const juce::MouseEvent& event)
{
    if (componentDragger)
    {
        componentDragger->dragComponent (this, event, nullptr);
        savePosition();
    }
    else if (event.mouseWasDraggedSinceMouseDown())
    {
        auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this);
        container->startDragging (IDs::dragSelected, this);
    }
}

void GuiItem::mouseUp (const juce::MouseEvent& event)
{
    if (! event.mouseWasDraggedSinceMouseDown())
        magicBuilder.setSelectedNode (configNode);
}

#endif

bool GuiItem::isInterestedInDragSource (const juce::DragAndDropTarget::SourceDetails &)
{
    return true;
}

void GuiItem::itemDropped (const juce::DragAndDropTarget::SourceDetails &dragSourceDetails)
{
    highlight.clear();

    auto dragString = dragSourceDetails.description.toString();
    if (dragString.startsWith (IDs::dragCC))
    {
        auto number = dragString.substring (IDs::dragCC.length()).getIntValue();
        auto parameterID = getControlledParameterID (dragSourceDetails.localPosition);
        if (number > 0 && parameterID.isNotEmpty())
            if (auto* procState = dynamic_cast<MagicProcessorState*>(&magicBuilder.getMagicState()))
                procState->mapMidiController (number, parameterID);

        repaint();
        return;
    }

#if FOLEYS_SHOW_GUI_EDITOR_PALLETTE
    if (dragSourceDetails.description == IDs::dragSelected)
    {
        auto dragged = magicBuilder.getSelectedNode();
        if (dragged.isValid() == false)
            return;

        magicBuilder.draggedItemOnto (dragged, configNode);
        return;
    }

    auto node = juce::ValueTree::fromXml (dragSourceDetails.description.toString());
    if (node.isValid())
        magicBuilder.draggedItemOnto (node, configNode);
#endif
}


}
