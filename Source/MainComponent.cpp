#include "MainComponent.h"

// Include SoundTouch implementation
#include "SoundTouchImpl.cpp"

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

    // Pitch knob (center module) - 180-degree rotation range centered at top
    pitchKnob.setSliderStyle (juce::Slider::Rotary);
    pitchKnob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    pitchKnob.setRange (-12.0, 12.0, 0.1);  // -12 to +12 semitones
    pitchKnob.setValue (0.0);  // Start at center (no pitch shift) - indicator at 12 o'clock (TOP)

    // PITCH KNOB: 360° sweep - center at TOP (12 o'clock), shifted to fix alignment
    // Rotated 90° clockwise so center (0) is at 12 o'clock, not 9 o'clock
    pitchKnob.setRotaryParameters (
        juce::MathConstants<float>::pi * 1.0f,   // 180° = 9 o'clock (left, START)
        juce::MathConstants<float>::pi * 3.0f,   // 540° = 9 o'clock (left after full rotation, END)
        true                                      // Stop at end (don't wrap around)
    );

    // DEBUG: Verify rotation (shifted 90° from previous)
    DBG("Pitch knob: 180° to 540° (360° sweep, rotated 90° clockwise)");
    DBG("  At -12 semitones (MIN): 180° (9 o'clock left)");
    DBG("  At 0 semitones (CENTER): 360° (12 o'clock TOP)");
    DBG("  At +12 semitones (MAX): 540° (9 o'clock right)");

    pitchKnob.setLookAndFeel (&customLookAndFeel);

    // REAL-TIME pitch shifting with ResamplingAudioSource (turntable-style)
    pitchKnob.onValueChange = [this] {
        float semitones = pitchKnob.getValue();
        currentPitchSemitones = semitones;

        // Update resampling ratio in REAL-TIME (smooth, click-free!)
        if (pitchShifter != nullptr)
            pitchShifter->setPitchSemitones (semitones);

        DBG("Pitch: " << semitones << " semitones - REAL-TIME ResamplingAudioSource");
    };

    addAndMakeVisible (pitchKnob);

    // FX Randomize button - generates random parameters for all effects
    fxToggleButton.setButtonText ("");  // Text drawn by look and feel
    fxToggleButton.setClickingTogglesState (false);  // Not a toggle, just a button
    fxToggleButton.setLookAndFeel (&customLookAndFeel);
    fxToggleButton.onClick = [this] {
        // Randomize all effect parameters AND on/off states!
        juce::Random random;

        // Start flash animation
        fxButtonFlashing = true;
        fxButtonFlashCounter = 6;  // Flash 3 times (6 toggles)
        fxToggleButton.getProperties().set ("flashing", true);
        fxToggleButton.repaint();

        // Phaser
        phaserGroup->getKnob().setValue (random.nextDouble(), juce::sendNotification);
        phaserGroup->getSlider1().setValue (random.nextDouble(), juce::sendNotification);
        phaserGroup->getSlider2().setValue (random.nextDouble(), juce::sendNotification);
        phaserGroup->getBypassButton().setToggleState (random.nextBool(), juce::sendNotification);

        // Delay
        delayGroup->getKnob().setValue (random.nextDouble(), juce::sendNotification);
        delayGroup->getSlider1().setValue (random.nextDouble(), juce::sendNotification);
        delayGroup->getSlider2().setValue (random.nextDouble(), juce::sendNotification);
        delayGroup->getBypassButton().setToggleState (random.nextBool(), juce::sendNotification);

        // Chorus
        chorusGroup->getKnob().setValue (random.nextDouble(), juce::sendNotification);
        chorusGroup->getSlider1().setValue (random.nextDouble(), juce::sendNotification);
        chorusGroup->getSlider2().setValue (random.nextDouble(), juce::sendNotification);
        chorusGroup->getBypassButton().setToggleState (random.nextBool(), juce::sendNotification);

        // Distortion
        distortionGroup->getKnob().setValue (random.nextDouble(), juce::sendNotification);
        distortionGroup->getSlider1().setValue (random.nextDouble(), juce::sendNotification);
        distortionGroup->getBypassButton().setToggleState (random.nextBool(), juce::sendNotification);

        // Reverb
        reverbGroup->getKnob().setValue (random.nextDouble(), juce::sendNotification);
        reverbGroup->getSlider1().setValue (random.nextDouble(), juce::sendNotification);
        reverbGroup->getSlider2().setValue (random.nextDouble(), juce::sendNotification);
        reverbGroup->getBypassButton().setToggleState (random.nextBool(), juce::sendNotification);

        // Filter
        filterGroup->getKnob().setValue (random.nextDouble(), juce::sendNotification);
        filterGroup->getSlider1().setValue (random.nextDouble(), juce::sendNotification);
        filterGroup->getBypassButton().setToggleState (random.nextBool(), juce::sendNotification);

        // Time
        timeGroup->getKnob().setValue (random.nextDouble(), juce::sendNotification);
        timeGroup->getSlider1().setValue (random.nextDouble(), juce::sendNotification);
        timeGroup->getSlider2().setValue (random.nextDouble(), juce::sendNotification);
        timeGroup->getBypassButton().setToggleState (random.nextBool(), juce::sendNotification);

        DBG("Randomized all effect parameters and on/off states!");
    };

    // Add FX button to draggable wrapper
    fxDraggableWrapper.addAndMakeVisible (fxToggleButton);
    fxDraggableWrapper.onDragEnd = [this] { saveComponentPositions(); };
    addAndMakeVisible (fxDraggableWrapper);

    // LED indicator (shows playback status)
    ledIndicator.setText ("", juce::dontSendNotification);
    ledIndicator.setColour (juce::Label::backgroundColourId, juce::Colour (0xff1a4d1a));  // Dark green
    ledIndicator.setColour (juce::Label::textColourId, juce::Colours::transparentBlack);
    ledIndicator.setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    // Note: LED is drawn manually in paint() as a circle

    // Add LED to draggable wrapper
    ledDraggableWrapper.addAndMakeVisible (ledIndicator);
    ledDraggableWrapper.onDragEnd = [this] { saveComponentPositions(); };
    addAndMakeVisible (ledDraggableWrapper);

    // Create effect knob groups (1 knob + 2 sliders each)
    // Phaser: Knob=Rate, Sliders: Depth, Mix
    phaserGroup = std::make_unique<EffectKnobGroup> ("Phaser", "DEPTH", "MIX",
        juce::Colours::cyan,
        [this](float v) { effectsProcessor.setPhaserRate(v); },
        [this](float v) { effectsProcessor.setPhaserDepth(v); },
        [this](float v) { effectsProcessor.setPhaserMix(v); });
    phaserGroup->setBypassCallback ([this](bool bypassed) {
        effectsProcessor.setPhaserBypassed(bypassed);
    });
    phaserGroup->getBypassButton().setToggleState (false, juce::dontSendNotification);  // Start bypassed (grey)
    phaserDraggableWrapper.addAndMakeVisible (phaserGroup.get());
    phaserDraggableWrapper.onDragEnd = [this] { saveComponentPositions(); };
    addAndMakeVisible (phaserDraggableWrapper);

    // Delay: Knob=Mix, Sliders: Time, Feedback
    delayGroup = std::make_unique<EffectKnobGroup> ("Delay", "TIME", "FDBK",
        juce::Colours::red,
        [this](float v) { effectsProcessor.setDelayMix(v); },
        [this](float v) { effectsProcessor.setDelayTime(v * 3.0f); },
        [this](float v) { effectsProcessor.setDelayFeedback(v * 0.95f); });
    delayGroup->setBypassCallback ([this](bool bypassed) {
        effectsProcessor.setDelayBypassed(bypassed);
    });
    delayDraggableWrapper.addAndMakeVisible (delayGroup.get());
    delayDraggableWrapper.onDragEnd = [this] { saveComponentPositions(); };
    addAndMakeVisible (delayDraggableWrapper);

    // Chorus: Knob=Rate, Sliders: Depth, Mix
    chorusGroup = std::make_unique<EffectKnobGroup> ("Chorus", "DEPTH", "MIX",
        juce::Colours::blue,
        [this](float v) { effectsProcessor.setChorusRate(v); },
        [this](float v) { effectsProcessor.setChorusDepth(v); },
        [this](float v) { effectsProcessor.setChorusMix(v); });
    chorusGroup->setBypassCallback ([this](bool bypassed) {
        effectsProcessor.setChorusBypassed(bypassed);
    });
    chorusDraggableWrapper.addAndMakeVisible (chorusGroup.get());
    chorusDraggableWrapper.onDragEnd = [this] { saveComponentPositions(); };
    addAndMakeVisible (chorusDraggableWrapper);

    // Distortion: Knob=Drive, Sliders: Mix, (empty)
    distortionGroup = std::make_unique<EffectKnobGroup> ("Distortion", "MIX", "",
        juce::Colours::orange,
        [this](float v) { effectsProcessor.setDistortionDrive(v); },
        [this](float v) { effectsProcessor.setDistortionMix(v); },
        [this](float v) { });
    distortionGroup->setBypassCallback ([this](bool bypassed) {
        effectsProcessor.setDistortionBypassed(bypassed);
    });
    distortionDraggableWrapper.addAndMakeVisible (distortionGroup.get());
    distortionDraggableWrapper.onDragEnd = [this] { saveComponentPositions(); };
    addAndMakeVisible (distortionDraggableWrapper);

    // Reverb: Knob=Mix, Sliders: Size, Damping
    reverbGroup = std::make_unique<EffectKnobGroup> ("Reverb", "SIZE", "DAMP",
        juce::Colours::yellow,
        [this](float v) { effectsProcessor.setReverbMix(v); },
        [this](float v) { effectsProcessor.setReverbSize(v); },
        [this](float v) { effectsProcessor.setReverbDamping(v); });
    reverbGroup->setBypassCallback ([this](bool bypassed) {
        effectsProcessor.setReverbBypassed(bypassed);
    });
    reverbDraggableWrapper.addAndMakeVisible (reverbGroup.get());
    reverbDraggableWrapper.onDragEnd = [this] { saveComponentPositions(); };
    addAndMakeVisible (reverbDraggableWrapper);

    // Filter: Knob=Cutoff, Slider: Resonance (buttons for type)
    filterGroup = std::make_unique<EffectKnobGroup> ("Filter", "RESO", "GAIN",
        juce::Colours::green,
        [this](float v) { effectsProcessor.setFilterCutoff(v); },
        [this](float v) { effectsProcessor.setFilterResonance(v); },
        [this](float v) { /* gain/drive */ });
    filterGroup->setBypassCallback ([this](bool bypassed) {
        effectsProcessor.setFilterBypassed(bypassed);
    });
    filterGroup->getSlider2().setVisible (false);  // Hide second slider for filter

    // Filter type buttons (HP, LP, BP) - radio button group - INSIDE the filter group wrapper
    filterHPButton.setButtonText ("HP");
    filterHPButton.setClickingTogglesState (true);
    filterHPButton.setRadioGroupId (1001);
    filterHPButton.setLookAndFeel (&customLookAndFeel);
    filterHPButton.onClick = [this] { effectsProcessor.setFilterType(2); };  // High-pass
    filterGroup->addAndMakeVisible (filterHPButton);  // Add to filter group

    filterLPButton.setButtonText ("LP");
    filterLPButton.setClickingTogglesState (true);
    filterLPButton.setRadioGroupId (1001);
    filterLPButton.setLookAndFeel (&customLookAndFeel);
    filterLPButton.setToggleState (true, juce::dontSendNotification);  // Default to LP
    filterLPButton.onClick = [this] { effectsProcessor.setFilterType(0); };  // Low-pass
    filterGroup->addAndMakeVisible (filterLPButton);  // Add to filter group

    filterBPButton.setButtonText ("BP");
    filterBPButton.setClickingTogglesState (true);
    filterBPButton.setRadioGroupId (1001);
    filterBPButton.setLookAndFeel (&customLookAndFeel);
    filterBPButton.onClick = [this] { effectsProcessor.setFilterType(1); };  // Band-pass
    filterGroup->addAndMakeVisible (filterBPButton);  // Add to filter group

    // Add filter group to draggable wrapper
    filterDraggableWrapper.addAndMakeVisible (filterGroup.get());
    filterDraggableWrapper.onDragEnd = [this] { saveComponentPositions(); };
    addAndMakeVisible (filterDraggableWrapper);

    // Time: Knob=Stretch, Sliders: Pitch, Mix
    timeGroup = std::make_unique<EffectKnobGroup> ("Time", "PITCH", "MIX",
        juce::Colours::purple,
        [this](float v) { effectsProcessor.setTimeStretch(0.5f + v * 1.5f); },  // 0.5x to 2x
        [this](float v) { effectsProcessor.setTimePitch((v - 0.5f) * 24.0f); },  // -12 to +12 semitones
        [this](float v) { effectsProcessor.setTimeMix(v); });
    timeGroup->setBypassCallback ([this](bool bypassed) {
        effectsProcessor.setTimeBypassed(bypassed);
    });
    timeDraggableWrapper.addAndMakeVisible (timeGroup.get());
    timeDraggableWrapper.onDragEnd = [this] { saveComponentPositions(); };
    addAndMakeVisible (timeDraggableWrapper);

    // Load bundled music
    loadBundledMusic();

    // Start timer
    startTimer (100);

    // Set window size to fit 16-inch MacBook screen comfortably
    setSize (1400, 900);
    setAudioChannels (0, 2);

    // Load saved component positions (do this BEFORE connecting audio to avoid callback loops)
    loadComponentPositions();

    DBG ("MainComponent initialization complete!");
}

