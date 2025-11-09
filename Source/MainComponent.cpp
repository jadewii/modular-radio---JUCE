#include "MainComponent.h"
#include <random>
#include <algorithm>

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
    transportDraggableWrapper.addAndMakeVisible (playButton);

    stopButton.setButtonText ("Stop");
    stopButton.onClick = [this] { stopButtonClicked(); };
    transportDraggableWrapper.addAndMakeVisible (stopButton);

    nextButton.setButtonText ("Next");
    nextButton.onClick = [this] { nextButtonClicked(); };
    nextButton.setLookAndFeel (&customLookAndFeel);
    transportDraggableWrapper.addAndMakeVisible (nextButton);

    previousButton.setButtonText ("Previous");
    previousButton.onClick = [this] { previousButtonClicked(); };
    previousButton.setLookAndFeel (&customLookAndFeel);
    transportDraggableWrapper.addAndMakeVisible (previousButton);

    // Add transport wrapper to main component with visible orange drag handle
    transportDraggableWrapper.setShowDragHandle (true);  // Show orange drag handle
    transportDraggableWrapper.onDragEnd = [this] { saveComponentPositions(); };
    addAndMakeVisible (transportDraggableWrapper);

    // Track name label - centered below transport controls
    trackNameLabel.setText ("No track loaded", juce::dontSendNotification);
    trackNameLabel.setJustificationType (juce::Justification::centred);
    trackNameLabel.setColour (juce::Label::textColourId, juce::Colours::black);  // Black like transport controls
    trackNameLabel.setFont (juce::Font (juce::FontOptions().withHeight(19.2f)));  // 20% larger (was 16, now 19.2)
    trackInfoDraggableWrapper.addAndMakeVisible (trackNameLabel);

    // Add track info wrapper to main component with visible orange drag handle
    trackInfoDraggableWrapper.setShowDragHandle (true);  // Show orange drag handle
    trackInfoDraggableWrapper.onDragEnd = [this] { saveComponentPositions(); };
    addAndMakeVisible (trackInfoDraggableWrapper);

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

        // Bitcrusher
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

    // Filter: Knob=Cutoff, Slider: Resonance, Gain (buttons for type)
    filterGroup = std::make_unique<EffectKnobGroup> ("Filter", "RESO", "GAIN",
        juce::Colours::green,
        [this](float v) { effectsProcessor.setFilterCutoff(v); },
        [this](float v) { effectsProcessor.setFilterResonance(v); },
        [this](float v) { effectsProcessor.setFilterGain(v); });
    filterGroup->setBypassCallback ([this](bool bypassed) {
        effectsProcessor.setFilterBypassed(bypassed);
    });
    // GAIN slider now visible and positioned below RESO slider (not covered by filter type buttons)

    // Filter type buttons (HP, LP, BP) - radio button group - INSIDE the filter group wrapper
    filterHPButton.setButtonText ("HP");
    filterHPButton.setClickingTogglesState (true);
    filterHPButton.setRadioGroupId (1001);
    filterHPButton.setLookAndFeel (&customLookAndFeel);
    filterHPButton.onClick = [this] { effectsProcessor.setFilterType(1); };  // High-pass (FIXED: was 2)
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
    filterBPButton.onClick = [this] { effectsProcessor.setFilterType(2); };  // Band-pass (FIXED: was 1)
    filterGroup->addAndMakeVisible (filterBPButton);  // Add to filter group

    // Add filter group to draggable wrapper
    filterDraggableWrapper.addAndMakeVisible (filterGroup.get());
    filterDraggableWrapper.onDragEnd = [this] { saveComponentPositions(); };
    addAndMakeVisible (filterDraggableWrapper);

    // RESET button - turns all FX off and resets sliders to 0
    resetButton.setButtonText ("RESET");
    resetButton.setLookAndFeel (&customLookAndFeel);
    resetButton.onClick = [this] {
        // Turn off all effect bypass buttons (set them to bypassed = true)
        phaserGroup->getBypassButton().setToggleState (false, juce::sendNotification);
        delayGroup->getBypassButton().setToggleState (false, juce::sendNotification);
        chorusGroup->getBypassButton().setToggleState (false, juce::sendNotification);
        distortionGroup->getBypassButton().setToggleState (false, juce::sendNotification);
        reverbGroup->getBypassButton().setToggleState (false, juce::sendNotification);
        filterGroup->getBypassButton().setToggleState (false, juce::sendNotification);
        timeGroup->getBypassButton().setToggleState (false, juce::sendNotification);

        // Reset all sliders to 0
        phaserGroup->getKnob().setValue (0.0, juce::sendNotification);
        phaserGroup->getSlider1().setValue (0.0, juce::sendNotification);
        phaserGroup->getSlider2().setValue (0.0, juce::sendNotification);

        delayGroup->getKnob().setValue (0.0, juce::sendNotification);
        delayGroup->getSlider1().setValue (0.0, juce::sendNotification);
        delayGroup->getSlider2().setValue (0.0, juce::sendNotification);

        chorusGroup->getKnob().setValue (0.0, juce::sendNotification);
        chorusGroup->getSlider1().setValue (0.0, juce::sendNotification);
        chorusGroup->getSlider2().setValue (0.0, juce::sendNotification);

        distortionGroup->getKnob().setValue (0.0, juce::sendNotification);
        distortionGroup->getSlider1().setValue (0.0, juce::sendNotification);

        reverbGroup->getKnob().setValue (0.0, juce::sendNotification);
        reverbGroup->getSlider1().setValue (0.0, juce::sendNotification);
        reverbGroup->getSlider2().setValue (0.0, juce::sendNotification);

        // Filter gets DEFAULT values (not zero) - useful starting position
        filterGroup->getKnob().setValue (0.5, juce::sendNotification);      // Cutoff at middle (1400Hz)
        filterGroup->getSlider1().setValue (0.3, juce::sendNotification);   // Low resonance
        filterGroup->getSlider2().setValue (0.5, juce::sendNotification);   // Unity gain

        timeGroup->getKnob().setValue (0.0, juce::sendNotification);
        timeGroup->getSlider1().setValue (0.0, juce::sendNotification);
        timeGroup->getSlider2().setValue (0.0, juce::sendNotification);

        DBG ("RESET: All effects turned off and sliders reset to 0");
    };

    // Make RESET button draggable with visible orange drag handle
    resetDraggableWrapper.setShowDragHandle (true);  // Show orange drag handle
    resetDraggableWrapper.addAndMakeVisible (resetButton);
    resetDraggableWrapper.onDragEnd = [this] { saveComponentPositions(); };
    addAndMakeVisible (resetDraggableWrapper);

    // Bitcrusher: Knob=BitDepth, Sliders: Crush, Mix
    timeGroup = std::make_unique<EffectKnobGroup> ("BitCrush", "CRUSH", "MIX",
        juce::Colours::purple,
        [this](float v) { effectsProcessor.setBitcrusherBitDepth(v); },  // 0-1 = 1-16 bits
        [this](float v) { effectsProcessor.setBitcrusherCrush(v); },  // 0-1 = 1x-32x sample rate reduction
        [this](float v) { effectsProcessor.setBitcrusherMix(v); });
    timeGroup->setBypassCallback ([this](bool bypassed) {
        effectsProcessor.setBitcrusherBypassed(bypassed);
    });
    timeDraggableWrapper.addAndMakeVisible (timeGroup.get());
    timeDraggableWrapper.onDragEnd = [this] { saveComponentPositions(); };
    addAndMakeVisible (timeDraggableWrapper);

    // Master Volume Control (single big knob, no bypass button)
    volumeKnob = std::make_unique<VolumeKnob> ([this](float v) {
        masterGain = v;  // 0.0 to 1.0
    });
    volumeDraggableWrapper.addAndMakeVisible (volumeKnob.get());
    volumeDraggableWrapper.onDragEnd = [this] { saveComponentPositions(); };
    addAndMakeVisible (volumeDraggableWrapper);

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
    resetButton.setLookAndFeel (nullptr);
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
    effectsProcessor.setFilterGain (filterGroup->getSlider2().getValue());
    // Filter type initialized from button states (LP is default)
    effectsProcessor.setFilterType (0);  // Low-pass by default

    // Bitcrusher effect initialization
    effectsProcessor.setBitcrusherBitDepth (timeGroup->getKnob().getValue());
    effectsProcessor.setBitcrusherCrush (timeGroup->getSlider1().getValue());
    effectsProcessor.setBitcrusherMix (timeGroup->getSlider2().getValue());

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

    // Apply master volume
    bufferToFill.buffer->applyGain (masterGain);
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

    // Draw LED indicator as realistic 3D circle (account for position within wrapper)
    auto wrapperBounds = ledDraggableWrapper.getBounds();
    auto ledLocalBounds = ledIndicator.getBounds().toFloat();
    auto ledBounds = ledLocalBounds.translated (wrapperBounds.getX(), wrapperBounds.getY());
    auto ledColor = ledIndicator.findColour (juce::Label::backgroundColourId);

    // Outer glow (only when LED is bright green / playing)
    if (state == Playing)
    {
        g.setColour (ledColor.withAlpha (0.3f));
        g.fillEllipse (ledBounds.expanded (2));
    }

    // Black bezel/border
    g.setColour (juce::Colours::black);
    g.fillEllipse (ledBounds);

    // Main LED body with radial gradient for 3D depth
    auto centerX = ledBounds.getCentreX();
    auto centerY = ledBounds.getCentreY();
    auto radius = ledBounds.getWidth() / 2.0f - 3.0f;

    juce::ColourGradient gradient (
        ledColor.brighter (0.5f),  // Brighter in center
        centerX - radius * 0.3f, centerY - radius * 0.3f,  // Offset up-left for light source
        ledColor.darker (0.3f),     // Darker at edges
        centerX + radius * 0.7f, centerY + radius * 0.7f,  // Toward bottom-right
        true  // Radial gradient
    );

    g.setGradientFill (gradient);
    g.fillEllipse (ledBounds.reduced (3));

    // Glossy highlight (small white shine in top-left)
    g.setColour (juce::Colours::white.withAlpha (0.6f));
    auto highlightX = centerX - radius * 0.35f;
    auto highlightY = centerY - radius * 0.35f;
    auto highlightSize = radius * 0.5f;
    g.fillEllipse (highlightX - highlightSize / 2, highlightY - highlightSize / 2,
                   highlightSize, highlightSize);
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

    // Transport controls wrapper - positioned with space for orange drag handle at top
    auto transportY = moduleY + 484;
    auto playButtonHeight = 60;
    auto dragHandleHeight = 20;

    // Position transport wrapper with extra height for drag handle
    transportDraggableWrapper.setBounds (centerX - 120, transportY - dragHandleHeight, 240, playButtonHeight + dragHandleHeight);

    // Position buttons within wrapper (relative to wrapper, accounting for drag handle)
    auto prevNextY = dragHandleHeight + (playButtonHeight - 50) / 2;  // Center 50px buttons with 60px play button

    previousButton.setBounds (10, prevNextY, 50, 50);  // Left of center, vertically aligned
    playButton.setBounds (90, dragHandleHeight, 60, 60);     // Center (bigger), below drag handle
    nextButton.setBounds (180, prevNextY, 50, 50);       // Right of center, vertically aligned
    stopButton.setBounds (0, 0, 0, 0); // Hide stop button

    // RESET button wrapper - positioned to the right of transport controls with space for orange drag handle
    resetDraggableWrapper.setBounds (centerX + 120, transportY - 10, 80, 60);  // Extra height for drag handle
    resetButton.setBounds (0, 20, 80, 40);  // Position button below drag handle (20px for orange bar)

    // Track info wrapper - draggable container for track name with orange drag handle
    auto labelAreaY = moduleY + 590;  // Position below transport controls
    trackInfoDraggableWrapper.setBounds (moduleX + 60, labelAreaY - dragHandleHeight, 360, 30 + dragHandleHeight);  // Extra height for drag handle

    // Position track label within wrapper (relative to wrapper, below drag handle)
    trackNameLabel.setBounds (0, dragHandleHeight, 360, 30);  // Below drag handle

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

    // Master volume knob - positioned below phaser on the right side
    volumeDraggableWrapper.setBounds (1030, 720, 360, 200);
    volumeKnob->setBounds (0, 0, 360, 200);

    // Filter type buttons (positioned relative to filter GROUP, not wrapper)
    // Moved DOWN to Y=160 so they don't overlap with slider2 (GAIN slider at Y=120)
    filterHPButton.setBounds (175, 160, 40, 40);  // HP button
    filterLPButton.setBounds (220, 160, 40, 40);  // LP button
    filterBPButton.setBounds (265, 160, 40, 40);  // BP button
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

    // Reset effects to clear any delay/reverb tail from previous track
    effectsProcessor.reset();
    DBG ("Effects reset on track change - no residue");

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

    // SHUFFLE tracks randomly on every startup for randomized playback order
    if (!trackFiles.isEmpty())
    {
        std::random_device rd;
        std::mt19937 rng (rd());
        std::shuffle (trackFiles.begin(), trackFiles.end(), rng);
        DBG ("Shuffled " << trackFiles.size() << " tracks into random order");
    }

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

        // Reset effects to clear any delay/reverb tail
        effectsProcessor.reset();
        DBG ("Effects reset on pause - no residue");
    }
}

