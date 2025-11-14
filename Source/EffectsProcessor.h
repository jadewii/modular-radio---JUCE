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

        // Bitcrusher needs no preparation - simple algorithm
        bitcrusherHoldSample[0] = 0.0f;
        bitcrusherHoldSample[1] = 0.0f;
        bitcrusherCounter = 0;

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

        // Reset bitcrusher state
        bitcrusherHoldSample[0] = 0.0f;
        bitcrusherHoldSample[1] = 0.0f;
        bitcrusherCounter = 0;
    }

    void process (juce::AudioBuffer<float>& buffer)
    {
        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> context (block);

        // Process through effects chain
        // Order: Phaser → Delay → Chorus → Distortion → Reverb → Filter → Time Effect
        // Note: Main pitch knob is handled at source level via ResamplingAudioSource

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
        {
            filter.process (context);
            // Apply gain to filter output
            block.multiplyBy (filterGain);
        }

        // Bitcrusher effect: lo-fi digital degradation
        if (!bitcrusherBypassed)
            processBitcrusher (buffer);
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
        if (bypassed != phaserBypassed)
        {
            phaserBypassed = bypassed;
            phaser.reset();  // Clear internal state to prevent pops
        }
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
        if (bypassed != delayBypassed)
        {
            delayBypassed = bypassed;
            delayLine.reset();  // Clear delay buffer to prevent pops
        }
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
        if (bypassed != chorusBypassed)
        {
            chorusBypassed = bypassed;
            chorus.reset();  // Clear internal state to prevent pops
        }
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
        if (bypassed != distortionBypassed)
        {
            distortionBypassed = bypassed;
            distortion.reset();  // Clear internal state to prevent pops
        }
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
        if (bypassed != reverbBypassed)
        {
            reverbBypassed = bypassed;
            reverb.reset();  // Clear reverb tail to prevent pops
        }
    }

    // Filter controls
    void setFilterCutoff (float cutoff)
    {
        // Store normalized value for when filter type changes
        filterCutoffNormalized = juce::jlimit (0.0f, 1.0f, cutoff);

        // Different mapping based on filter type for intuitive control
        switch (filterType)
        {
            case 0:  // Low-pass: 0=muffled(100Hz), 1=open(20kHz)
                filterCutoff = 100.0f * std::pow (200.0f, filterCutoffNormalized);
                break;
            case 1:  // High-pass: 0=open(20Hz), 1=thin(10kHz) - REVERSED for natural feel
                filterCutoff = 20.0f * std::pow (500.0f, 1.0f - filterCutoffNormalized);  // Inverted mapping
                break;
            case 2:  // Band-pass: 0=low band, 1=high band
                filterCutoff = 100.0f * std::pow (200.0f, filterCutoffNormalized);
                break;
        }
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
        if (type != filterType)
        {
            filterType = type;
            // Recalculate cutoff frequency with new type-specific mapping
            setFilterCutoff (filterCutoffNormalized);  // Use stored normalized value
            updateFilter();
            filter.reset();  // Clear filter state to prevent pops when changing type
        }
    }

    void setFilterGain (float gain)
    {
        filterGain = 0.1f + gain * 1.9f;  // 0.1x to 2.0x gain
    }

    void setFilterBypassed (bool bypassed)
    {
        if (bypassed != filterBypassed)
        {
            filterBypassed = bypassed;
            filter.reset();  // Clear filter state to prevent pops
        }
    }

    // Pitch shift controls (for main knob) - PITCH ONLY, NO TIME STRETCH
    // NOTE: Main pitch knob now handled in MainComponent via ResamplingAudioSource
    // This is instant and artifact-free for real-time DJ-style control
    void setPitchShift (float semitones)
    {
        // This is now a no-op - pitch is handled at the source level
        pitchShiftSemitones = juce::jlimit (-12.0f, 12.0f, semitones);
    }

    void setPitchBypassed (bool bypassed)
    {
        pitchBypassed = bypassed;
    }

    float getPitchShiftRatio() const
    {
        // Convert semitones to playback ratio: 2^(semitones/12)
        return std::pow (2.0f, pitchShiftSemitones / 12.0f);
    }

    // Bitcrusher controls
    void setBitcrusherBitDepth (float depth)
    {
        // Map 0-1 to 16 bits down to 1 bit
        bitcrusherBitDepth = 1.0f + depth * 15.0f;  // 1 to 16 bits
    }

    void setBitcrusherCrush (float crush)
    {
        // Map 0-1 to 1x to 32x sample rate reduction
        bitcrusherCrush = 1.0f + crush * 31.0f;  // 1 to 32
    }

    void setBitcrusherMix (float mix)
    {
        bitcrusherMix = juce::jlimit (0.0f, 1.0f, mix);
    }

    void setBitcrusherBypassed (bool bypassed)
    {
        if (bypassed != bitcrusherBypassed)
        {
            bitcrusherBypassed = bypassed;
            // Reset bitcrusher state
            bitcrusherHoldSample[0] = 0.0f;
            bitcrusherHoldSample[1] = 0.0f;
            bitcrusherCounter = 0;
        }
    }

