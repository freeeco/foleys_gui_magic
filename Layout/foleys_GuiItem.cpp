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
//    setOpaque (false);
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
    resized();
#endif
    
    setOpaque (decorator.getBackgroundColour().isOpaque() || decorator.getBackgroundImage().isValid());
    
    auto dynamicProp = magicBuilder.getStyleProperty (IDs::visibility, configNode).toString();
    if (dynamicProp.isEmpty())
        setVisible (getStaticVisibility());

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
    
    auto shadCol = magicBuilder.getStyleProperty (IDs::shadowColour, configNode);
    if (! shadCol.isVoid())
        shadowColour = magicBuilder.getStylesheet().getColour (shadCol.toString());
    else
        shadowColour = juce::Colours::black;
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
        
        auto tooltipFormatted = tooltip.replace("\\n", "\n"); // Use "\n" for new lines in tooltips fields
        
        if (tooltipFormatted.isNotEmpty())
            tooltipClient->setTooltip (tooltipFormatted);
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
    
    
    int minWidth = magicBuilder.getStyleProperty (IDs::minWidth, configNode);
    int minHeight = magicBuilder.getStyleProperty (IDs::minHeight, configNode);
    int maxWidth = magicBuilder.getStyleProperty (IDs::maxWidth, configNode);
    int maxHeight = magicBuilder.getStyleProperty (IDs::maxHeight, configNode);

    if (minWidth > 0.0)
        intBounds.setWidth(juce::jmax (intBounds.getWidth(), minWidth));
    
    if (minHeight > 0.0)
        intBounds.setHeight(juce::jmax (intBounds.getHeight(), minHeight));
    
    if (maxWidth > 0.0)
        intBounds.setWidth(juce::jmin (intBounds.getWidth(), maxWidth));
    
    if (maxHeight > 0.0)
        intBounds.setHeight(juce::jmin (intBounds.getHeight(), maxHeight));
    
    bool dontSnap = magicBuilder.getStyleProperty (IDs::dontSnapToPixels, configNode);
    
    if (dontSnap){
        juce::Rectangle<float> floatBounds = juce::Rectangle<float>(
                                              static_cast<float>(parent.getX()) + (posX.absolute ? posX.value : posX.value * static_cast<float>(parent.getWidth()) * 0.01f),
                                              static_cast<float>(parent.getY()) + (posY.absolute ? posY.value : posY.value * static_cast<float>(parent.getHeight()) * 0.01f),
                                              (posWidth.absolute ? posWidth.value : posWidth.value * static_cast<float>(parent.getWidth()) * 0.01f),
                                              (posHeight.absolute ? posHeight.value : posHeight.value * static_cast<float>(parent.getHeight()) * 0.01f)
                                          );
        
        diffX = floatBounds.toFloat().getX() - intBounds.getX();
        diffY = floatBounds.toFloat().getY() - intBounds.getY();
        diffWidth = floatBounds.toFloat().getWidth() - intBounds.getWidth();
        diffHeight = floatBounds.toFloat().getHeight() - intBounds.getHeight();
    }
    
    return intBounds;
}

