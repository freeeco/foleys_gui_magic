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

#include "foleys_MagicAnalyser.h"
#include <cmath> // Required for std::pow

namespace foleys
{

MagicAnalyser::MagicAnalyser (int channelToAnalyse)
  : channel (channelToAnalyse)
  , displayRangeMinDb (-90.0f)
  , displayCurveFactor (0.6f)
  , decayMilliseconds (2000)
{
}

void MagicAnalyser::pushSamples (const juce::AudioBuffer<float>& buffer)
{
    analyserJob.pushSamples (buffer, channel);
}

void MagicAnalyser::createPlotPaths (juce::Path& path, juce::Path& filledPath, juce::Rectangle<float> bounds, MagicPlotComponent&)
{
    const float minFreq = 20.0f;
    const auto& data = analyserJob.getAnalyserData();
    const auto* fftData = data.getReadPointer (0);
    const int numBins = data.getNumSamples();
    analyserJob.clearValues();

    // Resize display values if needed
    if (displayValues.getNumSamples() != numBins)
    {
        displayValues.setSize (1, numBins);
        displayValues.clear();
    }

    // Calculate time-based decay
    double now = juce::Time::getMillisecondCounterHiRes();
    float elapsed = (lastRepaintTime > 0.0) ? (float) (now - lastRepaintTime) / 1000.0f : 0.0f;
    lastRepaintTime = now;

    float decayPerSecond = 1.0f / (decayMilliseconds / 1000.0f);
    float decayThisFrame = decayPerSecond * elapsed;

    auto* display = displayValues.getWritePointer (0);

    for (int i = 0; i < numBins; ++i)
    {
        float peak = fftData[i];

        if (peak >= display[i])
        {
            display[i] = peak;
        }
        else
        {
            float currentDb = juce::Decibels::gainToDecibels (display[i], displayRangeMinDb);
            float normalized = juce::jmap (currentDb, displayRangeMinDb, 0.0f, 0.0f, 1.0f);
            float curved = std::pow (juce::jmax (normalized, 0.0f), displayCurveFactor);

            curved -= decayThisFrame;

            if (curved <= 0.0f)
            {
                display[i] = 0.0f;
            }
            else
            {
                float uncurved = std::pow (curved, 1.0f / displayCurveFactor);
                float db = juce::jmap (uncurved, 0.0f, 1.0f, displayRangeMinDb, 0.0f);
                display[i] = juce::Decibels::decibelsToGain (db, displayRangeMinDb);
            }
        }
    }
    
    bool stillDecaying = false;
    for (int i = 0; i < numBins; ++i)
    {
        if (display[i] > 0.0f)
        {
            stillDecaying = true;
            break;
        }
    }

    if (stillDecaying)
        resetLastDataFlag();

    // Draw using decayed display values
    path.clear();
    path.preallocateSpace (8 + numBins * 3);

    juce::ScopedLock lockedForReading (pathCreationLock);

    const float logFreqRange = std::log2 (displayRangeMaxFreq / minFreq);
    const auto factor = bounds.getWidth() / logFreqRange;

    path.startNewSubPath (bounds.getX() + factor * indexToX (0, minFreq), binToY (display[0], bounds));
    for (int i = 1, step = 1, count = 0; i < numBins; i += step, ++count)
    {
        auto avg = display[i];
        if (step > 1)
        {
            for (int j = i + 1; j < std::min (numBins, i + step); ++j)
                avg += display[j];
            avg = avg / step;
        }

        path.lineTo (bounds.getX() + factor * indexToX (i, minFreq), binToY (avg, bounds));

        if (count > 64)
        {
            ++step;
            count = 0;
        }
    }

    filledPath = path;
    filledPath.lineTo (bounds.getBottomRight());
    filledPath.lineTo (bounds.getBottomLeft());
    filledPath.closeSubPath();
}

void MagicAnalyser::prepareToPlay (double sampleRateToUse, int)
{
    sampleRate = sampleRateToUse;
    analyserJob.setupAnalyser (int (sampleRate));
}

juce::TimeSliceClient* MagicAnalyser::getBackgroundJob()
{
    return &analyserJob;
}

float MagicAnalyser::indexToX (int index, float minFreq) const
{
    const auto freq = (sampleRate * index) / analyserJob.fft.getSize();
    return (freq > 0.01f) ? static_cast<float> (std::log2 ((freq + minFreq) / minFreq)) : 0.0f;
}

float MagicAnalyser::binToY (float bin, juce::Rectangle<float> bounds) const
{
    const float minDb = displayRangeMinDb; // e.g., -90.0f
    const float maxDb = 0.0f; // Assuming 0dB is the top of the scale

    float dbValue = juce::Decibels::gainToDecibels (bin, minDb);

    dbValue = juce::jlimit (minDb, maxDb, dbValue);

    float normalizedDb = juce::jmap (dbValue, minDb, maxDb, 0.0f, 1.0f);

    float curvedNormalizedDb = std::pow (normalizedDb, displayCurveFactor);

    return juce::jmap (curvedNormalizedDb, 0.0f, 1.0f, bounds.getBottom(), bounds.getY());
}


//==============================================================================

MagicAnalyser::AnalyserJob::AnalyserJob (MagicAnalyser& ownerToUse)
  : owner (ownerToUse)
{
}

void MagicAnalyser::AnalyserJob::setupAnalyser (int audioFifoSize)
{
    audioFifo.setSize (1, audioFifoSize);
    abstractFifo.setTotalSize (audioFifoSize);

    audioFifo.clear();
    values.clear();
}

void MagicAnalyser::AnalyserJob::pushSamples (const juce::AudioBuffer<float>& buffer, int inChannel)
{
    if (abstractFifo.getFreeSpace() < buffer.getNumSamples())
        return;

    if (inChannel < 0)
    {
        const auto b = abstractFifo.write (buffer.getNumSamples());
        if (b.blockSize1 > 0) audioFifo.copyFrom (0, b.startIndex1, buffer.getReadPointer (0),               b.blockSize1);
        if (b.blockSize2 > 0) audioFifo.copyFrom (0, b.startIndex2, buffer.getReadPointer (0, b.blockSize1), b.blockSize2);

        for (int c = 1; c < audioFifo.getNumChannels(); ++c)
        {
            if (b.blockSize1 > 0) audioFifo.addFrom (0, b.startIndex1, buffer.getReadPointer (c),               b.blockSize1);
            if (b.blockSize2 > 0) audioFifo.addFrom (0, b.startIndex2, buffer.getReadPointer (c, b.blockSize1), b.blockSize2);
        }

        const auto gain = 1.0f / buffer.getNumChannels();
        audioFifo.applyGain (0, b.startIndex1, b.blockSize1, gain);
        audioFifo.applyGain (0, b.startIndex2, b.blockSize2, gain);
    }
    else
    {
        const auto b = abstractFifo.write (buffer.getNumSamples());
        if (b.blockSize1 > 0) audioFifo.copyFrom (0, b.startIndex1, buffer.getReadPointer (inChannel), b.blockSize1);
        if (b.blockSize2 > 0) audioFifo.copyFrom (0, b.blockSize1, audioFifo.getReadPointer (inChannel, b.blockSize1), b.blockSize2);
    }
}

int MagicAnalyser::AnalyserJob::useTimeSlice()
{
    if (abstractFifo.getNumReady() < fft.getSize())
        return 10;

    {
        fftBuffer.clear();

        const auto b = abstractFifo.read (fft.getSize());
        if (b.blockSize1 > 0) fftBuffer.copyFrom (0, 0,            audioFifo.getReadPointer (0, b.startIndex1), b.blockSize1);
        if (b.blockSize2 > 0) fftBuffer.copyFrom (0, b.blockSize1, audioFifo.getReadPointer (0, b.startIndex2), b.blockSize2);
    }

    juce::ScopedNoDenormals noDenormals;

    windowing.multiplyWithWindowingTable (fftBuffer.getWritePointer (0), size_t (fft.getSize()));
    fft.performFrequencyOnlyForwardTransform (fftBuffer.getWritePointer (0));

    {
        juce::ScopedLock lockedForWriting (owner.pathCreationLock);

        const auto  factor = 1.0f / fft.getSize();
        const auto* read  = fftBuffer.getReadPointer (0);
        auto* write = values.getWritePointer (0);

        for (int i = 0; i < values.getNumSamples(); ++i, ++read, ++write)
        {
            *write = *read * factor;
        }

        owner.resetLastDataFlag();
    }

    return 1;
}

const juce::AudioBuffer<float> MagicAnalyser::AnalyserJob::getAnalyserData() const
{
    return values;
}


} // namespace foleys
