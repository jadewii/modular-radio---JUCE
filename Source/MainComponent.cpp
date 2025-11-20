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

    // Transport buttons (matching SwiftUI design) - FIXED POSITION
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

    // Track name label - FIXED POSITION
    trackNameLabel.setText ("No track loaded", juce::dontSendNotification);
    trackNameLabel.setJustificationType (juce::Justification::centred);
    trackNameLabel.setColour (juce::Label::textColourId, juce::Colours::black);  // Black like transport controls
    trackNameLabel.setFont (juce::Font (juce::FontOptions().withHeight(19.2f)));  // 20% larger (was 16, now 19.2)
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

        // NO FLASHING - just do the randomization

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
        distortionGroup->getSlider2().setValue (random.nextDouble(), juce::sendNotification);
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

    // FX button - FIXED POSITION
    addAndMakeVisible (fxToggleButton);

    // LED indicator - FIXED POSITION
    ledIndicator.setText ("", juce::dontSendNotification);
    ledIndicator.setColour (juce::Label::backgroundColourId, juce::Colour (0xff1a4d1a));  // Dark green
    ledIndicator.setColour (juce::Label::textColourId, juce::Colours::transparentBlack);
    ledIndicator.setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    // Note: LED is drawn manually in paint() as a circle
    addAndMakeVisible (ledIndicator);

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
    addAndMakeVisible (phaserGroup.get());

    // Delay: Knob=Mix, Sliders: Time, Feedback
    delayGroup = std::make_unique<EffectKnobGroup> ("Delay", "TIME", "FDBK",
        juce::Colours::red,
        [this](float v) { effectsProcessor.setDelayMix(v); },
        [this](float v) { effectsProcessor.setDelayTime(v * 3.0f); },
        [this](float v) { effectsProcessor.setDelayFeedback(v * 0.95f); });
    delayGroup->setBypassCallback ([this](bool bypassed) {
        effectsProcessor.setDelayBypassed(bypassed);
    });
    addAndMakeVisible (delayGroup.get());

    // Chorus: Knob=Rate, Sliders: Depth, Mix
    chorusGroup = std::make_unique<EffectKnobGroup> ("Chorus", "DEPTH", "MIX",
        juce::Colours::blue,
        [this](float v) { effectsProcessor.setChorusRate(v); },
        [this](float v) { effectsProcessor.setChorusDepth(v); },
        [this](float v) { effectsProcessor.setChorusMix(v); });
    chorusGroup->setBypassCallback ([this](bool bypassed) {
        effectsProcessor.setChorusBypassed(bypassed);
    });
    addAndMakeVisible (chorusGroup.get());

    // Distortion: Knob=Drive, Sliders: Mix, Drive
    distortionGroup = std::make_unique<EffectKnobGroup> ("Distortion", "MIX", "DRIVE",
        juce::Colours::red,
        [this](float v) { effectsProcessor.setDistortionDrive(v); },
        [this](float v) { effectsProcessor.setDistortionMix(v); },
        [this](float v) { effectsProcessor.setDistortionDrive(v); });
    distortionGroup->setBypassCallback ([this](bool bypassed) {
        effectsProcessor.setDistortionBypassed(bypassed);
    });
    addAndMakeVisible (distortionGroup.get());

    // Reverb: Knob=Mix, Sliders: Size, Damping
    reverbGroup = std::make_unique<EffectKnobGroup> ("Reverb", "SIZE", "DAMP",
        juce::Colours::yellow,
        [this](float v) { effectsProcessor.setReverbMix(v); },
        [this](float v) { effectsProcessor.setReverbSize(v); },
        [this](float v) { effectsProcessor.setReverbDamping(v); });
    reverbGroup->setBypassCallback ([this](bool bypassed) {
        effectsProcessor.setReverbBypassed(bypassed);
    });
    addAndMakeVisible (reverbGroup.get());

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

    // Filter type buttons (HP, LP, BP) - radio button group
    filterHPButton.setButtonText ("HP");
    filterHPButton.setClickingTogglesState (true);
    filterHPButton.setRadioGroupId (1001);
    filterHPButton.setLookAndFeel (&customLookAndFeel);
    filterHPButton.onClick = [this] { effectsProcessor.setFilterType(1); };  // High-pass (FIXED: was 2)

    filterLPButton.setButtonText ("LP");
    filterLPButton.setClickingTogglesState (true);
    filterLPButton.setRadioGroupId (1001);
    filterLPButton.setLookAndFeel (&customLookAndFeel);
    filterLPButton.setToggleState (true, juce::dontSendNotification);  // Default to LP
    filterLPButton.onClick = [this] { effectsProcessor.setFilterType(0); };  // Low-pass

    filterBPButton.setButtonText ("BP");
    filterBPButton.setClickingTogglesState (true);
    filterBPButton.setRadioGroupId (1001);
    filterBPButton.setLookAndFeel (&customLookAndFeel);
    filterBPButton.onClick = [this] { effectsProcessor.setFilterType(2); };  // Band-pass (FIXED: was 1)

    // Create draggable filter buttons group
    draggableFilterButtons = std::make_unique<DraggableFilterButtons> (filterHPButton, filterLPButton, filterBPButton);
    filterGroup->addAndMakeVisible (draggableFilterButtons.get());  // Add to filter group

    // Add filter group - FIXED POSITION
    addAndMakeVisible (filterGroup.get());

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
        distortionGroup->getSlider2().setValue (0.0, juce::sendNotification);

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

        // RESET pitch knob to center (0 semitones)
        pitchKnob.setValue (0.0, juce::sendNotification);

        DBG ("RESET: All effects turned off, sliders reset to 0, and pitch knob centered");
    };

    // RESET button - FIXED POSITION
    addAndMakeVisible (resetButton);

    // Bitcrusher: Knob=BitDepth, Sliders: Crush, Mix
    timeGroup = std::make_unique<EffectKnobGroup> ("BitCrush", "CRUSH", "MIX",
        juce::Colours::purple,
        [this](float v) { effectsProcessor.setBitcrusherBitDepth(v); },  // 0-1 = 1-16 bits
        [this](float v) { effectsProcessor.setBitcrusherCrush(v); },  // 0-1 = 1x-32x sample rate reduction
        [this](float v) { effectsProcessor.setBitcrusherMix(v); });
    timeGroup->setBypassCallback ([this](bool bypassed) {
        effectsProcessor.setBitcrusherBypassed(bypassed);
    });
    addAndMakeVisible (timeGroup.get());

    // Master Volume Control (single big knob, no bypass button)
    volumeKnob = std::make_unique<VolumeKnob> ([this](float v) {
        masterGain = v;  // 0.0 to 1.0
    });
    addAndMakeVisible (volumeKnob.get());

    // Load bundled music
    loadBundledMusic();

    // Start timer - faster for better flashing effect
    startTimer (150);  // Faster timer (150ms for visible flashing)

    // Initialize device-specific window size
    auto deviceScale = DeviceDetection::getScaleFactor();

    // Set appropriate window size based on device
    if (DeviceDetection::isPhone())
    {
        setSize(400, 700); // Portrait phone size
    }
    else
    {
        setSize(1400, 900); // Reference tablet/desktop size
    }

    setAudioChannels (0, 2);

    // Make window resizable for testing different device sizes
    if (auto* window = findParentComponentOfClass<juce::DocumentWindow>())
    {
        window->setResizable (true, true);
        if (DeviceDetection::isPhone())
        {
            window->setResizeLimits (300, 500, 600, 1000); // Phone size limits
        }
        else
        {
            window->setResizeLimits (700, 450, 2800, 1800); // Tablet/desktop limits
        }
    }

    DBG("Initialized for device: " << DeviceDetection::getDeviceString()
        << " with scale factor: " << deviceScale);

    DBG ("MainComponent initialization complete - all components in FIXED positions!");

    // DEMONSTRATE: Show how your layout would scale on different iPad sizes
    DBG ("=== PROPORTIONAL SCALING DEMONSTRATION ===");

    // iPad Pro 12.9" (2732×2048 landscape = 2732×2048)
    float scale_ipad_pro_129 = juce::jmin(2732.0f / 1400.0f, 2048.0f / 900.0f);
    DBG ("iPad Pro 12.9\": " << 2732 << "x" << 2048 << " | Scale: " << scale_ipad_pro_129 << " (" << (scale_ipad_pro_129 * 100 - 100) << "% larger)");

    // iPad Pro 11" (2388×1668)
    float scale_ipad_pro_11 = juce::jmin(2388.0f / 1400.0f, 1668.0f / 900.0f);
    DBG ("iPad Pro 11\": " << 2388 << "x" << 1668 << " | Scale: " << scale_ipad_pro_11 << " (" << (scale_ipad_pro_11 * 100 - 100) << "% larger)");

    // iPad Air 11" (2360×1640)
    float scale_ipad_air_11 = juce::jmin(2360.0f / 1400.0f, 1640.0f / 900.0f);
    DBG ("iPad Air 11\": " << 2360 << "x" << 1640 << " | Scale: " << scale_ipad_air_11 << " (" << (scale_ipad_air_11 * 100 - 100) << "% larger)");

    // iPad Mini (2266×1488)
    float scale_ipad_mini = juce::jmin(2266.0f / 1400.0f, 1488.0f / 900.0f);
    DBG ("iPad Mini: " << 2266 << "x" << 1488 << " | Scale: " << scale_ipad_mini << " (" << (scale_ipad_mini * 100 - 100) << "% larger)");

    // Standard iPad (2360×1640)
    float scale_ipad = juce::jmin(2360.0f / 1400.0f, 1640.0f / 900.0f);
    DBG ("iPad (10th gen): " << 2360 << "x" << 1640 << " | Scale: " << scale_ipad << " (" << (scale_ipad * 100 - 100) << "% larger)");

    DBG ("Your perfect layout will scale proportionally to fit each device while maintaining exact proportions!");
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
    draggableFilterButtons.reset();
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

    // Draw LED indicator as realistic 3D circle (FIXED POSITION)
    auto ledBounds = ledIndicator.getBounds().toFloat();
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

    // Initialize adaptive layout system
    AdaptiveLayout::initializeForDevice(bounds);

    // Get device type and handle device-specific layouts
    auto deviceScale = DeviceDetection::getScaleFactor();

    DBG("Device detected: " << DeviceDetection::getDeviceString()
        << " with scale factor: " << deviceScale);

    // Phone vs Tablet layout
    if (DeviceDetection::isPhone())
    {
        // iPhone layout - simplified, single-page design
        layoutForPhone(bounds, deviceScale);
    }
    else
    {
        // iPad/Desktop layout - full modular interface
        layoutForTablet(bounds, deviceScale);
    }

    // Debug scaling info
    DBG("Screen: " << bounds.getWidth() << "x" << bounds.getHeight()
        << " | Device: " << DeviceDetection::getDeviceString()
        << " | Scale: " << deviceScale);
}

