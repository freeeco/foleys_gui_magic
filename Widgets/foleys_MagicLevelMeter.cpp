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

    setPaintingIsUnclipped (true);
    
    startTimerHz (refreshRateHz);
}



void MagicLevelMeter::paint (juce::Graphics& g)
{

    this->setTransform (juce::AffineTransform()); // clear transformation
    
    if (horizontalFlip){
        auto t1 = juce::AffineTransform::rotation (juce::MathConstants< float >::pi, getWidth()/2.0f, getHeight()/2.0f);
        auto t2 = juce::AffineTransform::verticalFlip (static_cast<float>(getHeight()));
        auto t3 = t1.followedBy(t2);
        this->setTransform (t3);
    }
    
    if (verticalFlip){
        auto transform = juce::AffineTransform::verticalFlip (static_cast<float>(getHeight()));
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

    if (aspect < 1.0f) // Vertical Meter
    {
        auto width = bounds.getWidth() / numChannels;
        for (int i=0; i < numChannels; ++i)
        {
            auto bar = bounds.removeFromLeft (width).reduced (1.0f); // Use 1.0f for clarity
            g.setColour (barBackgroundColour);
            g.fillRoundedRectangle (bar, bar.getWidth() * 0.5f * barCorner);
            g.setColour (outlineColour);
            g.drawRoundedRectangle (bar, bar.getWidth() * 0.5f * barCorner, 0.8f);
            
            auto fillBar = bar.reduced (0.8f, 0.8f); // Use a different variable for the fill area

            // RMS Bar
            float rms_db_val = juce::Decibels::gainToDecibels(source->getRMSvalue(i), DB_FLOOR);
            float normalized_rms_pos = db_to_position(rms_db_val, METER_SCALE_MIN_DB, METER_SCALE_MAX_DB, METER_SCALE_GAMMA);
            float top_y_rms = juce::jmap(normalized_rms_pos, 0.0f, 1.0f, fillBar.getBottom(), fillBar.getY());
            
            g.setColour (barFillColour);
            g.fillRoundedRectangle (fillBar.withTop(top_y_rms), fillBar.getWidth() * 0.5f * barCorner);
            
            // Peak-hold line
            float peak_db_val = juce::Decibels::gainToDecibels(source->getMaxValue(i), DB_FLOOR);
            if (peak_db_val > -80.0f) // Keep original threshold for drawing peak line at all
            {
                float normalized_peak_pos = db_to_position(peak_db_val, METER_SCALE_MIN_DB, METER_SCALE_MAX_DB, METER_SCALE_GAMMA);
                float top_y_peak = juce::jmap(normalized_peak_pos, 0.0f, 1.0f, fillBar.getBottom(), fillBar.getY());
                
                float peak_line_actual_height = (bounds.getHeight() / 100.0f) * peakLineThickness;
                if (normalized_peak_pos > 0.0f && peak_line_actual_height < 1.0f) peak_line_actual_height = 1.0f; // Ensure visibility

                g.setColour (tickmarkColour);
                g.fillRoundedRectangle (juce::Rectangle<float>(fillBar.getX(), top_y_peak, fillBar.getWidth(), peak_line_actual_height), 0.0f);
            }
        }
    }
    else // Horizontal Meter
    {
        auto height = bounds.getHeight() / numChannels;
        for (int i=0; i < numChannels; ++i)
        {
            auto bar = bounds.removeFromTop (height).reduced (1.0f);
            g.setColour (barBackgroundColour);
            g.fillRoundedRectangle (bar, bar.getHeight() * 0.5f * barCorner);
            g.setColour (outlineColour);
            g.drawRoundedRectangle (bar, bar.getHeight() * 0.5f * barCorner, 0.8f);

            auto fillBar = bar.reduced (0.8f, 0.8f);

            // RMS Bar
            float rms_db_val_h = juce::Decibels::gainToDecibels(source->getRMSvalue(i), DB_FLOOR);
            float normalized_rms_pos_h = db_to_position(rms_db_val_h, METER_SCALE_MIN_DB, METER_SCALE_MAX_DB, METER_SCALE_GAMMA);
            // Map normalized position (0-1) to the bar's right X-coordinate
            float right_x_rms = juce::jmap(normalized_rms_pos_h, 0.0f, 1.0f, fillBar.getX(), fillBar.getRight());
            
            g.setColour (barFillColour);
            g.fillRoundedRectangle (fillBar.withRight(right_x_rms), fillBar.getHeight() * 0.5f * barCorner);
            
            // Peak-hold line
            float peak_db_val_h = juce::Decibels::gainToDecibels(source->getMaxValue(i), DB_FLOOR);
            if (peak_db_val_h > -80.0f)
            {
                float normalized_peak_pos_h = db_to_position(peak_db_val_h, METER_SCALE_MIN_DB, METER_SCALE_MAX_DB, METER_SCALE_GAMMA);
                float peak_x_pos = juce::jmap(normalized_peak_pos_h, 0.0f, 1.0f, fillBar.getX(), fillBar.getRight());

                float peak_line_actual_width = (bounds.getWidth() / 100.0f) * peakLineThickness;
                if (normalized_peak_pos_h > 0.0f && peak_line_actual_width < 1.0f) peak_line_actual_width = 1.0f;

                g.setColour (tickmarkColour);
                g.fillRoundedRectangle (juce::Rectangle<float>(peak_x_pos, fillBar.getY(), peak_line_actual_width, fillBar.getHeight()), 0.0f);
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

void MagicLevelMeter::setRefreshRateHz (int rate)
{
    refreshRateHz = rate;
    startTimerHz (refreshRateHz);
}

void MagicLevelMeter::timerCallback()
{
    repaint();
}

} // namespace foleys
