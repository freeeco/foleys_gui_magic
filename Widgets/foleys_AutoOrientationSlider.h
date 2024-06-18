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
            juce::Slider::paint (g);
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
                
#if defined SLIDER_FILMSTRIP_HORIZONTALLY_CENTERED
                auto xOffset = ((knobArea.getWidth()-(knobArea.getHeight()/aspect))/2);
#else
                auto xOffset = 0;
#endif
                
#if defined SLIDER_FILMSTRIP_VERTICALLY_CENTERED
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
                
#if defined SLIDER_FILMSTRIP_HORIZONTALLY_CENTERED
                auto xOffset = ((knobArea.getWidth()-(knobArea.getHeight()/aspect))/2);
#else
                auto xOffset = 0;
#endif
                
#if defined SLIDER_FILMSTRIP_VERTICALLY_CENTERED
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
#if JUCE_WINDOWS  & JUCE_VERSION >= 0x80000 & FILMSTRIP_SOFTWARE_IMAGE_TYPE
        filmStrip = juce::SoftwareImageType().convert(filmStrip);
#endif
    }

    void setNumImages (int num, bool horizontal)
    {
        numImages = num;
        horizontalFilmStrip = horizontal;
    }

private:

    bool autoOrientation = true;

    juce::Image filmStrip;
    int         numImages = 0;
    bool        horizontalFilmStrip = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutoOrientationSlider)
};

} // namespace foleys
