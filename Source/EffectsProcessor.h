#pragma once

#include <JuceHeader.h>

/**
 * Professional effects processor using JUCE DSP
 * Matches the ModularRadio Swift app effect chain
 */
class EffectsProcessor
{
public:
    EffectsProcessor()
    {
        // Initialize reverb parameters
        reverbParams.roomSize = 0.8f;
        reverbParams.damping = 0.5f;
        reverbParams.wetLevel = 0.0f;
        reverbParams.dryLevel = 1.0f;
        reverbParams.width = 1.0f;
        reverbParams.freezeMode = 0.0f;
    }

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        // Prepare all effects with the audio spec
        phaser.prepare (spec);
        chorus.prepare (spec);
        reverb.prepare (spec);

        // Delay line setup (max 3 seconds)
        delayLine.prepare (spec);
        delayLine.setMaximumDelayInSamples (static_cast<int> (spec.sampleRate * 3.0));

        // Filter setup
        filter.prepare (spec);
        filter.reset();

        // Distortion/waveshaper setup
        distortion.prepare (spec);
        distortion.functionToUse = [] (float x)
        {
            // Soft clipping distortion
            return std::tanh (x);
        };

        sampleRate = spec.sampleRate;
    }

    void reset()
    {
        phaser.reset();
        chorus.reset();
        reverb.reset();
        delayLine.reset();
        filter.reset();
        distortion.reset();
    }

    void process (juce::AudioBuffer<float>& buffer)
    {
        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> context (block);

        // Process through effects chain
        // Order: Phaser → Delay → Chorus → Distortion → Reverb → Filter

        if (!phaserBypassed)
            phaser.process (context);

        if (!delayBypassed)
            processDelay (buffer);

        if (!chorusBypassed)
            chorus.process (context);

        if (!distortionBypassed)
            processDistortion (buffer);

        if (!reverbBypassed)
            reverb.process (context);

        if (!filterBypassed)
            filter.process (context);
    }

    // Phaser controls
    void setPhaserRate (float rate)
    {
        phaser.setRate (0.1f + rate * 9.9f);  // 0.1Hz to 10Hz
    }

    void setPhaserDepth (float depth)
    {
        phaser.setDepth (depth);
    }

    void setPhaserMix (float mix)
    {
        phaser.setMix (mix);
    }

    void setPhaserBypassed (bool bypassed)
    {
        phaserBypassed = bypassed;
    }

    void setPhaserFeedback (float feedback)
    {
        phaser.setFeedback (feedback);
    }

    // Delay controls
    void setDelayTime (float seconds)
    {
        delayTime = juce::jlimit (0.0f, 3.0f, seconds);
    }

    void setDelayFeedback (float feedback)
    {
        delayFeedback = juce::jlimit (0.0f, 0.95f, feedback);
    }

    void setDelayMix (float mix)
    {
        delayMix = juce::jlimit (0.0f, 1.0f, mix);
    }

    void setDelayBypassed (bool bypassed)
    {
        delayBypassed = bypassed;
    }

    // Chorus controls
    void setChorusRate (float rate)
    {
        chorus.setRate (rate * 10.0f);  // 0-10 Hz
    }

    void setChorusDepth (float depth)
    {
        chorus.setDepth (depth);
    }

    void setChorusMix (float mix)
    {
        chorus.setMix (mix);
    }

    void setChorusBypassed (bool bypassed)
    {
        chorusBypassed = bypassed;
    }

    void setChorusFeedback (float feedback)
    {
        chorus.setFeedback (feedback);
    }

    // Distortion controls
    void setDistortionDrive (float drive)
    {
        distortionDrive = 1.0f + drive * 10.0f;  // 1x to 11x gain
    }

    void setDistortionMix (float mix)
    {
        distortionMix = juce::jlimit (0.0f, 1.0f, mix);
    }

    void setDistortionBypassed (bool bypassed)
    {
        distortionBypassed = bypassed;
    }

    // Reverb controls
    void setReverbSize (float size)
    {
        reverbParams.roomSize = size;
        reverb.setParameters (reverbParams);
    }

    void setReverbDamping (float damping)
    {
        reverbParams.damping = damping;
        reverb.setParameters (reverbParams);
    }

    void setReverbMix (float mix)
    {
        reverbParams.wetLevel = mix;
        reverbParams.dryLevel = 1.0f - mix;
        reverb.setParameters (reverbParams);
    }

    void setReverbBypassed (bool bypassed)
    {
        reverbBypassed = bypassed;
    }

    // Filter controls
    void setFilterCutoff (float cutoff)
    {
        // Map 0-1 to 100Hz-20kHz logarithmically
        filterCutoff = 100.0f * std::pow (200.0f, cutoff);
        updateFilter();
    }

    void setFilterResonance (float resonance)
    {
        filterResonance = 0.5f + resonance * 9.5f;  // 0.5 to 10.0
        updateFilter();
    }

    void setFilterType (int type)
    {
        // 0 = Low-pass, 1 = High-pass, 2 = Band-pass
        filterType = type;
        updateFilter();
    }

    void setFilterBypassed (bool bypassed)
    {
        filterBypassed = bypassed;
    }

    // Pitch shift controls
    void setPitchShift (float semitones)
    {
        pitchShiftSemitones = juce::jlimit (-12.0f, 12.0f, semitones);
        // TODO: Implement real pitch shifting without time-stretch
        // For now, this is a placeholder
    }

    void setPitchBypassed (bool bypassed)
    {
        pitchBypassed = bypassed;
    }