void GuiItem::componentTransform()
{
    if (getBounds().toFloat().getWidth() == 0.0f || getBounds().toFloat().getHeight() == 0.0f)
        return;
    
    float scale = magicBuilder.getStyleProperty (IDs::scale, configNode);
    float widthScale = magicBuilder.getStyleProperty (IDs::widthScale, configNode);
    juce::String originXString    = magicBuilder.getStyleProperty (IDs::originX,       configNode).toString();
    juce::String originXOffsetStr = magicBuilder.getStyleProperty (IDs::originXOffset, configNode).toString();
    float heightScale = magicBuilder.getStyleProperty (IDs::heightScale, configNode);
    juce::String originYString    = magicBuilder.getStyleProperty (IDs::originY,       configNode).toString();
    juce::String originYOffsetStr = magicBuilder.getStyleProperty (IDs::originYOffset, configNode).toString();
    float horizontal = magicBuilder.getStyleProperty (IDs::horizontal, configNode);
    float vertical = magicBuilder.getStyleProperty (IDs::vertical, configNode);
    float rotate = magicBuilder.getStyleProperty (IDs::rotate, configNode);
    float opacity = magicBuilder.getStyleProperty (IDs::opacity, configNode);
    bool dontSnap = magicBuilder.getStyleProperty (IDs::dontSnapToPixels, configNode);
    
    int originX;
    if (originXString == "left")
        originX = getBounds().getX();
    else if (originXString == "right")
        originX = getBounds().getRight();
    else if (originXString == "centre")
        originX = getBounds().getCentreX();
    else
        originX = originXOffsetStr.isNotEmpty() ? getBounds().getX() : getBounds().getCentreX();

    int originY;
    if (originYString == "top")
        originY = getBounds().getY();
    else if (originYString == "bottom")
        originY = getBounds().getBottom();
    else if (originYString == "centre")
        originY = getBounds().getCentreY();
    else
        originY = originYOffsetStr.isNotEmpty() ? getBounds().getY() : getBounds().getCentreY();

    if (originXOffsetStr.isNotEmpty())
    {
        float offsetX = originXOffsetStr.endsWith ("%")
            ? originXOffsetStr.getFloatValue() * 0.01f * (float)getWidth()
            : originXOffsetStr.getFloatValue();
        originX += juce::roundToInt (offsetX);
    }

    if (originYOffsetStr.isNotEmpty())
    {
        float offsetY = originYOffsetStr.endsWith ("%")
            ? originYOffsetStr.getFloatValue() * 0.01f * (float)getHeight()
            : originYOffsetStr.getFloatValue();
        originY += juce::roundToInt (offsetY);
    }
    
#if FOLEYS_SHOW_GUI_EDITOR_PALLETTE
    lastOriginLocalX = originX - getBounds().getX();
    lastOriginLocalY = originY - getBounds().getY();
    hasOriginOffset  = originXOffsetStr.isNotEmpty() || originYOffsetStr.isNotEmpty()
                    || (originXString == "centre" && originYString == "centre");
#endif
    
    float componentHeight = (float)getHeight();

    juce::String glowRadiusString = magicBuilder.getStyleProperty (IDs::glowRadius, configNode);
    if (glowRadiusString.endsWith ("%")) {
        glowRadius = componentHeight * glowRadiusString.getFloatValue() * 0.01f;
    } else {
        glowRadius = glowRadiusString.getFloatValue();
    }

    juce::String glowDistanceString = magicBuilder.getStyleProperty (IDs::glowDistance, configNode);
    if (glowDistanceString.endsWith ("%")) {
        glowDistance = componentHeight * glowDistanceString.getFloatValue() * 0.01f;
    } else {
        glowDistance = glowDistanceString.getFloatValue();
    }

    glowAngle = magicBuilder.getStyleProperty (IDs::glowAngle, configNode);
    auto glowOpacityString = magicBuilder.getStyleProperty (IDs::glowOpacity, configNode).toString();
    if (glowOpacityString.isNotEmpty())
        glowOpacity = magicBuilder.getStyleProperty (IDs::glowOpacity, configNode);
    else
        glowOpacity = 1.0f;
        
    shadowEnable = magicBuilder.getStyleProperty (IDs::shadowEnable, configNode);
    redrawAll = magicBuilder.getStyleProperty (IDs::redrawAll, configNode);
    
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
    opacity = opacity + static_cast<float>(opacityValue.getValue());
    
    juce::AffineTransform transform;

    if (dontSnap){
        float diffScaleX = (getBounds().toFloat().getWidth() + diffWidth) / getBounds().toFloat().getWidth();
        float diffScaleY = (getBounds().toFloat().getHeight() + diffHeight) / getBounds().toFloat().getHeight();
        if ((diffScaleX != 1.0f || diffScaleY != 1.0 || diffX != 0.0 || diffY != 0.0) && diffScaleX != 0.0f && diffScaleY != 0.0f){
            transform = transform.scaled (diffScaleX, diffScaleY, getBounds().getX(), getBounds().getY());
            transform = transform.translated (diffX, diffY);
        }
    }
    
    if (scale != 1.0f || widthScale != 1.0f || heightScale != 1.0f || horizontal != 0.0f || vertical != 0.0f || rotate != 0.0f){
        scale = juce::jmax (scale, 0.00001f);
        
        // Check for small values
        
        if (heightScale > -0.00001f && heightScale < 0.00001f)
            heightScale = 0.00001f;
        
        if (widthScale > -0.00001f && widthScale < 0.00001f)
            widthScale = 0.00001f;
                    
        transform = transform.rotated((juce::MathConstants<float>::pi * 2.0f) * (rotate), originX, originY);
        transform = transform.scaled (scale * widthScale, scale * heightScale, originX, originY);
        transform = transform.translated (horizontal * getWidth(), vertical * getHeight());
    }
    
    if (scale != 1.0f || widthScale != 1.0f || heightScale != 1.0f || horizontal != 0.0f || vertical != 0.0f || rotate != 0.0f || dontSnap){
        setTransform (transform);
    } else {
        juce::AffineTransform noTransform;
        setTransform (noTransform);
    }
    
    setAlpha (opacity);
    
    auto propertyID = magicBuilder.getStyleProperty (IDs::visibility, configNode).toString();
    if (propertyID.isNotEmpty())
        setVisible (magicBuilder.getMagicState().getPropertyAsValue (propertyID).getValue());
}