MainComponent::~MainComponent()
{
    pitchKnob.setLookAndFeel (nullptr);
    playButton.setLookAndFeel (nullptr);
    nextButton.setLookAndFeel (nullptr);
    previousButton.setLookAndFeel (nullptr);
    fxToggleButton.setLookAndFeel (nullptr);
    filterHPButton.setLookAndFeel (nullptr);
    filterLPButton.setLookAndFeel (nullptr);
    filterBPButton.setLookAndFeel (nullptr);
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
    // Filter type initialized from button states (LP is default)
    effectsProcessor.setFilterType (0);  // Low-pass by default

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

    // Draw LED indicator as circle (account for position within wrapper)
    auto wrapperBounds = ledDraggableWrapper.getBounds();
    auto ledLocalBounds = ledIndicator.getBounds().toFloat();
    auto ledBounds = ledLocalBounds.translated (wrapperBounds.getX(), wrapperBounds.getY());
    auto ledColor = ledIndicator.findColour (juce::Label::backgroundColourId);

    // Black border
    g.setColour (juce::Colours::black);
    g.fillEllipse (ledBounds);

    // Inner LED light
    g.setColour (ledColor);
    g.fillEllipse (ledBounds.reduced (3));
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

    // FX toggle - draggable wrapper (no visible drag handle)
    fxDraggableWrapper.setBounds (moduleX + 75, moduleY + 410, 50, 50);  // Just fits the button
    fxToggleButton.setBounds (5, 5, 40, 40);  // Button centered in wrapper

    // LED indicator - draggable wrapper (no visible drag handle)
    ledDraggableWrapper.setBounds (moduleX + 420, moduleY + 150, 32, 32);  // Just fits the LED
    ledIndicator.setBounds (5, 5, 22, 22);  // LED centered in wrapper

    // Transport controls - perfectly aligned with play button
    auto transportY = moduleY + 484;
    auto playButtonY = transportY;
    auto playButtonHeight = 60;

    // Align prev/next buttons vertically centered with play button
    auto prevNextY = playButtonY + (playButtonHeight - 50) / 2;  // Center 50px buttons with 60px play button

    previousButton.setBounds (centerX - 110, prevNextY, 50, 50);  // Left of center, vertically aligned
    playButton.setBounds (centerX - 30, playButtonY, 60, 60);     // Center (bigger)
    nextButton.setBounds (centerX + 60, prevNextY, 50, 50);       // Right of center, vertically aligned
    stopButton.setBounds (0, 0, 0, 0); // Hide stop button

    // Track name - centered in bottom area of module
    trackNameLabel.setBounds (moduleX + 60, moduleY + 600, 360, 30);

    // Position effect knob group WRAPPERS around edges - sized for 1400x900 window
    // NO drag handle visuals, so wrappers are same size as content
    // Left side
    filterDraggableWrapper.setBounds (10, 60, 360, 200);
    filterGroup->setBounds (0, 0, 360, 200);  // Fill wrapper completely
    delayDraggableWrapper.setBounds (10, 280, 360, 200);
    delayGroup->setBounds (0, 0, 360, 200);
    reverbDraggableWrapper.setBounds (10, 500, 360, 200);
    reverbGroup->setBounds (0, 0, 360, 200);
    timeDraggableWrapper.setBounds (10, 720, 360, 200);
    timeGroup->setBounds (0, 0, 360, 200);

    // Right side
    chorusDraggableWrapper.setBounds (1030, 60, 360, 200);
    chorusGroup->setBounds (0, 0, 360, 200);
    distortionDraggableWrapper.setBounds (1030, 280, 360, 200);
    distortionGroup->setBounds (0, 0, 360, 200);
    phaserDraggableWrapper.setBounds (1030, 500, 360, 200);
    phaserGroup->setBounds (0, 0, 360, 200);

    // Filter type buttons (positioned relative to filter GROUP, not wrapper)
    filterHPButton.setBounds (175, 115, 40, 40);  // HP button
    filterLPButton.setBounds (220, 115, 40, 40);  // LP button
    filterBPButton.setBounds (265, 115, 40, 40);  // BP button
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

    // Update LED indicator based on playback state
    if (state == Playing)
    {
        ledIndicator.setColour (juce::Label::backgroundColourId, juce::Colour (0xff00ff00));  // Bright green
    }
    else
    {
        ledIndicator.setColour (juce::Label::backgroundColourId, juce::Colour (0xff1a4d1a));  // Dark green
    }
    ledIndicator.repaint();

    // Handle FX button flash animation
    if (fxButtonFlashing && fxButtonFlashCounter > 0)
    {
        fxButtonFlashCounter--;
        bool flashState = (fxButtonFlashCounter % 2) == 0;  // Toggle between true/false
        fxToggleButton.getProperties().set ("flashing", flashState);
        fxToggleButton.repaint();

        if (fxButtonFlashCounter == 0)
        {
            fxButtonFlashing = false;
            fxToggleButton.getProperties().set ("flashing", false);
            fxToggleButton.repaint();
        }
    }
}

