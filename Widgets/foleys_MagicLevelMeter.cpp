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

#include "foleys_MagicLevelMeter.h"
#include "../Visualisers/foleys_MagicLevelSource.h"

namespace foleys
{

MagicLevelMeter::MagicLevelMeter()
{
    setColour (backgroundColourId, juce::Colours::transparentBlack);
    setColour (barBackgroundColourId, juce::Colours::darkgrey);
    setColour (barFillColourId, juce::Colours::darkgreen);
    setColour (outlineColourId, juce::Colours::silver);
    setColour (tickmarkColourId, juce::Colours::silver);

    startTimerHz (30);
}

void MagicLevelMeter::paint (juce::Graphics& g)
{
    
    this->setTransform (juce::AffineTransform()); // clear transformation
    
    if (horizontalFlip){
        auto t1 = juce::AffineTransform::rotation (juce::MathConstants< float >::pi, getWidth()/2, getHeight()/2);
        auto t2 = juce::AffineTransform::verticalFlip (getHeight());
        auto t3 = t1.followedBy(t2);
        this->setTransform (t3);
    }
    
    if (verticalFlip){
        auto transform = juce::AffineTransform::verticalFlip (getHeight());
        this->setTransform (transform);
    }
    
    if (auto* lnf = dynamic_cast<LookAndFeelMethods*>(&getLookAndFeel()))
    {
        lnf->drawLevelMeter (g, *this, source, getLocalBounds());
        return;
    }

    const auto backgroundColour = findColour (backgroundColourId);
    if (!backgroundColour.isTransparent())
        g.fillAll (backgroundColour);

    if (source == nullptr)
        return;

    const auto numChannels = source->getNumChannels();
    if (numChannels == 0)
        return;

    auto bounds = getLocalBounds().reduced (3).toFloat();
    const auto aspect = bounds.getWidth() / bounds.getHeight();
    const auto barBackgroundColour = findColour (barBackgroundColourId);
    const auto barFillColour = findColour (barFillColourId);
    const auto outlineColour = findColour (outlineColourId);
    const auto tickmarkColour = findColour (tickmarkColourId);
    if (aspect < 1.0f)
    {
        auto width = bounds.getWidth() / numChannels;
        const auto infinity = -100.0f;
        for (int i=0; i < numChannels; ++i)
        {
            auto bar = bounds.removeFromLeft (width).reduced (1);
            g.setColour (barBackgroundColour);
            g.fillRoundedRectangle (bar, bar.getWidth() * 0.5f * barCorner);
            g.setColour (outlineColour);
            g.drawRoundedRectangle (bar, bar.getWidth() * 0.5f * barCorner, 0.8f);
            bar.reduce (0.8f, 0.8f);
            g.setColour (barFillColour);
            g.fillRoundedRectangle (bar.withTop (juce::jmap (juce::Decibels::gainToDecibels (source->getRMSvalue (i), infinity), infinity, 0.0f, bar.getBottom(), bar.getY())),bar.getWidth() * 0.5f * barCorner);
            
            // draw peak-hold line
            g.setColour (tickmarkColour);
            if ((juce::Decibels::gainToDecibels (source->getMaxValue (i)))> -80.0f){
                g.fillRoundedRectangle (juce::Rectangle(static_cast<float>(bar.getX ()), juce::jmap (juce::Decibels::gainToDecibels (source->getMaxValue (i), infinity),infinity, 0.0f, bar.getBottom (), bar.getY ()), static_cast<float>(bar.getWidth ()),(static_cast<float>((bounds.getHeight() / 100.0f))))*peakLineThickness, 0.0f);
            }
        }
    }
    else
    {
        auto height = bounds.getHeight() / numChannels;
        const auto infinity = -100.0f;
        for (int i=0; i < numChannels; ++i)
        {
            auto bar = bounds.removeFromTop (height).reduced (1);
            g.setColour (barBackgroundColour);
            g.fillRoundedRectangle (bar, bar.getHeight() * 0.5f * barCorner);
            g.setColour (outlineColour);
            g.drawRoundedRectangle (bar, bar.getHeight() * 0.5f * barCorner, 0.8f);
            bar.reduce (0.8f, 0.8f);
            g.setColour (barFillColour);
            g.fillRoundedRectangle (bar.withWidth (juce::jmap (juce::Decibels::gainToDecibels (source->getRMSvalue (i), infinity), infinity, 0.0f, bar.getX (), bar.getRight ())), bar.getHeight() * 0.5f * barCorner);
            
            // draw peak-hold line
            g.setColour (tickmarkColour);
            if ((juce::Decibels::gainToDecibels (source->getMaxValue (i)))> -80.0f){
                g.fillRoundedRectangle (juce::Rectangle(juce::jmap (juce::Decibels::gainToDecibels (source->getMaxValue (i), infinity),infinity, 0.0f, bar.getX (), bar.getRight ()), static_cast<float>(bar.getY ()), (static_cast<float>((bounds.getWidth()/100.0f)))*peakLineThickness, static_cast<float>(bar.getHeight ())),0.0f);
            }
        }
    }
}

void MagicLevelMeter::setLevelSource (MagicLevelSource* newSource)
{
    source = newSource;
}

void MagicLevelMeter::setBarCorner (float corner)
{
    barCorner = corner;
}

void MagicLevelMeter::setPeakLineThickness (float lineThickness)
{
    peakLineThickness = lineThickness;
}

void MagicLevelMeter::setHorizontalFlip (bool flip)
{
    horizontalFlip = flip;
}

void MagicLevelMeter::setVerticalFlip (bool flip)
{
    verticalFlip = flip;
}

void MagicLevelMeter::timerCallback()
{
    repaint();
}

} // namespace foleys