void GuiItem::referValues()
{
    juce::String propertyID;

    propertyID = magicBuilder.getStyleProperty (IDs::visibility, configNode).toString();
    if (propertyID.isNotEmpty())
        visibility.referTo (magicBuilder.getMagicState().getPropertyAsValue (propertyID));
    
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
    
    auto* component = getWrappedComponent();

    blur.setRadius (glowRadius);

    if (component != nullptr && glowRadius > 0.0f)
    {
        juce::Image componentSnapshot =
            component->createComponentSnapshot (component->getLocalBounds());

        juce::Image imageToBlur;

        if (shadowEnable)
        {
            juce::Image maskImage (juce::Image::ARGB,
                                   componentSnapshot.getWidth(),
                                   componentSnapshot.getHeight(),
                                   true);

            {
                juce::Rectangle<int> sourceBounds = componentSnapshot.getBounds();
                juce::Graphics maskG (maskImage);

                maskG.setColour (shadowColour);

                maskG.drawImage (componentSnapshot,
                                 sourceBounds.getX(), sourceBounds.getY(), sourceBounds.getWidth(), sourceBounds.getHeight(),
                                 sourceBounds.getX(), sourceBounds.getY(), sourceBounds.getWidth(), sourceBounds.getHeight(),
                                 true);
            }

            imageToBlur = maskImage;
        }
        else
        {
            imageToBlur = componentSnapshot;
        }

        const int radius     = (int) std::ceil (glowRadius);
        const int extraSpace = radius + (int) std::ceil (std::abs (glowDistance));

        const int srcW = imageToBlur.getWidth();
        const int srcH = imageToBlur.getHeight();

        const int dstW = srcW + extraSpace * 2;
        const int dstH = srcH + extraSpace * 2;

        juce::Image paddedImage (juce::Image::ARGB, dstW, dstH, true);

        juce::Graphics pg (paddedImage);
        pg.drawImageAt (imageToBlur, extraSpace, extraSpace, false);

        const float clockwiseAngle = glowAngle + 0.75f;
        const float angleInRadians = clockwiseAngle * juce::MathConstants<float>::twoPi;
        const int offsetX = (int) (glowDistance * std::cos (angleInRadians));
        const int offsetY = (int) (glowDistance * std::sin (angleInRadians));

        juce::Image blurredGlow = blur.render (paddedImage);

        auto client = getClientBounds();
        const int baseX = client.getX() - extraSpace;
        const int baseY = client.getY() - extraSpace;

        g.setOpacity (glowOpacity);
        g.drawImageAt (blurredGlow,
                       baseX + offsetX,
                       baseY + offsetY);
        g.setOpacity (1.0f);

        if (redrawAll){
            if (blurNeedsRepaint){
                blurNeedsRepaint = false;
                repaint();
            } else{
                blurNeedsRepaint = true;
            }
        }
    }
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
    if (magicBuilder.getStyleProperty (IDs::visibility, configNode).toString().isNotEmpty())
        setVisible (visibility.getValue());
    
    componentTransform();
}

void GuiItem::updateLayout()
{
    resized();
}

