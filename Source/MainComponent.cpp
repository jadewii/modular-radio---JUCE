#include "MainComponent.h"

MainComponent::MainComponent()
{
    // Register audio formats
    formatManager.registerBasicFormats();
    transportSource.addChangeListener (this);

    // Load images
    auto resourcesFolder = juce::File::getSpecialLocation (juce::File::currentApplicationFile)
                               .getChildFile ("Contents")
                               .getChildFile ("Resources")
                               .getChildFile ("Resources");

    auto bgImageFile = resourcesFolder.getChildFile ("modularradio-back.png");
    if (bgImageFile.existsAsFile())
    {
        backgroundImage = juce::ImageFileFormat::loadFrom (bgImageFile);
        DBG ("Background image loaded: " << bgImageFile.getFullPathName());
    }
    else
    {
        DBG ("Background image NOT found at: " << bgImageFile.getFullPathName());
    }

    auto moduleImageFile = resourcesFolder.getChildFile ("modularapp.PNG");
    if (moduleImageFile.existsAsFile())
    {
        moduleImage = juce::ImageFileFormat::loadFrom (moduleImageFile);
        DBG ("Module image loaded: " << moduleImageFile.getFullPathName());
    }
    else
    {
        DBG ("Module image NOT found at: " << moduleImageFile.getFullPathName());
    }

    // Transport buttons (matching SwiftUI design)
    playButton.setButtonText ("Play");
    playButton.onClick = [this] { playButtonClicked(); };
    playButton.setLookAndFeel (&customLookAndFeel);
    addAndMakeVisible (playButton);

    stopButton.setButtonText ("Stop");
    stopButton.onClick = [this] { stopButtonClicked(); };
    addAndMakeVisible (stopButton);

    nextButton.setButtonText ("Next");
    nextButton.onClick = [this] { nextButtonClicked(); };
    nextButton.setLookAndFeel (&customLookAndFeel);
    addAndMakeVisible (nextButton);

    previousButton.setButtonText ("Previous");
    previousButton.onClick = [this] { previousButtonClicked(); };
    previousButton.setLookAndFeel (&customLookAndFeel);
    addAndMakeVisible (previousButton);

    // Track labels
    trackNameLabel.setText ("No track loaded", juce::dontSendNotification);
    trackNameLabel.setJustificationType (juce::Justification::centred);
    trackNameLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (trackNameLabel);

    // Time label - HIDDEN (not added to view)

    // Pitch knob (center module)
    pitchKnob.setSliderStyle (juce::Slider::Rotary);
    pitchKnob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    pitchKnob.setRange (-12.0, 12.0, 0.1);
    pitchKnob.setValue (0.0);
    pitchKnob.setLookAndFeel (&customLookAndFeel);
    pitchKnob.onValueChange = [this] { effectsProcessor.setPitchShift (pitchKnob.getValue()); };
    addAndMakeVisible (pitchKnob);

    // FX On/Off toggle button
    fxToggleButton.setButtonText ("FX");
    fxToggleButton.setClickingTogglesState (true);
    fxToggleButton.setToggleState (true, juce::dontSendNotification);
    fxToggleButton.onClick = [this] {
        bool enabled = fxToggleButton.getToggleState();
        effectsProcessor.setPhaserBypassed (!enabled);
        effectsProcessor.setDelayBypassed (!enabled);
        effectsProcessor.setChorusBypassed (!enabled);
        effectsProcessor.setDistortionBypassed (!enabled);
        effectsProcessor.setReverbBypassed (!enabled);
        effectsProcessor.setFilterBypassed (!enabled);
    };
    addAndMakeVisible (fxToggleButton);

    // Create effect knob groups (1 knob + 2 sliders each)
    // Phaser: Knob=Rate, Sliders: Depth, Mix
    phaserGroup = std::make_unique<EffectKnobGroup> ("Phaser", "DEPTH", "MIX",
        juce::Colours::cyan,
        [this](float v) { effectsProcessor.setPhaserRate(v); },
        [this](float v) { effectsProcessor.setPhaserDepth(v); },
        [this](float v) { effectsProcessor.setPhaserMix(v); });
    phaserGroup->setBypassCallback ([this](bool bypassed) { effectsProcessor.setPhaserBypassed(bypassed); });
    phaserGroup->getBypassButton().setToggleState (false, juce::dontSendNotification);
    addAndMakeVisible (phaserGroup.get());

    // Delay: Knob=Mix, Sliders: Time, Feedback
    delayGroup = std::make_unique<EffectKnobGroup> ("Delay", "TIME", "FDBK",
        juce::Colours::red,
        [this](float v) { effectsProcessor.setDelayMix(v); },
        [this](float v) { effectsProcessor.setDelayTime(v * 3.0f); },
        [this](float v) { effectsProcessor.setDelayFeedback(v * 0.95f); });
    delayGroup->setBypassCallback ([this](bool bypassed) { effectsProcessor.setDelayBypassed(bypassed); });
    addAndMakeVisible (delayGroup.get());

    // Chorus: Knob=Rate, Sliders: Depth, Mix
    chorusGroup = std::make_unique<EffectKnobGroup> ("Chorus", "DEPTH", "MIX",
        juce::Colours::blue,
        [this](float v) { effectsProcessor.setChorusRate(v); },
        [this](float v) { effectsProcessor.setChorusDepth(v); },
        [this](float v) { effectsProcessor.setChorusMix(v); });
    chorusGroup->setBypassCallback ([this](bool bypassed) { effectsProcessor.setChorusBypassed(bypassed); });
    addAndMakeVisible (chorusGroup.get());

    // Distortion: Knob=Drive, Sliders: Mix, (empty)
    distortionGroup = std::make_unique<EffectKnobGroup> ("Distortion", "MIX", "",
        juce::Colours::orange,
        [this](float v) { effectsProcessor.setDistortionDrive(v); },
        [this](float v) { effectsProcessor.setDistortionMix(v); },
        [this](float v) { });
    distortionGroup->setBypassCallback ([this](bool bypassed) { effectsProcessor.setDistortionBypassed(bypassed); });
    addAndMakeVisible (distortionGroup.get());

    // Reverb: Knob=Mix, Sliders: Size, Damping
    reverbGroup = std::make_unique<EffectKnobGroup> ("Reverb", "SIZE", "DAMP",
        juce::Colours::yellow,
        [this](float v) { effectsProcessor.setReverbMix(v); },
        [this](float v) { effectsProcessor.setReverbSize(v); },
        [this](float v) { effectsProcessor.setReverbDamping(v); });
    reverbGroup->setBypassCallback ([this](bool bypassed) { effectsProcessor.setReverbBypassed(bypassed); });
    addAndMakeVisible (reverbGroup.get());

    // Filter: Knob=Cutoff, Sliders: Resonance, Type
    filterGroup = std::make_unique<EffectKnobGroup> ("Filter", "RESO", "TYPE",
        juce::Colours::green,
        [this](float v) { effectsProcessor.setFilterCutoff(v); },
        [this](float v) { effectsProcessor.setFilterResonance(v); },
        [this](float v) { effectsProcessor.setFilterType(static_cast<int>(v * 2.0f)); });
    filterGroup->setBypassCallback ([this](bool bypassed) { effectsProcessor.setFilterBypassed(bypassed); });
    addAndMakeVisible (filterGroup.get());

    // Load bundled music
    loadBundledMusic();

    // Start timer
    startTimer (100);

    setSize (1200, 834);
    setAudioChannels (0, 2);
}

