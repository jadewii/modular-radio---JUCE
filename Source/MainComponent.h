#pragma once

#include <JuceHeader.h>
#include "EffectsProcessor.h"
#include "ModularRadioLookAndFeel.h"

//==============================================================================
// SoundTouch-based pitch shifter with FIFO buffer for click-free operation
class SoundTouchPitchShifter : public juce::PositionableAudioSource
{
public:
    SoundTouchPitchShifter (juce::PositionableAudioSource* inputSource, bool deleteSourceWhenDeleted)
        : source (inputSource), deleteSource (deleteSourceWhenDeleted)
    {
        // Initialize SoundTouch with high-quality settings to eliminate flutter
        soundTouch.setSampleRate (44100);
        soundTouch.setChannels (2);

        // CRITICAL SETTINGS FOR ELIMINATING FLUTTER:
        // Longer overlap = smoother crossfading = less flutter
        soundTouch.setSetting (SETTING_OVERLAP_MS, 12);  // Longer overlap for smoother processing

        // Longer sequence = less frequent transitions = smoother
        soundTouch.setSetting (SETTING_SEQUENCE_MS, 82);  // Longer sequences

        // Seek window for finding best overlap match
        soundTouch.setSetting (SETTING_SEEKWINDOW_MS, 28);

        // Use best quality settings
        soundTouch.setSetting (SETTING_USE_QUICKSEEK, 0);  // Disable quick seek for best quality
        soundTouch.setSetting (SETTING_USE_AA_FILTER, 1);  // Enable anti-alias filter
    }

    ~SoundTouchPitchShifter() override
    {
        if (deleteSource)
            delete source;
    }

    void setPitchSemitones (double semitones)
    {
        // Smooth pitch changes to avoid clicks
        smoothedPitch.setTargetValue (juce::jlimit (-12.0, 12.0, semitones));
    }

    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override
    {
        if (source != nullptr)
            source->prepareToPlay (samplesPerBlockExpected * 3, sampleRate);  // Extra for buffering

        this->sampleRate = sampleRate;
        soundTouch.setSampleRate (static_cast<uint> (sampleRate));
        soundTouch.clear();

        // Initialize pitch smoothing (5ms ramp to avoid clicks when changing pitch)
        smoothedPitch.reset (sampleRate, 0.005);
        smoothedPitch.setCurrentAndTargetValue (0.0);

        inputBuffer.setSize (2, samplesPerBlockExpected * 3);

        // FIFO buffer for click-free output (large enough to handle SoundTouch latency)
        int fifoSize = samplesPerBlockExpected * 8;
        fifoBuffer.setSize (2, fifoSize);
        fifoBuffer.clear();
        abstractFifo.setTotalSize (fifoSize);

        // Pre-allocate interleaved buffers (larger for extra buffering)
        interleavedInput.resize (samplesPerBlockExpected * 3 * 2);
        interleavedOutput.resize (samplesPerBlockExpected * 3 * 2);

        needsPriming = true;
    }