LayoutType GuiItem::getParentsLayoutType() const
{
    if (auto* container = findParentComponentOfClass<Container>())
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
        const float handleSize = 5.0f;
        const float lineThickness = 1.0f;
        const auto lineColour = juce::Colour (0xff5ba6d6);
        const auto handleFill = juce::Colours::white;
        const float handleRadius = handleSize * 0.5f;

        auto bounds = getLocalBounds().toFloat().reduced (handleRadius);
        auto handleBounds = getLocalBounds().toFloat().reduced (handleRadius + 1.0f);

        // Selection border
        g.setColour (lineColour);
        g.drawRect (bounds, lineThickness);

        // Only show resize handles in Contents layout (free positioning)
        if (getParentsLayoutType() == LayoutType::Contents)
        {
            const juce::Point<float> handlePositions[] = {
                handleBounds.getTopLeft(),
                handleBounds.getTopRight(),
                handleBounds.getBottomLeft(),
                handleBounds.getBottomRight(),
                { handleBounds.getCentreX(), handleBounds.getY() },
                { handleBounds.getCentreX(), handleBounds.getBottom() },
                { handleBounds.getX(),       handleBounds.getCentreY() },
                { handleBounds.getRight(),   handleBounds.getCentreY() }
            };

            for (auto& pos : handlePositions)
            {
                auto handle = juce::Rectangle<float> (handleSize, handleSize).withCentre (pos);
                g.setColour (handleFill);
                g.fillEllipse (handle);
                g.setColour (lineColour);
                g.drawEllipse (handle, lineThickness);
            }
        }

        // Draw origin cross if an offset is active
        if (hasOriginOffset)
        {
            const float crossSize  = 6.0f;
            const float crossThick = 1.5f;
            const auto  crossCol   = juce::Colour (0xff5ba6d6);
            const float cx = (float) lastOriginLocalX;
            const float cy = (float) lastOriginLocalY;

            g.setColour (crossCol);
            g.drawLine (cx - crossSize, cy, cx + crossSize, cy, crossThick);
            g.drawLine (cx, cy - crossSize, cx, cy + crossSize, crossThick);
            g.drawEllipse (cx - 2.0f, cy - 2.0f, 4.0f, 4.0f, crossThick);
        }
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

    if (borderDragger)
        borderDragger->setVisible (shouldEdit);

    if (auto* component = getWrappedComponent())
        component->setInterceptsMouseClicks (!shouldEdit, !shouldEdit);
}

void GuiItem::toFrontForEditing()
{
    for (auto* comp = getParentComponent(); comp != nullptr; comp = comp->getParentComponent())
    {
        if (auto* container = dynamic_cast<Container*> (comp))
        {
            if (container->getLayoutMode() == LayoutType::Tabbed)
                container->showChildForEditing (this);
        }
    }

    toFront (false);
}