MainComponent::~MainComponent()
{
    pitchKnob.setLookAndFeel (nullptr);
    playButton.setLookAndFeel (nullptr);
    nextButton.setLookAndFeel (nullptr);
    previousButton.setLookAndFeel (nullptr);
    shutdownAudio();
}

void MainComponent::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    transportSource.prepareToPlay (samplesPerBlockExpected, sampleRate);

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32> (samplesPerBlockExpected);
    spec.numChannels = 2;

    effectsProcessor.prepare (spec);

    // Initialize from knob values
    effectsProcessor.setPhaserRate (phaserGroup->getKnob().getValue());
    effectsProcessor.setPhaserDepth (phaserGroup->getSlider1().getValue());
    effectsProcessor.setPhaserMix (phaserGroup->getSlider2().getValue());

    effectsProcessor.setDelayMix (delayGroup->getKnob().getValue());
    effectsProcessor.setDelayTime (delayGroup->getSlider1().getValue() * 3.0f);
    effectsProcessor.setDelayFeedback (delayGroup->getSlider2().getValue() * 0.95f);

    effectsProcessor.setChorusRate (chorusGroup->getKnob().getValue());
    effectsProcessor.setChorusDepth (chorusGroup->getSlider1().getValue());
    effectsProcessor.setChorusMix (chorusGroup->getSlider2().getValue());

    effectsProcessor.setDistortionDrive (distortionGroup->getKnob().getValue());
    effectsProcessor.setDistortionMix (distortionGroup->getSlider1().getValue());

    effectsProcessor.setReverbMix (reverbGroup->getKnob().getValue());
    effectsProcessor.setReverbSize (reverbGroup->getSlider1().getValue());
    effectsProcessor.setReverbDamping (reverbGroup->getSlider2().getValue());

    effectsProcessor.setFilterCutoff (filterGroup->getKnob().getValue());
    effectsProcessor.setFilterResonance (filterGroup->getSlider1().getValue());
    effectsProcessor.setFilterType (static_cast<int>(filterGroup->getSlider2().getValue() * 2.0f));

    DBG ("Audio prepared: " << sampleRate << " Hz");
}

