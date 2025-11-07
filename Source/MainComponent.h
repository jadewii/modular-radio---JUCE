#pragma once

#include <JuceHeader.h>
#include "EffectsProcessor.h"
#include "ModularRadioLookAndFeel.h"

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
    juce::AudioTransportSource transportSource;

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
    ModularRadioLookAndFeel customLookAndFeel;

    // Effect controls - custom knob groups (1 knob + 2 sliders each)
    std::unique_ptr<EffectKnobGroup> phaserGroup;
    std::unique_ptr<EffectKnobGroup> delayGroup;
    std::unique_ptr<EffectKnobGroup> chorusGroup;
    std::unique_ptr<EffectKnobGroup> distortionGroup;
    std::unique_ptr<EffectKnobGroup> reverbGroup;
    std::unique_ptr<EffectKnobGroup> filterGroup;

    //==============================================================================
    void loadTrack (int index);
    void loadTracksFromFolder (const juce::File& folder);
    void loadBundledMusic();
    void playButtonClicked();
    void stopButtonClicked();
    void nextButtonClicked();
    void previousButtonClicked();
    void updateTimeDisplay();

    enum TransportState
    {
        Stopped,
        Playing,
        Paused
    };

    TransportState state = Stopped;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
