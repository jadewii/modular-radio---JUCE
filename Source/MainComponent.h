#pragma once

#include <JuceHeader.h>
#include "EffectsProcessor.h"
#include "ModularRadioLookAndFeel.h"

//==============================================================================
// ResamplingAudioSource wrapper that implements PositionableAudioSource
// This provides smooth, click-free pitch shifting (changes pitch AND tempo like a turntable)
class SmoothResamplingSource : public juce::PositionableAudioSource
{
public:
    SmoothResamplingSource (juce::PositionableAudioSource* inputSource, bool deleteSourceWhenDeleted)
        : source (inputSource),
          deleteSource (deleteSourceWhenDeleted),
          resampler (inputSource, false, 2)  // 2 channels
    {
        // Start at normal pitch (1.0 = no change)
        resampler.setResamplingRatio (1.0);
    }

    ~SmoothResamplingSource() override
    {
        if (deleteSource)
            delete source;
    }

    void setPitchSemitones (double semitones)
    {
        // Convert semitones to playback ratio: ratio = 2^(semitones/12)
        double ratio = std::pow (2.0, semitones / 12.0);

        // Clamp to reasonable range
        ratio = juce::jlimit (0.5, 2.0, ratio);

        // This is inherently smooth - ResamplingAudioSource handles all smoothing internally
        resampler.setResamplingRatio (ratio);
    }

    // AudioSource methods
    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override
    {
        resampler.prepareToPlay (samplesPerBlockExpected, sampleRate);
    }

    void releaseResources() override
    {
        resampler.releaseResources();
    }

    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override
    {
        resampler.getNextAudioBlock (bufferToFill);
    }

    // PositionableAudioSource methods - delegate to source
    void setNextReadPosition (juce::int64 newPosition) override
    {
        if (source != nullptr)
            source->setNextReadPosition (newPosition);
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
    juce::ResamplingAudioSource resampler;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SmoothResamplingSource)
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
    std::unique_ptr<SmoothResamplingSource> pitchShifter;  // Real-time pitch shifting (turntable-style)
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