void MainComponent::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill)
{
    if (readerSource.get() == nullptr)
    {
        bufferToFill.clearActiveBufferRegion();
        return;
    }

    transportSource.getNextAudioBlock (bufferToFill);
    effectsProcessor.process (*bufferToFill.buffer);
}

void MainComponent::releaseResources()
{
    transportSource.releaseResources();
    effectsProcessor.reset();
}

void MainComponent::paint (juce::Graphics& g)
{
    // Draw background image (cables/patch cords)
    if (backgroundImage.isValid())
    {
        g.drawImage (backgroundImage, getLocalBounds().toFloat(),
                    juce::RectanglePlacement::fillDestination);
    }
    else
    {
        g.fillAll (juce::Colour (0xff2d2d2d));
    }

    // Draw center module overlay
    if (moduleImage.isValid())
    {
        // Center the module image
        auto bounds = getLocalBounds();
        auto moduleWidth = 480;
        auto moduleHeight = 640;
        auto moduleX = (bounds.getWidth() - moduleWidth) / 2;
        auto moduleY = (bounds.getHeight() - moduleHeight) / 2;

        g.drawImage (moduleImage, moduleX, moduleY, moduleWidth, moduleHeight,
                    0, 0, moduleImage.getWidth(), moduleImage.getHeight());
    }
}

void MainComponent::resized()
{
    auto bounds = getLocalBounds();
    auto centerX = bounds.getWidth() / 2;
    auto centerY = bounds.getHeight() / 2;

    // Center module bounds (480x640)
    auto moduleX = centerX - 240;
    auto moduleY = centerY - 320;

    // Pitch knob - centered in module (30% BIGGER!)
    pitchKnob.setBounds (moduleX + 123, moduleY + 153, 234, 234);  // Was 180x180, now 234x234 (30% larger)

    // Transport controls - centered below module (matching SwiftUI)
    auto transportY = moduleY + 484;  // Exact position from SwiftUI
    previousButton.setBounds (centerX - 110, transportY, 50, 50);  // Left of center
    playButton.setBounds (centerX - 30, transportY, 60, 60);       // Center (bigger)
    nextButton.setBounds (centerX + 60, transportY, 50, 50);       // Right of center
    stopButton.setBounds (0, 0, 0, 0); // Hide stop button

    // FX toggle - top right of module
    fxToggleButton.setBounds (moduleX + 380, moduleY + 180, 60, 30);

    // Track name - centered in bottom area of module
    trackNameLabel.setBounds (moduleX + 60, moduleY + 600, 360, 30);

    // Position effect knob groups around edges on background (BIGGER with LONGER sliders!)
    // Left side
    filterGroup->setBounds (10, 60, 320, 180);    // Wider for longer sliders!
    delayGroup->setBounds (10, 340, 320, 180);
    reverbGroup->setBounds (10, 620, 320, 180);

    // Right side
    chorusGroup->setBounds (870, 60, 320, 180);   // Adjusted x position
    distortionGroup->setBounds (870, 340, 320, 180);
    phaserGroup->setBounds (870, 620, 320, 180);
}