void MainComponent::layoutForTablet(juce::Rectangle<int> bounds, float deviceScale)
{
    // Use adaptive layout system for tablet/desktop
    const float refWidth = 1400.0f;
    const float refHeight = 900.0f;

    // Calculate scale factors
    float scaleX = bounds.getWidth() / refWidth;
    float scaleY = bounds.getHeight() / refHeight;
    float scale = juce::jmin(scaleX, scaleY);

    // Apply device-specific scale adjustment
    scale *= deviceScale;

    // Calculate scaled dimensions
    float scaledWidth = refWidth * scale;
    float scaledHeight = refHeight * scale;

    // Center the scaled content
    float offsetX = (bounds.getWidth() - scaledWidth) / 2;
    float offsetY = (bounds.getHeight() - scaledHeight) / 2;

    // Helper function to scale coordinates
    auto scaleBounds = [scale, offsetX, offsetY](float x, float y, float w, float h) -> juce::Rectangle<int>
    {
        return juce::Rectangle<int>(
            static_cast<int>(x * scale + offsetX),
            static_cast<int>(y * scale + offsetY),
            static_cast<int>(w * scale),
            static_cast<int>(h * scale)
        );
    };

    // Note: These could be used for more sophisticated layout later
    // auto moduleBounds = AdaptiveLayout::getModuleBounds();
    // auto pitchBounds = AdaptiveLayout::getPitchKnobBounds();
    // auto transportBounds = AdaptiveLayout::getTransportBounds();

    // Apply adaptive positioning with device scaling
    auto refCenterX = refWidth / 2;
    auto refCenterY = refHeight / 2;
    auto refModuleX = refCenterX - 240;
    auto refModuleY = refCenterY - 320;

    // Main controls (center module)
    pitchKnob.setBounds(scaleBounds(refModuleX + 123, refModuleY + 153, 234, 234));
    fxToggleButton.setBounds(scaleBounds(refModuleX + 80, refModuleY + 415, 40, 40));
    ledIndicator.setBounds(scaleBounds(refModuleX + 425, refModuleY + 155, 22, 22));

    // Transport controls
    auto refTransportY = refModuleY + 484;
    auto refPlayButtonHeight = 60;
    auto refPrevNextY = refTransportY + (refPlayButtonHeight - 50) / 2;

    previousButton.setBounds(scaleBounds(refCenterX - 110, refPrevNextY, 50, 50));
    playButton.setBounds(scaleBounds(refCenterX - 30, refTransportY, 60, 60));
    nextButton.setBounds(scaleBounds(refCenterX + 60, refPrevNextY, 50, 50));
    stopButton.setBounds(0, 0, 0, 0); // Keep hidden

    resetButton.setBounds(scaleBounds(refCenterX + 120, refTransportY + 10, 80, 40));

    // Track info
    auto refLabelAreaY = refModuleY + 590;
    trackNameLabel.setBounds(scaleBounds(refModuleX + 60, refLabelAreaY, 360, 30));

    // Effect knob groups - use adaptive positioning
    float effectScale = scale * 1.2f; // Make effects slightly larger on larger devices

    // Left side effects
    filterGroup->setBounds(scaleBounds(10, 60, 360, 200));
    delayGroup->setBounds(scaleBounds(10, 280, 360, 200));
    reverbGroup->setBounds(scaleBounds(10, 500, 360, 200));
    timeGroup->setBounds(scaleBounds(10, 720, 360, 200));

    // Right side effects
    chorusGroup->setBounds(scaleBounds(1030, 60, 360, 200));
    distortionGroup->setBounds(scaleBounds(1030, 280, 360, 200));
    phaserGroup->setBounds(scaleBounds(1030, 500, 360, 200));
    volumeKnob->setBounds(scaleBounds(1030, 720, 360, 200));
}

