#pragma once

#include <JuceHeader.h>
#include "EffectsProcessor.h"
#include "ModularRadioLookAndFeel.h"

//==============================================================================
// Varispeed audio source for smooth real-time pitch/tempo changes (DJ-style)
class VarispeedAudioSource : public juce::PositionableAudioSource
{
public:
    VarispeedAudioSource (juce::PositionableAudioSource* inputSource, bool deleteSourceWhenDeleted)
        : source (inputSource), deleteSource (deleteSourceWhenDeleted)
    {
    }

    ~VarispeedAudioSource() override
    {
        if (deleteSource)
            delete source;
    }

    void setPlaybackRate (double newRate)
    {
        playbackRate = juce::jlimit (0.5, 2.0, newRate);  // Limit to reasonable range
    }

    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override
    {
        if (source != nullptr)
            source->prepareToPlay (static_cast<int>(samplesPerBlockExpected * 2.0 + 10), sampleRate);

        this->sampleRate = sampleRate;
        interpolatorL.reset();
        interpolatorR.reset();
        tempBuffer.setSize (2, static_cast<int>(samplesPerBlockExpected * 2.0 + 10));
    }

    void releaseResources() override
    {
        if (source != nullptr)
            source->releaseResources();
    }

    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override
    {
        if (source == nullptr)
        {
            bufferToFill.clearActiveBufferRegion();
            return;
        }

        auto rate = playbackRate;

        if (std::abs (rate - 1.0) < 0.001)
        {
            // No pitch shift - pass through
            source->getNextAudioBlock (bufferToFill);
            return;
        }

        // Calculate how many input samples we need for the requested output
        auto numOutputSamples = bufferToFill.numSamples;
        auto numInputSamples = static_cast<int> (numOutputSamples * rate) + 10;  // Extra for interpolation

        tempBuffer.setSize (2, numInputSamples, false, false, true);

        juce::AudioSourceChannelInfo tempInfo;
        tempInfo.buffer = &tempBuffer;
        tempInfo.startSample = 0;
        tempInfo.numSamples = numInputSamples;

        source->getNextAudioBlock (tempInfo);

        // Resample using Lagrange interpolation (high quality, smooth)
        auto ratio = 1.0 / rate;  // Input samples per output sample

        auto numChannels = juce::jmin (bufferToFill.buffer->getNumChannels(), 2);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* inputData = tempBuffer.getReadPointer (ch);
            auto* outputData = bufferToFill.buffer->getWritePointer (ch, bufferToFill.startSample);

            auto& interpolator = (ch == 0) ? interpolatorL : interpolatorR;
            interpolator.process (ratio, inputData, outputData, numOutputSamples);
        }

        // Clear any extra channels
        for (int ch = numChannels; ch < bufferToFill.buffer->getNumChannels(); ++ch)
            bufferToFill.buffer->clear (ch, bufferToFill.startSample, bufferToFill.numSamples);
    }

    // PositionableAudioSource methods
    void setNextReadPosition (juce::int64 newPosition) override
    {
        if (source != nullptr)
            source->setNextReadPosition (newPosition);

        interpolatorL.reset();
        interpolatorR.reset();
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
    double playbackRate = 1.0;
    double sampleRate = 44100.0;
    juce::LagrangeInterpolator interpolatorL, interpolatorR;
    juce::AudioBuffer<float> tempBuffer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VarispeedAudioSource)
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
    std::unique_ptr<VarispeedAudioSource> varispeedSource;  // Real-time pitch/tempo control
    juce::AudioTransportSource transportSource;
    double currentPitchRatio = 1.0;  // Turntable-style pitch control (affects pitch + tempo)

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