void GuiItem::setDraggable (bool selected)
{
    if (selected && selectionToFront && magicBuilder.isEditModeOn())
            toFrontForEditing();
    
    if (selected &&
            magicBuilder.isEditModeOn() &&
            getParentsLayoutType() == LayoutType::Contents &&
            configNode != magicBuilder.getGuiRootNode())
    {
        borderDragger = std::make_unique<BorderDragger>(this, nullptr);
        componentDragger = std::make_unique<juce::ComponentDragger>();

        borderDragger->onDragStart = [&]
        {
            magicBuilder.getUndoManager().beginNewTransaction ("Drag Component Position");
        };
        borderDragger->onDragging = [&]
        {
            savePosition();
            if (borderDragger)
                borderDragger->setBounds (getLocalBounds());
        };
        borderDragger->onDragEnd = [&]
        {
            savePosition();
            if (borderDragger)
                borderDragger->setBounds (getLocalBounds());
            if (auto* topLevel = getTopLevelComponent())
                    topLevel->grabKeyboardFocus();
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
    magicBuilder.getUndoManager().beginNewTransaction ("Nudge Left");
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
    magicBuilder.getUndoManager().beginNewTransaction ("Nudge Right");
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
    magicBuilder.getUndoManager().beginNewTransaction ("Nudge Up");
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
    magicBuilder.getUndoManager().beginNewTransaction ("Nudge Down");
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
    if (getParentsLayoutType() == LayoutType::Contents &&
        configNode != magicBuilder.getGuiRootNode() &&
        magicBuilder.getSelectedNode() != configNode)
    {
        magicBuilder.setSelectedNode (configNode);
    }
    
    if (magicBuilder.isEditModeOn())
    {
        originDragStartY      = event.getScreenPosition().y;
        originDragStartRotate = (float) magicBuilder.getStyleProperty (IDs::rotate, configNode);

        if (event.mods.isCommandDown() && !event.mods.isShiftDown() && !event.mods.isAltDown() && !event.mods.isCtrlDown())
        {
            magicBuilder.getUndoManager().beginNewTransaction ("Rotate Component");
            return;
        }
    }
    
    if (magicBuilder.isEditModeOn() && hasOriginOffset)
    {
        const float hitRadius = 8.0f;
        auto localPos = event.getPosition().toFloat();
        auto originPos = juce::Point<float> ((float)lastOriginLocalX, (float)lastOriginLocalY);

        if (localPos.getDistanceFrom (originPos) <= hitRadius)
        {
            isDraggingOrigin = true;
            magicBuilder.getUndoManager().beginNewTransaction ("Drag Origin Offset");
            return;
        }
    }

    if (componentDragger)
    {
        mouseDownTime = juce::Time::currentTimeMillis();
        mouseDownBounds = getBounds();
        lockedDragAxis = DragAxis::None;
        hasDuplicatedOnDrag = false;
        dragStartPosX = configNode.getProperty (IDs::posX).toString();
        dragStartPosY = configNode.getProperty (IDs::posY).toString();

        magicBuilder.getUndoManager().beginNewTransaction ("Drag Component Position");
        componentDragger->startDraggingComponent (this, event);
    }
}

void GuiItem::mouseDrag (const juce::MouseEvent& event)
{
    if (magicBuilder.isEditModeOn() && event.mods.isCommandDown()
        && !event.mods.isShiftDown() && !event.mods.isAltDown() && !event.mods.isCtrlDown())
    {
        int deltaY = originDragStartY - event.getScreenPosition().y;
        float deltaRotate = (float)deltaY / ((float)getHeight() * 10.0f);
        configNode.setProperty (IDs::rotate, originDragStartRotate + deltaRotate, &magicBuilder.getUndoManager());
        setMouseCursor (juce::MouseCursor::CrosshairCursor);
        return;
    }
    
    if (isDraggingOrigin)
    {
        auto localPos = event.getPosition();

        juce::String originXString = magicBuilder.getStyleProperty (IDs::originX, configNode).toString();
        juce::String originYString = magicBuilder.getStyleProperty (IDs::originY, configNode).toString();
        auto existingX = configNode.getProperty (IDs::originXOffset).toString();
        auto existingY = configNode.getProperty (IDs::originYOffset).toString();

        int baseX;
        if (originXString == "left")        baseX = 0;
        else if (originXString == "right")  baseX = getWidth();
        else if (originXString == "centre") baseX = getWidth() / 2;
        else                                baseX = existingX.isNotEmpty() ? 0 : getWidth() / 2;

        int baseY;
        if (originYString == "top")         baseY = 0;
        else if (originYString == "bottom") baseY = getHeight();
        else if (originYString == "centre") baseY = getHeight() / 2;
        else                                baseY = existingY.isNotEmpty() ? 0 : getHeight() / 2;

        int offsetX = localPos.x - baseX;
        int offsetY = localPos.y - baseY;

        auto* undo = &magicBuilder.getUndoManager();

        if (existingX.endsWith ("%"))
            configNode.setProperty (IDs::originXOffset, juce::String (offsetX * 100.0f / (float)getWidth(),  2) + "%", undo);
        else
            configNode.setProperty (IDs::originXOffset, offsetX, undo);

        if (existingY.endsWith ("%"))
            configNode.setProperty (IDs::originYOffset, juce::String (offsetY * 100.0f / (float)getHeight(), 2) + "%", undo);
        else
            configNode.setProperty (IDs::originYOffset, offsetY, undo);

        return;
    }

    if (componentDragger)
    {
        if (juce::Time::currentTimeMillis() - mouseDownTime < dragDelayMs)
            return;
        
        if (!hasDuplicatedOnDrag &&
            event.mouseWasDraggedSinceMouseDown() &&
            event.mods.isAltDown())
        {
            hasDuplicatedOnDrag = true;
        }

        componentDragger->dragComponent (this, event, nullptr);
        
        if (event.mods.isCtrlDown())
        {
            const int gridSize = 4;
            auto bounds = getBounds();
            bounds.setPosition (gridSize * (bounds.getX() / gridSize),
                                gridSize * (bounds.getY() / gridSize));
            setBounds (bounds);
        }

#if defined ENABLE_CONSTRAINED_DRAG
        auto bounds = getBounds();

        if (event.mods.isShiftDown())
        {
            auto drag = event.getOffsetFromDragStart();
            const int lockThreshold = 8;
            const int hysteresis = 20;

            if (lockedDragAxis == DragAxis::None)
            {
                if (std::abs (drag.x) >= std::abs (drag.y))
                    bounds.setY (mouseDownBounds.getY());
                else
                    bounds.setX (mouseDownBounds.getX());

                if (std::abs (drag.x) >= lockThreshold || std::abs (drag.y) >= lockThreshold)
                {
                    if (std::abs (drag.x) >= std::abs (drag.y))
                        lockedDragAxis = DragAxis::Horizontal;
                    else
                        lockedDragAxis = DragAxis::Vertical;
                }
            }
            else if (lockedDragAxis == DragAxis::Horizontal)
            {
                if (std::abs (drag.y) > std::abs (drag.x) + hysteresis)
                    lockedDragAxis = DragAxis::Vertical;
                bounds.setY (mouseDownBounds.getY());
            }
            else if (lockedDragAxis == DragAxis::Vertical)
            {
                if (std::abs (drag.x) > std::abs (drag.y) + hysteresis)
                    lockedDragAxis = DragAxis::Horizontal;
                bounds.setX (mouseDownBounds.getX());
            }
        }

        setBounds (bounds);
#endif

        savePosition();
    }
    else if (magicBuilder.isEditModeOn() && event.mouseWasDraggedSinceMouseDown())
    {
        auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this);
        container->startDragging (IDs::dragSelected, this);
    }
}

void GuiItem::mouseUp (const juce::MouseEvent& event)
{
    setMouseCursor (juce::MouseCursor::NormalCursor);
    
    if (isDraggingOrigin)
    {
        isDraggingOrigin = false;
        return;
    }
    
    if (hasDuplicatedOnDrag)
    {
        // Capture everything by value before the delay
        auto configCopy = configNode;
        auto posX = dragStartPosX;
        auto posY = dragStartPosY;
        auto* undo = &magicBuilder.getUndoManager();
        auto* builderPtr = &magicBuilder;

        juce::Timer::callAfterDelay (100, [configCopy, posX, posY, undo, builderPtr]() mutable
        {
            auto copy = juce::ValueTree::fromXml (configCopy.toXmlString());
            if (copy.isValid())
            {
                copy.setProperty (IDs::posX, posX, nullptr);
                copy.setProperty (IDs::posY, posY, nullptr);
                auto parent = configCopy.getParent();
                auto index  = parent.indexOf (configCopy);
                parent.addChild (copy, index, undo);
            }

            // Re-select the original to restore proper drag state
            builderPtr->setSelectedNode (juce::ValueTree());
            builderPtr->setSelectedNode (configCopy);
        });
    }
    hasDuplicatedOnDrag = false;
    
    if (! event.mouseWasDraggedSinceMouseDown())
    {
        if (isContainer())
        {
            if (auto* parentContainer = findParentComponentOfClass<Container>())
            {
                if (parentContainer->getLayoutMode() != LayoutType::Contents)
                {
                    for (int i = configNode.getNumChildren() - 1; i >= 0; --i)
                    {
                        auto childNode = configNode.getChild (i);
                        if (auto* childItem = magicBuilder.findGuiItem (childNode))
                        {
                            auto posInChild = event.getEventRelativeTo (childItem).getPosition();
                            if (childItem->getLocalBounds().contains (posInChild))
                            {
                                magicBuilder.setSelectedNode (childNode);
                                return;
                            }
                        }
                    }
                }
            }
        }

        magicBuilder.setSelectedNode (configNode);
    }
    
    if (auto* topLevel = getTopLevelComponent())
            topLevel->grabKeyboardFocus();
}

void GuiItem::mouseDoubleClick (const juce::MouseEvent& event)
{
    if (!isContainer())
        return;

    for (int i = configNode.getNumChildren() - 1; i >= 0; --i)
    {
        auto childNode = configNode.getChild (i);
        if (auto* childItem = magicBuilder.findGuiItem (childNode))
        {
            auto posInChild = event.getEventRelativeTo (childItem).getPosition();
            if (childItem->getLocalBounds().contains (posInChild))
            {
                magicBuilder.setSelectedNode (childNode);
                return;
            }
        }
    }
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

bool GuiItem::getStaticVisibility() const
{
    auto v = magicBuilder.getStyleProperty (IDs::visible, configNode);
    if (! v.isVoid())
        return static_cast<bool> (v);

    return isVisibleByDefault();
}

void GuiItem::refreshVisibility()
{
    auto dynamicProp = magicBuilder.getStyleProperty (IDs::visibility, configNode).toString();
    if (dynamicProp.isNotEmpty())
        setVisible (magicBuilder.getMagicState().getPropertyAsValue (dynamicProp).getValue());
    else
        setVisible (getStaticVisibility());
}


}