void MainComponent::layoutForPhone(juce::Rectangle<int> bounds, float deviceScale)
{
    // iPhone layout - simplified design focused on essential controls
    auto centerX = bounds.getCentreX();
    auto centerY = bounds.getCentreY();

    float phoneScale = 0.6f * deviceScale; // Smaller scale for phone

    // Simplified module in center (smaller)
    auto moduleWidth = static_cast<int>(300 * phoneScale);
    auto moduleHeight = static_cast<int>(400 * phoneScale);
    auto moduleX = centerX - moduleWidth / 2;
    auto moduleY = centerY - moduleHeight / 2 - 50; // Offset up slightly

    // Main pitch knob (prominent)
    auto knobSize = static_cast<int>(120 * phoneScale);
    pitchKnob.setBounds(centerX - knobSize/2, moduleY + 20, knobSize, knobSize);

    // Transport controls below (larger for touch)
    auto buttonSize = static_cast<int>(50 * phoneScale);
    auto transportY = moduleY + moduleHeight - 80;

    previousButton.setBounds(centerX - buttonSize*3/2 - 10, transportY, buttonSize, buttonSize);
    playButton.setBounds(centerX - buttonSize/2, transportY - 5, buttonSize + 10, buttonSize + 10);
    nextButton.setBounds(centerX + buttonSize/2 + 10, transportY, buttonSize, buttonSize);

    // FX and LED indicators (smaller)
    fxToggleButton.setBounds(moduleX + 10, moduleY + 10, 30, 30);
    ledIndicator.setBounds(moduleX + moduleWidth - 40, moduleY + 10, 15, 15);

    // Track name below transport
    trackNameLabel.setBounds(moduleX - 50, transportY + 70, moduleWidth + 100, 25);

    // Reset button (smaller, top right)
    resetButton.setBounds(bounds.getWidth() - 70, 20, 50, 30);

    // Hide effect groups on phone or make them very small/overlaid
    // For now, position them off-screen to hide them
    filterGroup->setBounds(-1000, -1000, 1, 1);
    delayGroup->setBounds(-1000, -1000, 1, 1);
    reverbGroup->setBounds(-1000, -1000, 1, 1);
    timeGroup->setBounds(-1000, -1000, 1, 1);
    chorusGroup->setBounds(-1000, -1000, 1, 1);
    distortionGroup->setBounds(-1000, -1000, 1, 1);
    phaserGroup->setBounds(-1000, -1000, 1, 1);
    volumeKnob->setBounds(-1000, -1000, 1, 1);

    // Hide stop button
    stopButton.setBounds(0, 0, 0, 0);

    DBG("iPhone layout applied with scale: " << phoneScale);
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
    // DISABLED: updateTimeDisplay() - timeLabel is hidden and may cause issues
    // updateTimeDisplay();

    // Update LED indicator based on playback state - SAFER VERSION (no repaint calls)
    if (state == Playing)
    {
        ledIndicator.setColour (juce::Label::backgroundColourId, juce::Colour (0xff00ff00));  // Bright green
    }
    else
    {
        ledIndicator.setColour (juce::Label::backgroundColourId, juce::Colour (0xff1a4d1a));  // Dark green
    }
    // REMOVED: ledIndicator.repaint(); - manual repaint can cause hangs

    // FX button flashing removed - now uses press state only
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

    DBG ("Loading tracks from folder: " + folder.getFullPathName());
    DBG ("Folder exists: " + juce::String(folder.exists() ? "true" : "false"));
    DBG ("Folder is directory: " + juce::String(folder.isDirectory() ? "true" : "false"));

    juce::Array<juce::File> files;
    folder.findChildFiles (files, juce::File::findFiles, true, "*.mp3;*.wav;*.aiff;*.aif;*.m4a;*.flac");

    DBG ("Found " + juce::String(files.size()) + " files in folder scan");

    // Log first few files for debugging
    for (int i = 0; i < juce::jmin(5, files.size()); ++i)
    {
        DBG ("File " + juce::String(i) + ": " + files[i].getFullPathName());
    }

    trackFiles.addArray (files);

    // SHUFFLE tracks randomly on every startup for randomized playbook order
    if (!trackFiles.isEmpty())
    {
        std::random_device rd;
        std::mt19937 rng (rd());
        std::shuffle (trackFiles.begin(), trackFiles.end(), rng);
        DBG ("Shuffled " + juce::String(trackFiles.size()) + " tracks into random order");
    }

    DBG ("Final loaded track count: " + juce::String(trackFiles.size()));

    if (!trackFiles.isEmpty())
        loadTrack (0);
}