    void releaseResources() override
    {
        if (source != nullptr)
            source->releaseResources();

        soundTouch.clear();
    }

    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override
    {
        if (source == nullptr)
        {
            bufferToFill.clearActiveBufferRegion();
            return;
        }

        auto pitch = smoothedPitch.getNextValue();

        // Pass through if no pitch shift
        if (std::abs (pitch) < 0.01)
        {
            source->getNextAudioBlock (bufferToFill);
            needsPriming = false;
            return;
        }

        // PRIME SOUNDTOUCH: Feed extra samples initially to fill internal buffer
        if (needsPriming)
        {
            // Feed 4 blocks worth of samples to prime SoundTouch
            for (int i = 0; i < 4; ++i)
            {
                juce::AudioSourceChannelInfo primeInfo;
                primeInfo.buffer = &inputBuffer;
                primeInfo.startSample = 0;
                primeInfo.numSamples = bufferToFill.numSamples;
                source->getNextAudioBlock (primeInfo);

                // Interleave and feed to SoundTouch
                int idx = 0;
                for (int s = 0; s < bufferToFill.numSamples; ++s)
                {
                    interleavedInput[idx++] = inputBuffer.getSample (0, s);
                    interleavedInput[idx++] = inputBuffer.getSample (1, s);
                }
                soundTouch.putSamples (interleavedInput.data(), bufferToFill.numSamples);
            }
            needsPriming = false;
        }

        // Update pitch (smoothed for click-free transitions)
        soundTouch.setPitchSemiTones (pitch);

        // STEP 1: Feed more input to SoundTouch
        int samplesToRead = bufferToFill.numSamples * 2;  // Feed 2x to ensure we get enough output
        juce::AudioSourceChannelInfo inputInfo;
        inputInfo.buffer = &inputBuffer;
        inputInfo.startSample = 0;
        inputInfo.numSamples = juce::jmin (samplesToRead, inputBuffer.getNumSamples());
        source->getNextAudioBlock (inputInfo);

        // Interleave for SoundTouch
        int idx = 0;
        for (int i = 0; i < inputInfo.numSamples; ++i)
        {
            interleavedInput[idx++] = inputBuffer.getSample (0, i);
            interleavedInput[idx++] = inputBuffer.getSample (1, i);
        }
        soundTouch.putSamples (interleavedInput.data(), inputInfo.numSamples);

        // STEP 2: Receive processed samples from SoundTouch
        auto receivedSamples = soundTouch.receiveSamples (interleavedOutput.data(),
                                                          interleavedOutput.size() / 2);

        // STEP 3: Write received samples to FIFO buffer
        if (receivedSamples > 0)
        {
            auto scope = abstractFifo.write (receivedSamples);
            if (scope.blockSize1 > 0)
            {
                for (int i = 0; i < scope.blockSize1; ++i)
                {
                    fifoBuffer.setSample (0, scope.startIndex1 + i, interleavedOutput[i * 2]);
                    fifoBuffer.setSample (1, scope.startIndex1 + i, interleavedOutput[i * 2 + 1]);
                }
            }
            if (scope.blockSize2 > 0)
            {
                for (int i = 0; i < scope.blockSize2; ++i)
                {
                    fifoBuffer.setSample (0, scope.startIndex2 + i,
                                         interleavedOutput[(scope.blockSize1 + i) * 2]);
                    fifoBuffer.setSample (1, scope.startIndex2 + i,
                                         interleavedOutput[(scope.blockSize1 + i) * 2 + 1]);
                }
            }
        }

        // STEP 4: Read from FIFO buffer to output (NEVER output zeros!)
        auto numAvailable = abstractFifo.getNumReady();
        auto numToRead = juce::jmin (bufferToFill.numSamples, numAvailable);

        if (numToRead > 0)
        {
            auto scope = abstractFifo.read (numToRead);
            if (scope.blockSize1 > 0)
            {
                for (int ch = 0; ch < 2; ++ch)
                    bufferToFill.buffer->copyFrom (ch, bufferToFill.startSample,
                                                   fifoBuffer, ch, scope.startIndex1, scope.blockSize1);
            }
            if (scope.blockSize2 > 0)
            {
                for (int ch = 0; ch < 2; ++ch)
                    bufferToFill.buffer->copyFrom (ch, bufferToFill.startSample + scope.blockSize1,
                                                   fifoBuffer, ch, scope.startIndex2, scope.blockSize2);
            }
        }

        // If FIFO didn't have enough, repeat last sample to avoid clicks
        if (numToRead < bufferToFill.numSamples)
        {
            for (int ch = 0; ch < 2; ++ch)
            {
                auto lastSample = bufferToFill.buffer->getSample (ch, bufferToFill.startSample + numToRead - 1);
                for (int i = numToRead; i < bufferToFill.numSamples; ++i)
                    bufferToFill.buffer->setSample (ch, bufferToFill.startSample + i, lastSample);
            }
        }
    }

    // PositionableAudioSource methods
    void setNextReadPosition (juce::int64 newPosition) override
    {
        if (source != nullptr)
            source->setNextReadPosition (newPosition);

        soundTouch.clear();
        abstractFifo.reset();
        fifoBuffer.clear();
        needsPriming = true;  // Re-prime after seek
    }

    juce::int64 getNextReadPosition() const override
    {
        return source != nullptr ? source->getNextReadPosition() : 0;
    }

    juce::int64 getTotalLength() const override
    {
        return source != nullptr ? source->getTotalLength() : 0;
    }

    bool isLooping() const override
    {
        return source != nullptr ? source->isLooping() : false;
    }

private:
    juce::PositionableAudioSource* source;
    bool deleteSource;
    double sampleRate = 44100.0;

    // Smooth pitch changes to avoid clicks
    juce::LinearSmoothedValue<double> smoothedPitch;

    soundtouch::SoundTouch soundTouch;
    juce::AudioBuffer<float> inputBuffer;

    // FIFO buffer for click-free output
    juce::AbstractFifo abstractFifo { 4096 };
    juce::AudioBuffer<float> fifoBuffer;

    std::vector<float> interleavedInput;
    std::vector<float> interleavedOutput;

    bool needsPriming = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SoundTouchPitchShifter)
};