void MainComponent::loadTrack (int index)
{
    if (index < 0 || index >= trackFiles.size())
        return;

    transportSource.stop();
    transportSource.setSource (nullptr);
    pitchShifter.reset();
    readerSource.reset();

    // RESET pitch knob to center (0 semitones) when changing tracks
    pitchKnob.setValue (0.0, juce::dontSendNotification);
    currentPitchSemitones = 0.0;

    auto file = trackFiles[index];
    auto* reader = formatManager.createReaderFor (file);

    if (reader != nullptr)
    {
        readerSource.reset (new juce::AudioFormatReaderSource (reader, true));

        // Wrap reader source in ResamplingAudioSource for smooth, click-free pitch shifting
        pitchShifter.reset (new SmoothResamplingSource (readerSource.get(), false));
        pitchShifter->prepareToPlay (512, reader->sampleRate);
        pitchShifter->setPitchSemitones (currentPitchSemitones);  // Start at 0 (normal pitch)

        // Connect pitch shifter to transport (now implements PositionableAudioSource)
        transportSource.setSource (pitchShifter.get(), 0, nullptr, reader->sampleRate);

        currentTrackIndex = index;
        currentTrackName = file.getFileNameWithoutExtension();
        trackNameLabel.setText (currentTrackName, juce::dontSendNotification);

        DBG ("Loaded: " << currentTrackName << " with ResamplingAudioSource (0 semitones)");
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

void MainComponent::saveComponentPositions()
{
    // Get app properties folder
    auto appDataDir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                          .getChildFile ("ModularRadio");

    if (!appDataDir.exists())
        appDataDir.createDirectory();

    auto propertiesFile = appDataDir.getChildFile ("positions.xml");

    // Create XML to store positions
    juce::XmlElement xml ("ComponentPositions");

    // Save all draggable component positions
    auto saveBounds = [&xml] (const juce::String& name, const juce::Component& component)
    {
        auto bounds = component.getBounds();
        auto* elem = xml.createNewChildElement (name);
        elem->setAttribute ("x", bounds.getX());
        elem->setAttribute ("y", bounds.getY());
        elem->setAttribute ("width", bounds.getWidth());
        elem->setAttribute ("height", bounds.getHeight());
    };

    saveBounds ("FxButton", fxDraggableWrapper);
    saveBounds ("Led", ledDraggableWrapper);
    saveBounds ("Phaser", phaserDraggableWrapper);
    saveBounds ("Delay", delayDraggableWrapper);
    saveBounds ("Chorus", chorusDraggableWrapper);
    saveBounds ("Distortion", distortionDraggableWrapper);
    saveBounds ("Reverb", reverbDraggableWrapper);
    saveBounds ("Filter", filterDraggableWrapper);
    saveBounds ("Time", timeDraggableWrapper);

    // Write to file
    xml.writeTo (propertiesFile);

    DBG ("Saved component positions to: " << propertiesFile.getFullPathName());
}

void MainComponent::loadComponentPositions()
{
    auto appDataDir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                          .getChildFile ("ModularRadio");
    auto propertiesFile = appDataDir.getChildFile ("positions.xml");

    if (!propertiesFile.existsAsFile())
    {
        DBG ("No saved positions found, using defaults");
        return;
    }

    // Load XML
    auto xml = juce::XmlDocument::parse (propertiesFile);

    if (xml == nullptr)
    {
        DBG ("Failed to parse positions file");
        return;
    }

    // Restore positions
    auto loadBounds = [&xml] (const juce::String& name, juce::Component& component)
    {
        auto* elem = xml->getChildByName (name);
        if (elem != nullptr)
        {
            int x = elem->getIntAttribute ("x", component.getX());
            int y = elem->getIntAttribute ("y", component.getY());
            int width = elem->getIntAttribute ("width", component.getWidth());
            int height = elem->getIntAttribute ("height", component.getHeight());
            component.setBounds (x, y, width, height);
        }
    };

    loadBounds ("FxButton", fxDraggableWrapper);
    loadBounds ("Led", ledDraggableWrapper);
    loadBounds ("Phaser", phaserDraggableWrapper);
    loadBounds ("Delay", delayDraggableWrapper);
    loadBounds ("Chorus", chorusDraggableWrapper);
    loadBounds ("Distortion", distortionDraggableWrapper);
    loadBounds ("Reverb", reverbDraggableWrapper);
    loadBounds ("Filter", filterDraggableWrapper);
    loadBounds ("Time", timeDraggableWrapper);

    DBG ("Loaded component positions from: " << propertiesFile.getFullPathName());
}
