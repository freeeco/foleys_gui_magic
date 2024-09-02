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

#include <juce_gui_basics/juce_gui_basics.h>

namespace foleys
{

/**
 This is a Slider, that holds an attachment to the AudioProcessorValueTreeState
 */
class AutoOrientationSlider  : public juce::Slider
{
public:

    AutoOrientationSlider() = default;

    void setAutoOrientation (bool shouldAutoOrient)
    {
        autoOrientation = shouldAutoOrient;
        resized();
    }

    void paint (juce::Graphics& g) override
    {
        if (filmStrip.isNull() || numImages == 0)
        {
            if (singleImage.isValid()){
                if (imageMode == 0){
                    float h = singleImage.getHeight();
                    float w = singleImage.getWidth();
                    float aspect = h / w;
                    
                    auto knobArea = getLookAndFeel().getSliderLayout(*this).sliderBounds;
                    
                    float areaW = knobArea.getWidth();
                    float areaH = knobArea.getHeight();
                    float areaAspect = areaH / areaW;
                    
#if SLIDER_FILMSTRIP_HORIZONTALLY_CENTERED
                    auto xOffset = ((knobArea.getWidth()-(knobArea.getHeight()/aspect))/2);
#else
                    auto xOffset = 0;
#endif
                    
#if SLIDER_FILMSTRIP_VERTICALLY_CENTERED
                    auto yOffset = ((knobArea.getHeight()-(knobArea.getWidth()*aspect))/2);
#else
                    auto yOffset = 0;
#endif
                    
                    auto originalBounds = singleImage.getBounds();
                    
                    auto rotation = startAngle + (valueToProportionOfLength (getValue()) * (1 - startAngle * 2));
                    
                    juce::AffineTransform transform = juce::AffineTransform::rotation(juce::MathConstants<float>::pi * 2.0f * rotation, originalBounds.getCentreX(), originalBounds.getCentreY());
                    
                    juce::Image rotatedImage(juce::Image::ARGB, singleImage.getWidth(), singleImage.getHeight(), true);
                    juce::Graphics gImage(rotatedImage);
                    gImage.drawImageTransformed(singleImage, transform);
                    
                    if (aspect > areaAspect){
                        g.drawImage (rotatedImage, knobArea.getX()+xOffset, knobArea.getY(), knobArea.getHeight()/aspect, knobArea.getHeight(),
                                     0, 0, singleImage.getWidth(), h);
                    }
                    else{
                        g.drawImage (rotatedImage, knobArea.getX(), knobArea.getY()+yOffset, knobArea.getWidth(), knobArea.getWidth()*aspect,
                                     0, 0, singleImage.getWidth(), h);
                    }
                }
                else if (imageMode == 1){
                    auto originalBounds = singleImage.getBounds().toFloat();
                    float scaleFactor = (getHeight() / originalBounds.getHeight());
                    juce::AffineTransform transform = juce::AffineTransform::scale(scaleFactor, scaleFactor);
                    float position = (getWidth() - (originalBounds.getWidth() * scaleFactor)) * (1 - startAngle) * valueToProportionOfLength (getValue());
                    transform = transform.translated(position, 0.0f);
                    g.drawImageTransformed(singleImage, transform);
                }
                else{ // imageMode == 2
                    auto originalBounds = singleImage.getBounds().toFloat();
                    float scaleFactor = (getWidth() / originalBounds.getWidth());
                    juce::AffineTransform transform = juce::AffineTransform::scale(scaleFactor, scaleFactor);
                    float position = (getHeight() - (originalBounds.getHeight() * scaleFactor)) * (1 - startAngle) * (1 - valueToProportionOfLength (getValue()));
                    transform = transform.translated(0.0f, position);
                    g.drawImageTransformed(singleImage, transform);
                }
            }
            else if (svg){
                if (imageMode == 0){
                    auto rotation = startAngle + (valueToProportionOfLength (getValue()) * (1 - startAngle * 2));
                    auto placement (juce::RectanglePlacement::centred);
                    auto originalBounds = svg->getDrawableBounds();
                    if (rotation == 0.0f)
                        rotation = 1.0f; // Strange bug with drawable transform which doesn't seem to like angle of 0.0f
                    juce::AffineTransform transform = juce::AffineTransform::rotation(juce::MathConstants<float>::pi * 2.0f * rotation, originalBounds.getCentreX(), originalBounds.getCentreY());
                    svg->setDrawableTransform(transform);
                    svg->drawWithin(g, getBounds().toFloat(), placement ,1.0f);
                }
                else if (imageMode == 1){
                    auto originalBounds = svg->getDrawableBounds().toFloat();
                    float scaleFactor = (getHeight() / originalBounds.getHeight());
                    juce::AffineTransform transform = juce::AffineTransform::scale(scaleFactor, scaleFactor);
                    svg->setDrawableTransform(transform);
                    float position = (getWidth() - (originalBounds.getWidth() * scaleFactor)) * (1 - startAngle) * valueToProportionOfLength (getValue());
                    svg->drawAt(g, position, 0.0f, 1.0f);
                }
                else{ // imageMode == 2
                    auto originalBounds = svg->getDrawableBounds().toFloat();
                    float scaleFactor = (getWidth() / originalBounds.getWidth());
                    juce::AffineTransform transform = juce::AffineTransform::scale(scaleFactor, scaleFactor);
                    svg->setDrawableTransform(transform);
                    float position = (getHeight() - (originalBounds.getHeight() * scaleFactor)) * (1 - startAngle) * (1.0f - valueToProportionOfLength (getValue()));
                    svg->drawAt(g, 0.0f, position, 1.0f);
                }
            }
            else{
                juce::Slider::paint (g);
            }
        }
        else
        {
            auto index = juce::roundToInt ((numImages - 1) * valueToProportionOfLength (getValue()));
            auto knobArea = getLookAndFeel().getSliderLayout(*this).sliderBounds;

            float areaW = knobArea.getWidth();
            float areaH = knobArea.getHeight();
            float areaAspect = areaH / areaW;
            
            g.setImageResamplingQuality(juce::Graphics::ResamplingQuality::highResamplingQuality);
            
            if (horizontalFilmStrip)
            {
                float w = filmStrip.getWidth() / numImages;
                float h = filmStrip.getHeight();
                float aspect = h / w;
                
#if SLIDER_FILMSTRIP_HORIZONTALLY_CENTERED
                auto xOffset = ((knobArea.getWidth()-(knobArea.getHeight()/aspect))/2);
#else
                auto xOffset = 0;
#endif
                
#if SLIDER_FILMSTRIP_VERTICALLY_CENTERED
                auto yOffset = ((knobArea.getHeight()-(knobArea.getWidth()*aspect))/2);
#else
                auto yOffset = 0;
#endif

                if (aspect > areaAspect){
                    g.drawImage (filmStrip, knobArea.getX()+xOffset, knobArea.getY(), knobArea.getHeight()/aspect, knobArea.getHeight(),
                                 index * w, 0, w, filmStrip.getHeight());
                }
                else{
                    g.drawImage (filmStrip, knobArea.getX(), knobArea.getY()+yOffset, knobArea.getWidth(), knobArea.getWidth()*aspect,
                                 index * w, 0, w, filmStrip.getHeight());
                }
            }
            else
            {
                float h = filmStrip.getHeight() / numImages;
                float w = filmStrip.getWidth();
                float aspect = h / w;
                
#if SLIDER_FILMSTRIP_HORIZONTALLY_CENTERED
                auto xOffset = ((knobArea.getWidth()-(knobArea.getHeight()/aspect))/2);
#else
                auto xOffset = 0;
#endif
                
#if SLIDER_FILMSTRIP_VERTICALLY_CENTERED
                auto yOffset = ((knobArea.getHeight()-(knobArea.getWidth()*aspect))/2);
#else
                auto yOffset = 0;
#endif
                
                if (aspect > areaAspect){
                    g.drawImage (filmStrip, knobArea.getX()+xOffset, knobArea.getY(), knobArea.getHeight()/aspect, knobArea.getHeight(),
                                 0, index * h, filmStrip.getWidth(), h);
                }
                else{
                    g.drawImage (filmStrip, knobArea.getX(), knobArea.getY()+yOffset, knobArea.getWidth(), knobArea.getWidth()*aspect,
                                 0, index * h, filmStrip.getWidth(), h);
                }
            }
        }
    }

