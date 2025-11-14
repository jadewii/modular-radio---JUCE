#include "MainComponent.h"
#include <random>
#include <algorithm>

MainComponent::MainComponent()
{
    // Register audio formats
    formatManager.registerBasicFormats();
    transportSource.addChangeListener (this);

    // Load background image from binary data
    backgroundImage = juce::ImageFileFormat::loadFrom(BinaryData::modback_png, BinaryData::modback_pngSize);

    // Load module image from binary data
    moduleImage = juce::ImageFileFormat::loadFrom(BinaryData::modularapp_PNG, BinaryData::modularapp_PNGSize);

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
    trackNameLabel.setColour (juce::Label::textColourId, juce::Colours::black);
    trackNameLabel.setFont (juce::Font (juce::FontOptions().withHeight(19.2f)));
    addAndMakeVisible (trackNameLabel);

    // Pitch knob (center module) - 180-degree rotation range centered at top
    pitchKnob.setSliderStyle (juce::Slider::Rotary);
    pitchKnob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    pitchKnob.setRange (-12.0, 12.0, 0.1);  // -12 to +12 semitones
    pitchKnob.setValue (0.0);  // Start at center (no pitch shift)

    // PITCH KNOB: 360° sweep - center at TOP (12 o'clock), shifted to fix alignment
    pitchKnob.setRotaryParameters (
        juce::MathConstants<float>::pi * 1.0f,   // 180° = 9 o'clock (left, START)
        juce::MathConstants<float>::pi * 3.0f,   // 540° = 9 o'clock (left after full rotation, END)
        true                                      // Stop at end (don't wrap around)
    );

    pitchKnob.setLookAndFeel (&customLookAndFeel);

    // REAL-TIME pitch shifting with ResamplingAudioSource (turntable-style)
    pitchKnob.onValueChange = [this] {
        float semitones = pitchKnob.getValue();
        currentPitchSemitones = semitones;

        // Update resampling ratio in REAL-TIME (smooth, click-free!)
        if (pitchShifter != nullptr)
            pitchShifter->setPitchSemitones (semitones);
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

        // Randomize all effects (would need effect groups - simplified for now)
    };

    addAndMakeVisible (fxToggleButton);

    // LED indicator - FIXED POSITION
    ledIndicator.setText ("", juce::dontSendNotification);
    ledIndicator.setColour (juce::Label::backgroundColourId, juce::Colour (0xff1a4d1a));  // Dark green
    ledIndicator.setColour (juce::Label::textColourId, juce::Colours::transparentBlack);
    ledIndicator.setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (ledIndicator);

    // Initialize embedded track list
    trackList.add({BinaryData::Misty_Reverie_m4a, BinaryData::Misty_Reverie_m4aSize, "Misty Reverie"});
    trackList.add({BinaryData::Charlatan_m4a, BinaryData::Charlatan_m4aSize, "Charlatan"});
    trackList.add({BinaryData::The_Flow_of_Times_River_m4a, BinaryData::The_Flow_of_Times_River_m4aSize, "The Flow of Time's River"});

    // SHUFFLE tracks randomly on every startup for randomized playback order (same as macOS)
    if (!trackList.isEmpty())
    {
        std::random_device rd;
        std::mt19937 rng (rd());
        std::shuffle (trackList.begin(), trackList.end(), rng);
        DBG ("Shuffled " << trackList.size() << " tracks into random order");
    }

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

    // Distortion: Knob=Drive, Sliders: Mix, (empty)
    distortionGroup = std::make_unique<EffectKnobGroup> ("Distortion", "MIX", "",
        juce::Colours::red,
        [this](float v) { effectsProcessor.setDistortionDrive(v); },
        [this](float v) { effectsProcessor.setDistortionMix(v); },
        [this](float v) { });
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

    // Filter type buttons (HP, LP, BP) - radio button group
    filterHPButton.setButtonText ("HP");
    filterHPButton.setClickingTogglesState (true);
    filterHPButton.setRadioGroupId (1001);
    filterHPButton.setLookAndFeel (&customLookAndFeel);
    filterHPButton.onClick = [this] { effectsProcessor.setFilterType(1); };  // High-pass
    filterGroup->addAndMakeVisible (filterHPButton);

    filterLPButton.setButtonText ("LP");
    filterLPButton.setClickingTogglesState (true);
    filterLPButton.setRadioGroupId (1001);
    filterLPButton.setLookAndFeel (&customLookAndFeel);
    filterLPButton.setToggleState (true, juce::dontSendNotification);  // Default to LP
    filterLPButton.onClick = [this] { effectsProcessor.setFilterType(0); };  // Low-pass
    filterGroup->addAndMakeVisible (filterLPButton);

    filterBPButton.setButtonText ("BP");
    filterBPButton.setClickingTogglesState (true);
    filterBPButton.setRadioGroupId (1001);
    filterBPButton.setLookAndFeel (&customLookAndFeel);
    filterBPButton.onClick = [this] { effectsProcessor.setFilterType(2); };  // Band-pass
    filterGroup->addAndMakeVisible (filterBPButton);

    addAndMakeVisible (filterGroup.get());

    // Bitcrusher: Knob=BitDepth, Sliders: Crush, Mix
    timeGroup = std::make_unique<EffectKnobGroup> ("BitCrush", "CRUSH", "MIX",
        juce::Colours::purple,
        [this](float v) { effectsProcessor.setBitcrusherBitDepth(v); },
        [this](float v) { effectsProcessor.setBitcrusherCrush(v); },
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

    // Load first track
    loadTrack(0);

    // Start timer
    startTimer (500);

    // Start with reference size for perfect scaling
    setSize (800, 600);
    setAudioChannels (0, 2);
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

    // Set up tone generator
    currentSampleRate = sampleRate;
    angleDelta = 2.0 * juce::MathConstants<double>::pi * 440.0 / sampleRate; // 440 Hz tone

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
    effectsProcessor.setFilterType (0);  // Low-pass by default

    // Bitcrusher effect initialization
    effectsProcessor.setBitcrusherBitDepth (timeGroup->getKnob().getValue());
    effectsProcessor.setBitcrusherCrush (timeGroup->getSlider1().getValue());
    effectsProcessor.setBitcrusherMix (timeGroup->getSlider2().getValue());
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

    // Draw center module overlay image behind transport controls
    if (moduleImage.isValid())
    {
        // Use the same scaling system as the components
        const float refWidth = 1400.0f;
        const float refHeight = 900.0f;
        auto bounds = getLocalBounds();
        float scaleX = bounds.getWidth() / refWidth;
        float scaleY = bounds.getHeight() / refHeight;
        float scale = juce::jmin(scaleX, scaleY);

        // Center module bounds (480x640 in reference)
        auto refCenterX = refWidth / 2;   // 700
        auto refCenterY = refHeight / 2;  // 450
        auto refModuleX = refCenterX - 240;  // 460
        auto refModuleY = refCenterY - 320;  // 130

        // Scale the module image bounds
        auto scaledModuleX = static_cast<int>(refModuleX * scale);
        auto scaledModuleY = static_cast<int>(refModuleY * scale);
        auto scaledModuleWidth = static_cast<int>(480 * scale);
        auto scaledModuleHeight = static_cast<int>(640 * scale);

        g.drawImage (moduleImage, scaledModuleX, scaledModuleY, scaledModuleWidth, scaledModuleHeight,
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

    // iPad layout exactly matching macOS reference image
    auto centerX = bounds.getWidth() / 2;
    auto centerY = bounds.getHeight() / 2;

    // Effect groups sized to match macOS version
    auto effectWidth = 320;
    auto effectHeight = 160;

    // *** PROPORTIONAL SCALING - EXACT COPY FROM MACOS VERSION ***
    // Reference dimensions from macOS
    const float refWidth = 1400.0f;
    const float refHeight = 900.0f;

    // *** CENTER MODULE - EXACT COPY FROM MACOS POSITIONING ***
    auto refCenterX = refWidth / 2;   // 700
    auto refCenterY = refHeight / 2;  // 450

    // Center module bounds (480x640 in reference)
    auto refModuleX = refCenterX - 240;  // 460
    auto refModuleY = refCenterY - 320;  // 130

    // Calculate scale factor for iPad
    float scaleX = bounds.getWidth() / refWidth;
    float scaleY = bounds.getHeight() / refHeight;
    float scale = juce::jmin(scaleX, scaleY);

    // Helper function to scale coordinates (copied from macOS)
    auto scaleBounds = [scale](float x, float y, float w, float h) -> juce::Rectangle<int>
    {
        return juce::Rectangle<int>(
            static_cast<int>(x * scale),
            static_cast<int>(y * scale),
            static_cast<int>(w * scale),
            static_cast<int>(h * scale)
        );
    };

    // Pitch knob - exact macOS positioning
    pitchKnob.setBounds(scaleBounds(refModuleX + 123, refModuleY + 153, 234, 234));

    // FX button - exact macOS positioning
    fxToggleButton.setBounds(scaleBounds(refModuleX + 80, refModuleY + 415, 40, 40));

    // LED indicator - exact macOS positioning
    ledIndicator.setBounds(scaleBounds(refModuleX + 425, refModuleY + 155, 22, 22));

    // Transport controls - exact macOS positioning
    auto refTransportY = refModuleY + 484;
    auto refPlayButtonHeight = 60;
    auto refPrevNextY = refTransportY + (refPlayButtonHeight - 50) / 2;

    previousButton.setBounds(scaleBounds(refCenterX - 110, refPrevNextY, 50, 50));
    playButton.setBounds(scaleBounds(refCenterX - 30, refTransportY, 60, 60));
    nextButton.setBounds(scaleBounds(refCenterX + 60, refPrevNextY, 50, 50));

    // *** EFFECTS LAYOUT - EXACT COPY FROM MACOS VERSION ***

    // EXACT macOS positions scaled for iPad:
    // Left side effects
    filterGroup->setBounds(scaleBounds(10, 60, 360, 200));
    delayGroup->setBounds(scaleBounds(10, 280, 360, 200));
    reverbGroup->setBounds(scaleBounds(10, 500, 360, 200));
    timeGroup->setBounds(scaleBounds(10, 720, 360, 200));

    // Right side effects
    chorusGroup->setBounds(scaleBounds(1030, 60, 360, 200));
    distortionGroup->setBounds(scaleBounds(1030, 280, 360, 200));
    phaserGroup->setBounds(scaleBounds(1030, 500, 360, 200));

    // Don't apply internal scaling - the bounds are already scaled with scaleBounds()
    // This prevents double-scaling of the effect group contents

    // Filter type buttons within filter group - positioned ABOVE resonance slider to match macOS
    // Use original coordinates since the filterGroup itself is already scaled
    auto buttonSize = 35;  // Original size - no need to scale since parent is already scaled
    auto buttonY = 45;     // Position above RESO slider (which is at y=75)
    auto buttonStartX = 190; // Align with slider position
    auto buttonSpacing = 40; // Space between buttons

    filterHPButton.setBounds(buttonStartX, buttonY, buttonSize, buttonSize);
    filterLPButton.setBounds(buttonStartX + buttonSpacing, buttonY, buttonSize, buttonSize);
    filterBPButton.setBounds(buttonStartX + buttonSpacing * 2, buttonY, buttonSize, buttonSize);

    // Master volume - exact macOS positioning (right side, bottom)
    // Don't apply internal scaling - the bounds are already scaled with scaleBounds()
    volumeKnob->setBounds(scaleBounds(1030, 720, 360, 200));

    // Track info - exact macOS positioning
    auto refLabelAreaY = refModuleY + 590;
    trackNameLabel.setBounds(scaleBounds(refModuleX + 60, refLabelAreaY, 360, 30));
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
    // Update LED indicator based on playback state
    if (state == Playing)
    {
        ledIndicator.setColour (juce::Label::backgroundColourId, juce::Colour (0xff00ff00));  // Bright green
    }
    else
    {
        ledIndicator.setColour (juce::Label::backgroundColourId, juce::Colour (0xff1a4d1a));  // Dark green
    }

    // Handle FX button flash animation
    if (fxButtonFlashing && fxButtonFlashCounter > 0)
    {
        fxButtonFlashCounter--;
        bool flashState = (fxButtonFlashCounter % 2) == 0;
        fxToggleButton.getProperties().set ("flashing", flashState);

        if (fxButtonFlashCounter == 0)
        {
            fxButtonFlashing = false;
            fxToggleButton.getProperties().set ("flashing", false);
        }
    }
}

void MainComponent::loadTrack (int index)
{
    if (index < 0 || index >= trackList.size())
        return;

    transportSource.stop();
    transportSource.setSource (nullptr);
    pitchShifter.reset();
    readerSource.reset();

    // Reset pitch knob to center when changing tracks
    pitchKnob.setValue (0.0, juce::dontSendNotification);
    currentPitchSemitones = 0.0;

    // Load track from binary data
    auto& track = trackList.getReference(index);
    auto* inputStream = new juce::MemoryInputStream(track.data, track.size, false);
    auto* reader = formatManager.createReaderFor(std::unique_ptr<juce::InputStream>(inputStream));

    if (reader != nullptr)
    {
        readerSource.reset (new juce::AudioFormatReaderSource (reader, true));

        // Wrap reader source in ResamplingAudioSource for smooth, click-free pitch shifting
        pitchShifter.reset (new SmoothResamplingSource (readerSource.get(), false));
        pitchShifter->prepareToPlay (512, reader->sampleRate);
        pitchShifter->setPitchSemitones (currentPitchSemitones);  // Start at 0 (normal pitch)

        // Connect pitch shifter to transport
        transportSource.setSource (pitchShifter.get(), 0, nullptr, reader->sampleRate);

        currentTrackIndex = index;
        currentTrackName = track.name;
        trackNameLabel.setText (currentTrackName, juce::dontSendNotification);

        DBG ("Loaded: " << currentTrackName << " from binary data (0 semitones)");
    }
}

void MainComponent::loadTracksFromFolder (const juce::File& folder)
{
    // Not used on iOS - we embed tracks as binary data
}

void MainComponent::loadBundledMusic()
{
    // TODO: Load tracks from binary data instead of files
    trackNameLabel.setText ("ModularRadio v2 - Professional DJ System", juce::dontSendNotification);
}

void MainComponent::playButtonClicked()
{
    if (state == Stopped || state == Paused)
    {
        transportSource.start();
        state = Playing;
        playButton.setButtonText ("Pause");
    }
    else if (state == Playing)
    {
        transportSource.stop();
        state = Paused;
        playButton.setButtonText ("Play");
    }
}

void MainComponent::stopButtonClicked()
{
    state = Stopped;
    playButton.setButtonText ("Play");
}

void MainComponent::nextButtonClicked()
{
    if (trackList.isEmpty())
        return;

    int nextIndex = (currentTrackIndex + 1) % trackList.size();
    loadTrack (nextIndex);

    if (state == Playing)
        transportSource.start();
}

void MainComponent::previousButtonClicked()
{
    if (trackList.isEmpty())
        return;

    int prevIndex = currentTrackIndex - 1;
    if (prevIndex < 0)
        prevIndex = trackList.size() - 1;

    loadTrack (prevIndex);

    if (state == Playing)
        transportSource.start();
}

void MainComponent::updateTimeDisplay()
{
    // TODO: Implement time display
}