void MainComponent::stopButtonClicked()
{
    transportSource.stop();
    transportSource.setPosition (0);
    state = Stopped;
    playButton.setButtonText ("Play");

    // Reset effects to clear any delay/reverb tail
    effectsProcessor.reset();
    DBG ("Effects reset on stop - no residue");
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
    saveBounds ("TrackInfo", trackInfoDraggableWrapper);
    saveBounds ("Transport", transportDraggableWrapper);
    saveBounds ("Phaser", phaserDraggableWrapper);
    saveBounds ("Delay", delayDraggableWrapper);
    saveBounds ("Chorus", chorusDraggableWrapper);
    saveBounds ("Distortion", distortionDraggableWrapper);
    saveBounds ("Reverb", reverbDraggableWrapper);
    saveBounds ("Filter", filterDraggableWrapper);
    saveBounds ("Time", timeDraggableWrapper);
    saveBounds ("Volume", volumeDraggableWrapper);
    saveBounds ("Reset", resetDraggableWrapper);

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
    loadBounds ("TrackInfo", trackInfoDraggableWrapper);
    loadBounds ("Transport", transportDraggableWrapper);
    loadBounds ("Phaser", phaserDraggableWrapper);
    loadBounds ("Delay", delayDraggableWrapper);
    loadBounds ("Chorus", chorusDraggableWrapper);
    loadBounds ("Distortion", distortionDraggableWrapper);
    loadBounds ("Reverb", reverbDraggableWrapper);
    loadBounds ("Filter", filterDraggableWrapper);
    loadBounds ("Time", timeDraggableWrapper);
    loadBounds ("Volume", volumeDraggableWrapper);
    loadBounds ("Reset", resetDraggableWrapper);

    DBG ("Loaded component positions from: " << propertiesFile.getFullPathName());
}
