#pragma once

#include <JuceHeader.h>
#include <cmath>

// All-Pass Filter for Phaser
class AllPassFilter
{
public:
    AllPassFilter (float coefficient = 0.5f) : coefficient (coefficient)
    {
    }

    void setCoefficient (float newCoeff)
    {
        coefficient = newCoeff;
    }

    float process (float input)
    {
        float output = -input + buffer;
        buffer = input + (coefficient * output);
        return output;
    }

    void reset()
    {
        buffer = 0.0f;
    }

private:
    float buffer = 0.0f;
    float coefficient;
};

// Professional Phaser with All-Pass Filters
// Direct port from Swift ModularRadio
class PhaserEffect
{
public:
    PhaserEffect()
    {
        // Create 6 all-pass filters for rich phasing
        for (int i = 0; i < 6; ++i)
            allPassFilters.add (new AllPassFilter (0.5f));
    }

    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
        reset();
    }

    float processSample (float input)
    {
        if (bypassed || mix < 0.01f)
            return input;

        // Update LFO phase
        float lfoFreq = 0.1f + (rate * 7.9f);  // 0.1Hz to 8Hz
        lfoPhase += lfoFreq / static_cast<float> (sampleRate);
        if (lfoPhase >= 1.0f)
            lfoPhase -= 1.0f;

        // Calculate LFO value (sine wave)
        float lfoValue = std::sin (lfoPhase * 2.0f * juce::MathConstants<float>::pi);

        // Modulate all-pass filter coefficients
        float minCoeff = 0.3f;
        float maxCoeff = 0.95f;
        float centerCoeff = (minCoeff + maxCoeff) / 2.0f;
        float coeffRange = (maxCoeff - minCoeff) / 2.0f;
        float modulatedCoeff = centerCoeff + (lfoValue * coeffRange * depth);

        // Update filter coefficients
        for (int i = 0; i < allPassFilters.size(); ++i)
        {
            float offset = static_cast<float> (i) / static_cast<float> (allPassFilters.size());
            allPassFilters.getUnchecked (i)->setCoefficient (modulatedCoeff + (offset * 0.1f));
        }

        // Process through all-pass filter chain
        float processedSample = input + (feedbackSample * feedback);
        for (auto* filter : allPassFilters)
            processedSample = filter->process (processedSample);

        feedbackSample = processedSample;

        // Mix wet/dry
        return input * (1.0f - mix) + processedSample * mix;
    }

    void reset()
    {
        lfoPhase = 0.0f;
        feedbackSample = 0.0f;
        for (auto* filter : allPassFilters)
            filter->reset();
    }

    // Parameters (0.0 to 1.0)
    void setRate (float newRate) { rate = juce::jlimit (0.0f, 1.0f, newRate); }
    void setDepth (float newDepth) { depth = juce::jlimit (0.0f, 1.0f, newDepth); }
    void setFeedback (float newFeedback) { feedback = juce::jlimit (0.0f, 0.9f, newFeedback); }
    void setMix (float newMix) { mix = juce::jlimit (0.0f, 1.0f, newMix); }
    void setBypassed (bool shouldBypass) { bypassed = shouldBypass; }

private:
    juce::OwnedArray<AllPassFilter> allPassFilters;
    float lfoPhase = 0.0f;
    double sampleRate = 44100.0;

    // Parameters
    float rate = 0.5f;      // LFO rate (0-1)
    float depth = 0.5f;     // Modulation depth (0-1)
    float feedback = 0.0f;  // Feedback amount (0-0.9)
    float mix = 0.5f;       // Wet/dry mix (0-1)
    bool bypassed = false;

    float feedbackSample = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PhaserEffect)
};