    void resized() override
    {
        if (autoOrientation)
        {
            const auto w = getWidth();
            const auto h = getHeight();

            if (w > 2 * h)
                setSliderStyle (juce::Slider::LinearHorizontal);
            else if (h > 2 * w)
                setSliderStyle (juce::Slider::LinearVertical);
            else
                setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        }

        juce::Slider::resized();
    }

    void setFilmStrip (juce::Image& image)
    {
        filmStrip = image;
#if JUCE_WINDOWS  && JUCE_VERSION >= 0x80000 && FILMSTRIP_SOFTWARE_IMAGE_TYPE
        filmStrip = juce::SoftwareImageType().convert(filmStrip);
#endif
    }

    void setNumImages (int num, bool horizontal)
    {
        numImages = num;
        horizontalFilmStrip = horizontal;
    }
    
    void setImage (juce::Image& image)
    {
        singleImage = image;
    }
    
    void createImage (const char* data, int dataSize)
    {
        svg = juce::Drawable::createFromImageData (data, dataSize);
    }

    void setImageMode (int mode)
    {
        imageMode = mode;
    }

    void setStartAngle (float angle)
    {
        angle = angle + juce::MathConstants<float>::pi;
        startAngle = juce::jmap(angle, 0.0f, juce::MathConstants<float>::pi, 0.0f, 0.5f);
    }


private:

    bool autoOrientation = true;

    juce::Image filmStrip;
    int         numImages = 0;
    juce::Image singleImage;
    int         imageMode;
    float       startAngle;
    std::unique_ptr<juce::Drawable> svg;
    bool        horizontalFilmStrip = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutoOrientationSlider)
};

} // namespace foleys