//==============================================================================
// Helper class to make any component draggable with a visible drag handle
class DraggableComponent : public juce::Component
{
public:
    DraggableComponent()
    {
        // Intercept ALL mouse events on this component
        setInterceptsMouseClicks (true, true);
    }

    void paint (juce::Graphics& g) override
    {
        // No visible drag handle - dragging works invisibly
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        // Check if we clicked on a child component (knob, slider, button)
        auto child = getComponentAt (e.getPosition());

        if (child != nullptr && child != this)
        {
            // Clicked on a child - let it handle the event
            isDragging = false;
            auto childEvent = e.getEventRelativeTo (child);
            child->mouseDown (childEvent);
        }
        else
        {
            // Clicked on empty space - start dragging
            dragger.startDraggingComponent (this, e);
            isDragging = true;
        }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (isDragging)
        {
            dragger.dragComponent (this, e, nullptr);
        }
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (isDragging)
        {
            isDragging = false;
            // Notify that dragging ended so position can be saved
            if (onDragEnd)
                onDragEnd();
        }
        else
        {
            // Forward mouseUp to child
            auto child = getComponentAt (e.getPosition());
            if (child != nullptr && child != this)
            {
                auto childEvent = e.getEventRelativeTo (child);
                child->mouseUp (childEvent);
            }
        }
    }

    std::function<void()> onDragEnd;

private:
    juce::ComponentDragger dragger;
    bool isDragging = false;
};

//==============================================================================
class MainComponent : public juce::AudioAppComponent,
                      public juce::ChangeListener,
                      public juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    //==============================================================================
    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

    //==============================================================================
    void paint (juce::Graphics& g) override;
    void resized() override;

    //==============================================================================
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;
    void timerCallback() override;

private:
    //==============================================================================
    // Audio playback
    juce::AudioFormatManager formatManager;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    std::unique_ptr<SoundTouchPitchShifter> pitchShifter;  // Real-time pitch shifting with SoundTouch
    juce::AudioTransportSource transportSource;
    double currentPitchSemitones = 0.0;  // Current pitch in semitones

    // Track management
    juce::Array<juce::File> trackFiles;
    int currentTrackIndex = 0;
    juce::String currentTrackName;

    // Audio parameters
    float masterGain = 0.7f;

    // Professional effects processor
    EffectsProcessor effectsProcessor;

    // Transport controls
    juce::TextButton playButton;
    juce::TextButton stopButton;
    juce::TextButton nextButton;
    juce::TextButton previousButton;

    // Display
    juce::Label brandLabel;
    juce::Label trackNameLabel;
    juce::Label timeLabel;

    // Images
    juce::Image backgroundImage;
    juce::Image moduleImage;

    // Center module controls
    juce::Slider pitchKnob;
    juce::ToggleButton fxToggleButton;
    juce::Label ledIndicator;  // LED shows playback status
    ModularRadioLookAndFeel customLookAndFeel;

    // Draggable wrappers for FX button and LED
    DraggableComponent fxDraggableWrapper;
    DraggableComponent ledDraggableWrapper;

    // Effect controls - custom knob groups (1 knob + 2 sliders each)
    std::unique_ptr<EffectKnobGroup> phaserGroup;
    std::unique_ptr<EffectKnobGroup> delayGroup;
    std::unique_ptr<EffectKnobGroup> chorusGroup;
    std::unique_ptr<EffectKnobGroup> distortionGroup;
    std::unique_ptr<EffectKnobGroup> reverbGroup;
    std::unique_ptr<EffectKnobGroup> filterGroup;
    std::unique_ptr<EffectKnobGroup> timeGroup;

    // Draggable wrappers for all effect groups
    DraggableComponent phaserDraggableWrapper;
    DraggableComponent delayDraggableWrapper;
    DraggableComponent chorusDraggableWrapper;
    DraggableComponent distortionDraggableWrapper;
    DraggableComponent reverbDraggableWrapper;
    DraggableComponent filterDraggableWrapper;
    DraggableComponent timeDraggableWrapper;

    // Filter type buttons
    juce::ToggleButton filterHPButton;
    juce::ToggleButton filterLPButton;
    juce::ToggleButton filterBPButton;

    //==============================================================================
    void loadTrack (int index);
    void loadTracksFromFolder (const juce::File& folder);
    void loadBundledMusic();
    void playButtonClicked();
    void stopButtonClicked();
    void nextButtonClicked();
    void previousButtonClicked();
    void updateTimeDisplay();

    // Position persistence
    void saveComponentPositions();
    void loadComponentPositions();

    enum TransportState
    {
        Stopped,
        Playing,
        Paused
    };

    TransportState state = Stopped;

    // FX button flash
    bool fxButtonFlashing = false;
    int fxButtonFlashCounter = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