void MainComponent::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    if (source == &transportSource)
    {
        if (transportSource.isPlaying())
            state = Playing;
        else if (state == Playing)
            nextButtonClicked();
    }
}

void MainComponent::timerCallback()
{
    updateTimeDisplay();
}

void MainComponent::loadTrack (int index)
{
    if (index < 0 || index >= trackFiles.size())
        return;

    transportSource.stop();
    transportSource.setSource (nullptr);
    readerSource.reset();

    auto file = trackFiles[index];
    auto* reader = formatManager.createReaderFor (file);

    if (reader != nullptr)
    {
        readerSource.reset (new juce::AudioFormatReaderSource (reader, true));
        transportSource.setSource (readerSource.get(), 0, nullptr, reader->sampleRate);

        currentTrackIndex = index;
        currentTrackName = file.getFileNameWithoutExtension();
        trackNameLabel.setText (currentTrackName, juce::dontSendNotification);

        DBG ("Loaded: " << currentTrackName);
    }
}

void MainComponent::loadTracksFromFolder (const juce::File& folder)
{
    trackFiles.clear();

    juce::Array<juce::File> files;
    folder.findChildFiles (files, juce::File::findFiles, true, "*.mp3;*.wav;*.aiff;*.aif;*.m4a;*.flac");

    trackFiles.addArray (files);

    DBG ("Loaded " << trackFiles.size() << " tracks");

    if (!trackFiles.isEmpty())
        loadTrack (0);
}

void MainComponent::loadBundledMusic()
{
    auto resourcesFolder = juce::File::getSpecialLocation (juce::File::currentApplicationFile)
                               .getChildFile ("Contents")
                               .getChildFile ("Resources")
                               .getChildFile ("Resources")
                               .getChildFile ("Modular Radio - All Tracks");

    if (resourcesFolder.exists() && resourcesFolder.isDirectory())
    {
        loadTracksFromFolder (resourcesFolder);
        DBG ("Loaded bundled music from: " << resourcesFolder.getFullPathName());
    }
    else
    {
        DBG ("Bundled music not found at: " << resourcesFolder.getFullPathName());
        trackNameLabel.setText ("Music not found in bundle", juce::dontSendNotification);
    }
}

void MainComponent::playButtonClicked()
{
    if (state == Stopped || state == Paused)
    {
        transportSource.start();
        state = Playing;
        playButton.setButtonText ("Pause");  // Changes icon to pause bars
    }
    else if (state == Playing)
    {
        transportSource.stop();
        state = Paused;
        playButton.setButtonText ("Play");  // Changes icon to play triangle
    }
}

void MainComponent::stopButtonClicked()
{
    transportSource.stop();
    transportSource.setPosition (0);
    state = Stopped;
    playButton.setButtonText ("Play");
}

void MainComponent::nextButtonClicked()
{
    if (trackFiles.isEmpty())
        return;

    int nextIndex = (currentTrackIndex + 1) % trackFiles.size();
    loadTrack (nextIndex);

    if (state == Playing)
        transportSource.start();
}

void MainComponent::previousButtonClicked()
{
    if (trackFiles.isEmpty())
        return;

    int prevIndex = currentTrackIndex - 1;
    if (prevIndex < 0)
        prevIndex = trackFiles.size() - 1;

    loadTrack (prevIndex);

    if (state == Playing)
        transportSource.start();
}

void MainComponent::updateTimeDisplay()
{
    if (readerSource.get() != nullptr)
    {
        auto currentPos = transportSource.getCurrentPosition();
        auto totalLength = transportSource.getLengthInSeconds();

        auto formatTime = [] (double seconds) -> juce::String
        {
            int mins = static_cast<int> (seconds) / 60;
            int secs = static_cast<int> (seconds) % 60;
            return juce::String::formatted ("%d:%02d", mins, secs);
        };

        timeLabel.setText (formatTime (currentPos) + " / " + formatTime (totalLength),
                          juce::dontSendNotification);
    }
}
