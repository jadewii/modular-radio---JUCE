#pragma once

#include <JuceHeader.h>
#include "SoundTouch/SoundTouch.h"

/**
 * Professional effects processor using JUCE DSP + SoundTouch
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

        // Initialize SoundTouch for pitch shifting and time stretching
        pitchShifter.setSampleRate (static_cast<uint> (spec.sampleRate));
        pitchShifter.setChannels (static_cast<uint> (spec.numChannels));
        pitchShifter.setPitchSemiTones (0.0f);  // No pitch change initially
        pitchShifter.setTempo (1.0f);           // No time stretch (pitch-only mode)
        pitchShifter.setRate (1.0f);            // Playback rate = 1.0
        pitchShifter.setSetting (SETTING_USE_QUICKSEEK, 1);  // Faster processing
        pitchShifter.setSetting (SETTING_USE_AA_FILTER, 1);  // Anti-aliasing

        timeStretcher.setSampleRate (static_cast<uint> (spec.sampleRate));
        timeStretcher.setChannels (static_cast<uint> (spec.numChannels));
        timeStretcher.setPitchSemiTones (0.0f);  // No pitch change initially
        timeStretcher.setTempo (1.0f);           // No time stretch initially
        timeStretcher.setRate (1.0f);
        timeStretcher.setSetting (SETTING_USE_QUICKSEEK, 1);
        timeStretcher.setSetting (SETTING_USE_AA_FILTER, 1);

        // Pre-allocate buffers for SoundTouch processing
        soundTouchBuffer.resize (spec.maximumBlockSize * spec.numChannels);
        soundTouchOutput.resize (spec.maximumBlockSize * spec.numChannels);

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
        pitchShifter.clear();
        timeStretcher.clear();
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
            filter.process (context);

        // Time effect: time-stretch + pitch together
        if (!timeBypassed)
            processTimeStretch (buffer);
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

    // Time effect controls (pitch + time stretch)
    void setTimeStretch (float amount)
    {
        timeStretch = juce::jlimit (0.5f, 2.0f, amount);  // 0.5x to 2x speed
        timeStretcher.setTempo (timeStretch);  // Apply time stretching
    }

    void setTimePitch (float semitones)
    {
        timePitch = juce::jlimit (-12.0f, 12.0f, semitones);
        timeStretcher.setPitchSemiTones (timePitch);  // Apply pitch shift
    }

    void setTimeMix (float mix)
    {
        timeMix = juce::jlimit (0.0f, 1.0f, mix);
    }

    void setTimeBypassed (bool bypassed)
    {
        timeBypassed = bypassed;
    }

private:
    // JUCE DSP effects
    juce::dsp::Phaser<float> phaser;
    juce::dsp::Chorus<float> chorus;
    juce::dsp::Reverb reverb;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayLine;
    juce::dsp::StateVariableTPTFilter<float> filter;
    juce::dsp::WaveShaper<float> distortion;

    // SoundTouch for real-time pitch shifting and time stretching
    soundtouch::SoundTouch pitchShifter;      // Main pitch knob: pitch-only (no time stretch)
    soundtouch::SoundTouch timeStretcher;     // Time effect: time-stretch + pitch together

    // Buffers for SoundTouch processing
    std::vector<float> soundTouchBuffer;
    std::vector<float> soundTouchOutput;

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

    float timeStretch = 1.0f;    // Time stretch amount (0.5x to 2x)
    float timePitch = 0.0f;      // Pitch shift in semitones
    float timeMix = 0.5f;        // Mix amount

    double sampleRate = 44100.0;

    // Bypass states (ALL start bypassed by default - user enables them)
    bool phaserBypassed = true;
    bool delayBypassed = true;
    bool chorusBypassed = true;
    bool distortionBypassed = true;
    bool reverbBypassed = true;
    bool filterBypassed = true;
    bool pitchBypassed = false;  // Main pitch knob is ALWAYS active (not a toggleable effect)
    bool timeBypassed = true;

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

    // Process main pitch shift (pitch-only, no time stretch)
    void processPitchShift (juce::AudioBuffer<float>& buffer)
    {
        int numSamples = buffer.getNumSamples();
        int numChannels = buffer.getNumChannels();

        // Interleave samples for SoundTouch (it expects interleaved stereo)
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* channelData = buffer.getReadPointer (ch);
            for (int i = 0; i < numSamples; ++i)
            {
                soundTouchBuffer[i * numChannels + ch] = channelData[i];
            }
        }

        // Process with SoundTouch
        pitchShifter.putSamples (soundTouchBuffer.data(), numSamples);
        uint receivedSamples = pitchShifter.receiveSamples (soundTouchOutput.data(), numSamples);

        // De-interleave and copy back to buffer
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* channelData = buffer.getWritePointer (ch);
            for (uint i = 0; i < receivedSamples; ++i)
            {
                channelData[i] = soundTouchOutput[i * numChannels + ch];
            }
            // Clear remaining samples if we got fewer than expected
            for (uint i = receivedSamples; i < static_cast<uint>(numSamples); ++i)
            {
                channelData[i] = 0.0f;
            }
        }
    }

    // Process time-stretch + pitch (Time effect)
    void processTimeStretch (juce::AudioBuffer<float>& buffer)
    {
        int numSamples = buffer.getNumSamples();
        int numChannels = buffer.getNumChannels();

        // Store dry signal for mixing
        juce::AudioBuffer<float> dryBuffer (numChannels, numSamples);
        dryBuffer.makeCopyOf (buffer);

        // Interleave samples for SoundTouch
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* channelData = buffer.getReadPointer (ch);
            for (int i = 0; i < numSamples; ++i)
            {
                soundTouchBuffer[i * numChannels + ch] = channelData[i];
            }
        }

        // Process with SoundTouch (time-stretch + pitch)
        timeStretcher.putSamples (soundTouchBuffer.data(), numSamples);
        uint receivedSamples = timeStretcher.receiveSamples (soundTouchOutput.data(), numSamples);

        // De-interleave and mix wet/dry
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* channelData = buffer.getWritePointer (ch);
            auto* dryData = dryBuffer.getReadPointer (ch);

            for (uint i = 0; i < receivedSamples; ++i)
            {
                float wet = soundTouchOutput[i * numChannels + ch];
                channelData[i] = dryData[i] * (1.0f - timeMix) + wet * timeMix;
            }
            // Mix dry signal for remaining samples
            for (uint i = receivedSamples; i < static_cast<uint>(numSamples); ++i)
            {
                channelData[i] = dryData[i];
            }
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EffectsProcessor)
};