void MainComponent::loadBundledMusic()
{
    auto appFile = juce::File::getSpecialLocation (juce::File::currentApplicationFile);
    DBG ("App location: " + appFile.getFullPathName());

    auto resourcesFolder = appFile
                               .getChildFile ("Contents")
                               .getChildFile ("Resources")
                               .getChildFile ("Resources")
                               .getChildFile ("Modular Radio - All Tracks");

    DBG ("Looking for music folder at: " + resourcesFolder.getFullPathName());

    if (resourcesFolder.exists() && resourcesFolder.isDirectory())
    {
        loadTracksFromFolder (resourcesFolder);
        DBG ("Loaded bundled music from: " + resourcesFolder.getFullPathName());
    }
    else
    {
        DBG ("Bundled music not found at macOS path, trying iOS path...");

        // Try iOS app bundle structure (single Resources folder)
        auto iosResourcesFolder = appFile
                                    .getChildFile ("Resources")
                                    .getChildFile ("Modular Radio - All Tracks");

        DBG ("Trying iOS path: " + iosResourcesFolder.getFullPathName());

        if (iosResourcesFolder.exists() && iosResourcesFolder.isDirectory())
        {
            loadTracksFromFolder (iosResourcesFolder);
            DBG ("Loaded bundled music from iOS path: " + iosResourcesFolder.getFullPathName());
        }
        else
        {
            // Try iOS with double Resources path
            auto iosDoubleResourcesFolder = appFile
                                            .getChildFile ("Resources")
                                            .getChildFile ("Resources")
                                            .getChildFile ("Modular Radio - All Tracks");

            DBG ("Trying iOS double Resources path: " + iosDoubleResourcesFolder.getFullPathName());

            if (iosDoubleResourcesFolder.exists() && iosDoubleResourcesFolder.isDirectory())
            {
                loadTracksFromFolder (iosDoubleResourcesFolder);
                DBG ("Loaded bundled music from iOS double Resources path: " + iosDoubleResourcesFolder.getFullPathName());
            }
            else
            {
                DBG ("Bundled music not found at any path");
                DBG ("MacOS path: " + resourcesFolder.getFullPathName());
                DBG ("iOS path: " + iosResourcesFolder.getFullPathName());
                DBG ("iOS double Resources path: " + iosDoubleResourcesFolder.getFullPathName());
                trackNameLabel.setText ("Music not found in bundle", juce::dontSendNotification);
            }
        }
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

// POSITION SAVING/LOADING COMPLETELY REMOVED - all components are now in FIXED positions