private:
    // JUCE DSP effects
    juce::dsp::Phaser<float> phaser;
    juce::dsp::Chorus<float> chorus;
    juce::dsp::Reverb reverb;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayLine;
    juce::dsp::StateVariableTPTFilter<float> filter;
    juce::dsp::WaveShaper<float> distortion;

    // Effect parameters
    juce::Reverb::Parameters reverbParams;

    float delayTime = 0.5f;
    float delayFeedback = 0.3f;
    float delayMix = 0.5f;

    float distortionDrive = 1.0f;
    float distortionMix = 0.5f;

    float filterCutoff = 1000.0f;
    float filterResonance = 1.0f;
    int filterType = 0;  // 0=LP, 1=HP, 2=BP

    float pitchShiftSemitones = 0.0f;

    double sampleRate = 44100.0;

    // Bypass states (all start bypassed except phaser which has UI controls)
    bool phaserBypassed = false;  // Enabled by default, controlled by UI
    bool delayBypassed = true;    // Disabled until we add UI
    bool chorusBypassed = true;
    bool distortionBypassed = true;
    bool reverbBypassed = true;
    bool filterBypassed = true;
    bool pitchBypassed = true;

    // Helper functions
    void processDelay (juce::AudioBuffer<float>& buffer)
    {
        int numSamples = buffer.getNumSamples();
        int delaySamples = static_cast<int> (delayTime * sampleRate);

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            auto* channelData = buffer.getWritePointer (channel);

            for (int i = 0; i < numSamples; ++i)
            {
                float input = channelData[i];
                float delayed = delayLine.popSample (channel);

                // Write input + feedback to delay line
                delayLine.pushSample (channel, input + delayed * delayFeedback);
                delayLine.setDelay (static_cast<float> (delaySamples));

                // Mix wet/dry
                channelData[i] = input * (1.0f - delayMix) + delayed * delayMix;
            }
        }
    }

    void processDistortion (juce::AudioBuffer<float>& buffer)
    {
        juce::dsp::AudioBlock<float> block (buffer);

        // Apply drive
        block.multiplyBy (distortionDrive);

        // Apply waveshaping
        juce::dsp::ProcessContextReplacing<float> context (block);

        if (distortionMix > 0.01f)
        {
            // Store dry signal
            juce::AudioBuffer<float> dryBuffer (buffer.getNumChannels(), buffer.getNumSamples());
            dryBuffer.makeCopyOf (buffer);

            // Process wet signal
            distortion.process (context);

            // Mix dry/wet
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            {
                auto* wet = buffer.getWritePointer (ch);
                auto* dry = dryBuffer.getReadPointer (ch);

                for (int i = 0; i < buffer.getNumSamples(); ++i)
                {
                    wet[i] = dry[i] * (1.0f - distortionMix) + wet[i] * distortionMix;
                }
            }

            // Compensate for drive gain
            block.multiplyBy (1.0f / distortionDrive);
        }
        else
        {
            block.multiplyBy (1.0f / distortionDrive);
        }
    }

    void updateFilter()
    {
        using FilterType = juce::dsp::StateVariableTPTFilterType;

        switch (filterType)
        {
            case 0:
                filter.setType (FilterType::lowpass);
                break;
            case 1:
                filter.setType (FilterType::highpass);
                break;
            case 2:
                filter.setType (FilterType::bandpass);
                break;
        }

        filter.setCutoffFrequency (filterCutoff);
        filter.setResonance (filterResonance);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EffectsProcessor)
};