private:
    // JUCE DSP effects
    juce::dsp::Phaser<float> phaser;
    juce::dsp::Chorus<float> chorus;
    juce::dsp::Reverb reverb;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayLine;
    juce::dsp::StateVariableTPTFilter<float> filter;
    juce::dsp::WaveShaper<float> distortion;

    // BITCRUSHER effect - lo-fi digital degradation
    float bitcrusherHoldSample[2] = {0.0f, 0.0f};  // Held samples for sample rate reduction
    int bitcrusherCounter = 0;  // Counter for sample rate decimation

    // Effect parameters
    juce::Reverb::Parameters reverbParams;

    float delayTime = 0.5f;
    float delayFeedback = 0.3f;
    float delayMix = 0.5f;

    float distortionDrive = 1.0f;
    float distortionMix = 0.5f;

    float filterCutoff = 1000.0f;
    float filterCutoffNormalized = 0.5f;  // Stored normalized value (0-1) for type switching
    float filterResonance = 1.0f;
    float filterGain = 1.0f;     // Output gain (0.1 to 2.0)
    int filterType = 0;  // 0=LP, 1=HP, 2=BP

    float pitchShiftSemitones = 0.0f;

    float bitcrusherBitDepth = 16.0f;     // Bit depth (1 to 16 bits)
    float bitcrusherCrush = 1.0f;         // Sample rate reduction factor (1 to 32)
    float bitcrusherMix = 0.5f;           // Mix amount

    double sampleRate = 44100.0;

    // Bypass states (ALL start bypassed by default - user enables them)
    bool phaserBypassed = true;
    bool delayBypassed = true;
    bool chorusBypassed = true;
    bool distortionBypassed = true;
    bool reverbBypassed = true;
    bool filterBypassed = true;
    bool pitchBypassed = false;  // Main pitch knob is ALWAYS active (not a toggleable effect)
    bool bitcrusherBypassed = true;

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
        if (distortionMix < 0.01f)
            return;  // Bypassed - do nothing

        // Store REAL dry signal BEFORE any processing
        juce::AudioBuffer<float> dryBuffer (buffer.getNumChannels(), buffer.getNumSamples());
        dryBuffer.makeCopyOf (buffer);

        // Apply drive to buffer (this will become the wet signal)
        juce::dsp::AudioBlock<float> block (buffer);
        block.multiplyBy (distortionDrive);

        // Apply waveshaping to driven signal
        juce::dsp::ProcessContextReplacing<float> context (block);
        distortion.process (context);

        // Mix dry (normal level) with wet (distorted and loud)
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* output = buffer.getWritePointer (ch);
            auto* dry = dryBuffer.getReadPointer (ch);

            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                // Wet signal is already amplified by distortionDrive and distorted
                // Mix with original dry signal at normal level
                output[i] = dry[i] * (1.0f - distortionMix) + output[i] * distortionMix;
            }
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

    // Process BITCRUSHER effect - lo-fi digital degradation
    void processBitcrusher (juce::AudioBuffer<float>& buffer)
    {
        int numSamples = buffer.getNumSamples();
        int numChannels = buffer.getNumChannels();

        // Store dry signal for mixing
        juce::AudioBuffer<float> dryBuffer (numChannels, numSamples);
        dryBuffer.makeCopyOf (buffer);

        // Calculate quantization step size based on bit depth
        float levels = std::pow (2.0f, bitcrusherBitDepth);  // e.g., 16 bits = 65536 levels
        float stepSize = 2.0f / levels;  // Audio range is -1.0 to +1.0

        int crushFactor = static_cast<int> (bitcrusherCrush);  // Sample rate reduction factor

        // Process each channel
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* channelData = buffer.getWritePointer (ch);

            for (int i = 0; i < numSamples; ++i)
            {
                // Sample rate reduction (hold samples)
                if (bitcrusherCounter == 0)
                {
                    // Update held sample
                    float input = channelData[i];

                    // Bit depth reduction (quantization)
                    float quantized = std::floor (input / stepSize) * stepSize;
                    quantized = juce::jlimit (-1.0f, 1.0f, quantized);

                    bitcrusherHoldSample[ch] = quantized;
                }

                // Output the held sample
                channelData[i] = bitcrusherHoldSample[ch];

                // Increment counter
                bitcrusherCounter = (bitcrusherCounter + 1) % crushFactor;
            }
        }

        // Mix wet/dry
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* out = buffer.getWritePointer (ch);
            auto* dry = dryBuffer.getReadPointer (ch);

            for (int i = 0; i < numSamples; ++i)
            {
                out[i] = dry[i] * (1.0f - bitcrusherMix) + out[i] * bitcrusherMix;
            }
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EffectsProcessor)
};
