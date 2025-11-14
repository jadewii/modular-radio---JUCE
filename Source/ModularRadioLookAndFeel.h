#pragma once

#include <JuceHeader.h>

/**
 * Custom Look and Feel for Modular Radio
 * Matches the Swift app's visual style
 */
class ModularRadioLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ModularRadioLookAndFeel()
    {
        setColour (juce::Slider::thumbColourId, juce::Colours::white);
        setColour (juce::Slider::trackColourId, juce::Colours::white);
        setColour (juce::Slider::backgroundColourId, juce::Colours::transparentBlack);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider& slider) override
    {
        auto bounds = juce::Rectangle<float> (x, y, width, height);
        auto centerX = bounds.getCentreX();
        auto centerY = bounds.getCentreY();
        auto diameter = juce::jmin (bounds.getWidth(), bounds.getHeight());
        auto radius = diameter / 2.0f;
        auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

        // Check if this is the main pitch knob (larger than 200px)
        bool isPitchKnob = diameter > 200.0f;

        // LAYER 1: Outer light grey ring
        auto outerRadius = radius * 0.85f;
        g.setColour (juce::Colour (0xffe0e0e0));  // Very light grey
        g.fillEllipse (centerX - outerRadius, centerY - outerRadius, outerRadius * 2, outerRadius * 2);

        // LAYER 2: Middle lighter grey ring
        auto middleRadius = outerRadius * 0.75f;
        g.setColour (juce::Colour (0xffd0d0d0));  // Lighter grey (matching reference)
        g.fillEllipse (centerX - middleRadius, centerY - middleRadius, middleRadius * 2, middleRadius * 2);

        // LAYER 3: Inner white/light center circle
        auto innerRadius = middleRadius * 0.7f;
        g.setGradientFill (juce::ColourGradient (
            juce::Colour (0xfff5f5f5), centerX, centerY - innerRadius,
            juce::Colour (0xffe8e8e8), centerX, centerY + innerRadius,
            false));
        g.fillEllipse (centerX - innerRadius, centerY - innerRadius, innerRadius * 2, innerRadius * 2);

        // Draw tick marks INSIDE on the outer grey ring - THIN and SUBTLE
        g.setColour (juce::Colour (0xff888888));  // Light grey (not dark!)

        if (isPitchKnob)
        {
            // Pitch knob: Draw ticks all the way around (360 degrees)
            for (int i = 0; i < 40; ++i)  // 40 ticks around full circle
            {
                auto tickAngle = juce::degreesToRadians (i * 9.0f);  // Every 9 degrees

                // Major ticks every 5th mark - THIN!
                auto tickLength = (i % 5 == 0) ? 12.0f : 6.0f;
                auto tickWidth = (i % 5 == 0) ? 1.5f : 1.0f;

                // Ticks are INSIDE - start at outer edge and go inward
                auto startRadius = outerRadius - 2.0f;
                auto endRadius = startRadius - tickLength;

                auto startX = centerX + startRadius * std::sin (tickAngle);
                auto startY = centerY - startRadius * std::cos (tickAngle);
                auto endX = centerX + endRadius * std::sin (tickAngle);
                auto endY = centerY - endRadius * std::cos (tickAngle);

                g.drawLine (startX, startY, endX, endY, tickWidth);
            }
        }
        else
        {
            // Regular knobs: Only draw ticks in the reachable range
            auto angleRange = rotaryEndAngle - rotaryStartAngle;
            int numTicks = 30;  // Number of ticks in the reachable range

            for (int i = 0; i <= numTicks; ++i)
            {
                // Map tick position to the actual angle range
                float t = static_cast<float>(i) / static_cast<float>(numTicks);
                auto tickAngle = rotaryStartAngle + t * angleRange;

                // Major ticks every 5th mark - THIN!
                auto tickLength = (i % 5 == 0) ? 12.0f : 6.0f;
                auto tickWidth = (i % 5 == 0) ? 1.5f : 1.0f;

                // Ticks are INSIDE - start at outer edge and go inward
                auto startRadius = outerRadius - 2.0f;
                auto endRadius = startRadius - tickLength;

                auto startX = centerX + startRadius * std::sin (tickAngle);
                auto startY = centerY - startRadius * std::cos (tickAngle);
                auto endX = centerX + endRadius * std::sin (tickAngle);
                auto endY = centerY - endRadius * std::cos (tickAngle);

                g.drawLine (startX, startY, endX, endY, tickWidth);
            }
        }

        // Draw indicator line - starts from MIDDLE of the dark grey ring
        juce::Path indicator;
        auto indicatorStart = (middleRadius + innerRadius) / 2.0f;  // Middle of dark grey ring
        auto indicatorEnd = outerRadius - 5.0f;      // End near outer edge
        indicator.addLineSegment (
            juce::Line<float> (centerX, centerY - indicatorStart,
                              centerX, centerY - indicatorEnd), 2.5f);  // Thinner

        g.setColour (juce::Colours::black);
        g.fillPath (indicator, juce::AffineTransform::rotation (angle, centerX, centerY));
    }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float minSliderPos, float maxSliderPos,
                          const juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        if (style == juce::Slider::LinearHorizontal)
        {
            auto trackBounds = juce::Rectangle<float> (x, y + height / 2 - 3, width, 6);

            // Get the custom track color from the slider's property
            auto trackColor = slider.findColour (juce::Slider::trackColourId);

            // Draw background track (lighter color)
            g.setColour (trackColor.withAlpha (0.3f));
            g.fillRoundedRectangle (trackBounds, 3.0f);

            // Draw filled track (white)
            auto filledTrack = trackBounds.withWidth (sliderPos - x);
            g.setColour (juce::Colours::white);
            g.fillRoundedRectangle (filledTrack, 3.0f);

            // Draw thumb
            auto thumbRadius = 9.0f;
            auto thumbX = sliderPos;
            auto thumbY = y + height / 2;

            g.setGradientFill (juce::ColourGradient (
                juce::Colour (0xffd8d8d8), thumbX, thumbY - thumbRadius,
                juce::Colour (0xffbfbfbf), thumbX, thumbY + thumbRadius,
                false));
            g.fillEllipse (thumbX - thumbRadius, thumbY - thumbRadius, thumbRadius * 2, thumbRadius * 2);

            g.setColour (juce::Colours::black.withAlpha (0.3f));
            g.drawEllipse (thumbX - thumbRadius, thumbY - thumbRadius, thumbRadius * 2, thumbRadius * 2, 1.0f);
        }
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                          bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat();
        auto buttonText = button.getButtonText();

        // Check if this is a filter type button (HP/LP/BP)
        if (buttonText == "HP" || buttonText == "LP" || buttonText == "BP")
        {
            auto centerX = bounds.getCentreX();
            auto centerY = bounds.getCentreY();
            auto diameter = juce::jmin(bounds.getWidth(), bounds.getHeight()) - 4;

            // Outer circle (black border)
            g.setColour (juce::Colours::black);
            g.fillEllipse (centerX - diameter/2, centerY - diameter/2, diameter, diameter);

            // Inner circle (colored when ON, grey when OFF)
            auto innerDiameter = diameter - 5;
            if (button.getToggleState())
            {
                // Colored when active (green for filter)
                g.setColour (juce::Colour (0xff4CAF50));  // Green
            }
            else
            {
                // Grey when inactive
                g.setColour (juce::Colour (0xff909090));
            }
            g.fillEllipse (centerX - innerDiameter/2, centerY - innerDiameter/2, innerDiameter, innerDiameter);

            // Draw label text INSIDE the circle (centered)
            g.setColour (juce::Colours::white);
            g.setFont (juce::Font (juce::FontOptions().withHeight(10.0f)).boldened());
            g.drawText (buttonText, bounds, juce::Justification::centred);
        }
        else if (button.getWidth() >= 40 && buttonText.isEmpty())  // FX randomize button - circular
        {
            auto centerX = bounds.getCentreX();
            auto centerY = bounds.getCentreY();
            auto diameter = juce::jmin(bounds.getWidth(), bounds.getHeight()) - 4;

            // Outer circle (black border)
            g.setColour (juce::Colours::black);
            g.fillEllipse (centerX - diameter/2, centerY - diameter/2, diameter, diameter);

            // Inner circle - WHITE normally, PINK when pressed
            auto innerDiameter = diameter - 6;
            // Use button press state instead of flashing property
            g.setColour (shouldDrawButtonAsDown ? juce::Colour (0xffFF1493) : juce::Colours::white);  // White normally, hot pink when pressed
            g.fillEllipse (centerX - innerDiameter/2, centerY - innerDiameter/2, innerDiameter, innerDiameter);
        }
        else  // Small circular bypass indicators
        {
            auto indicatorSize = 20.0f;
            auto indicatorX = bounds.getX() + 5;
            auto indicatorY = bounds.getCentreY() - indicatorSize / 2;

            // Outer circle (border)
            g.setColour (juce::Colours::black);
            g.fillEllipse (indicatorX, indicatorY, indicatorSize, indicatorSize);

            // Inner circle (indicator)
            auto innerSize = indicatorSize - 4.0f;
            auto innerX = indicatorX + 2.0f;
            auto innerY = indicatorY + 2.0f;

            if (button.getToggleState())
            {
                // Green when active (NOT bypassed)
                g.setColour (juce::Colour (0xff00ff00));  // Bright green
            }
            else
            {
                // Grey when bypassed
                g.setColour (juce::Colour (0xff505050));  // Dark grey
            }
            g.fillEllipse (innerX, innerY, innerSize, innerSize);

            // Draw button text (if any)
            if (button.getButtonText().isNotEmpty())
            {
                g.setColour (juce::Colours::black);
                g.setFont (juce::Font (juce::FontOptions().withHeight(10.0f)).boldened());
                g.drawText (button.getButtonText(), indicatorX + indicatorSize + 5, indicatorY,
                           bounds.getWidth() - indicatorSize - 10, indicatorSize,
                           juce::Justification::centredLeft);
            }
        }
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat();

        // Check if this is the RESET button
        if (button.getButtonText() == "RESET")
        {
            // Clear background (transparent), white outline
            g.setColour (juce::Colours::transparentBlack);
            g.fillRoundedRectangle (bounds, 5.0f);

            // White outline
            g.setColour (juce::Colours::white);
            g.drawRoundedRectangle (bounds.reduced (1), 5.0f, 2.0f);
            return;
        }

        // Check if this is the play button (60x60) vs prev/next (50x50)
        if (button.getWidth() == 60 && button.getHeight() == 60)
        {
            // Play button - draw circle outline (matching SwiftUI: 60x60, 3px stroke)
            g.setColour (juce::Colours::black);
            g.drawEllipse (bounds.reduced (2), 3.0f);
        }
        // Don't draw background for prev/next buttons (just icons)
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& button, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat();
        auto centerX = bounds.getCentreX();
        auto centerY = bounds.getCentreY();

        // Check if this is the RESET button - draw white text
        if (button.getButtonText() == "RESET")
        {
            g.setColour (juce::Colours::white);
            g.setFont (juce::Font (juce::FontOptions().withHeight(14.0f)).boldened());
            g.drawText ("RESET", bounds, juce::Justification::centred);
            return;
        }

        g.setColour (juce::Colours::black);

        // Draw transport icons - BOLD and FILLED
        if (button.getButtonText() == "Play" || button.getButtonText() == "Pause")
        {
            if (button.getButtonText() == "Play")
            {
                // Play triangle - BOLD and filled
                auto iconSize = 20.0f;
                juce::Path play;
                play.addTriangle (centerX - iconSize/3, centerY - iconSize/2,
                                 centerX - iconSize/3, centerY + iconSize/2,
                                 centerX + iconSize/2, centerY);
                g.fillPath (play);
            }
            else
            {
                // Pause bars - thinner and more elegant
                auto barWidth = 5.0f;
                auto barHeight = 18.0f;
                auto spacing = 4.0f;
                g.fillRect (centerX - spacing - barWidth, centerY - barHeight/2, barWidth, barHeight);
                g.fillRect (centerX + spacing, centerY - barHeight/2, barWidth, barHeight);
            }
        }
        else if (button.getButtonText() == "Previous")
        {
            // Previous - two triangles pointing LEFT (like ◄◄) touching with NO space - 15% LARGER
            auto iconSize = 23.0f;  // Was 20.0f, now 15% larger
            juce::Path prev;

            // Right triangle (pointing left) - this one is on the right side
            prev.addTriangle (centerX + iconSize/2, centerY - iconSize/2,
                            centerX + iconSize/2, centerY + iconSize/2,
                            centerX, centerY);

            // Left triangle (pointing left) - this one is on the left side, touching the right triangle
            prev.addTriangle (centerX, centerY - iconSize/2,
                            centerX, centerY + iconSize/2,
                            centerX - iconSize/2, centerY);
            g.fillPath (prev);
        }
        else if (button.getButtonText() == "Next")
        {
            // Next - two triangles pointing RIGHT (like ►►) touching with NO space - 15% LARGER
            auto iconSize = 23.0f;  // Was 20.0f, now 15% larger
            juce::Path next;

            // Left triangle (pointing right) - this one is on the left side
            next.addTriangle (centerX - iconSize/2, centerY - iconSize/2,
                            centerX - iconSize/2, centerY + iconSize/2,
                            centerX, centerY);

            // Right triangle (pointing right) - this one is on the right side, touching the left triangle
            next.addTriangle (centerX, centerY - iconSize/2,
                            centerX, centerY + iconSize/2,
                            centerX + iconSize/2, centerY);
            g.fillPath (next);
        }
    }
};

/**
 * Effect Knob Group Component
 * Contains 1 rotary knob + 2 horizontal sliders
 */
class EffectKnobGroup : public juce::Component
{
public:
    EffectKnobGroup (const juce::String& name, const juce::String& param1, const juce::String& param2,
                     juce::Colour color, std::function<void(float)> onKnobChange,
                     std::function<void(float)> onParam1Change, std::function<void(float)> onParam2Change)
        : effectName (name), param1Name (param1), param2Name (param2), effectColor (color),
          knobCallback (onKnobChange), param1Callback (onParam1Change), param2Callback (onParam2Change)
    {
        // Setup knob
        knob.setSliderStyle (juce::Slider::Rotary);
        knob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        knob.setRange (0.0, 1.0, 0.01);
        knob.setValue (0.5);
        knob.setLookAndFeel (&customLookAndFeel);
        knob.onValueChange = [this] { if (knobCallback) knobCallback (knob.getValue()); };
        addAndMakeVisible (knob);

        // Setup slider 1
        slider1.setSliderStyle (juce::Slider::LinearHorizontal);
        slider1.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider1.setRange (0.0, 1.0, 0.01);
        slider1.setValue (0.5);
        slider1.setColour (juce::Slider::trackColourId, effectColor);
        slider1.setLookAndFeel (&customLookAndFeel);
        slider1.onValueChange = [this] { if (param1Callback) param1Callback (slider1.getValue()); };
        addAndMakeVisible (slider1);

        // Setup slider 2
        slider2.setSliderStyle (juce::Slider::LinearHorizontal);
        slider2.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider2.setRange (0.0, 1.0, 0.01);
        slider2.setValue (0.5);
        slider2.setColour (juce::Slider::trackColourId, effectColor);
        slider2.setLookAndFeel (&customLookAndFeel);
        slider2.onValueChange = [this] { if (param2Callback) param2Callback (slider2.getValue()); };
        addAndMakeVisible (slider2);

        // Setup bypass button (circular indicator)
        bypassButton.setButtonText ("");  // No text, just indicator
        bypassButton.setClickingTogglesState (true);
        bypassButton.setToggleState (false, juce::dontSendNotification);  // Start with effect bypassed
        bypassButton.setLookAndFeel (&customLookAndFeel);
        bypassButton.onClick = [this] {
            if (bypassCallback) bypassCallback (!bypassButton.getToggleState());  // Inverted: green (ON) = NOT bypassed
            repaint();
        };
        addAndMakeVisible (bypassButton);
    }

    ~EffectKnobGroup()
    {
        knob.setLookAndFeel (nullptr);
        slider1.setLookAndFeel (nullptr);
        slider2.setLookAndFeel (nullptr);
        bypassButton.setLookAndFeel (nullptr);
    }

    void paint (juce::Graphics& g) override
    {
        // Draw effect name with bypass indicator inline
        g.setColour (juce::Colours::black);
        g.setFont (juce::Font (juce::FontOptions().withHeight(16.0f)).boldened());
        g.drawText (effectName.toUpperCase(), 35, 0, getWidth() - 35, 25, juce::Justification::left);

        // Draw parameter labels (positioned to the right of the knob)
        g.setFont (juce::Font (juce::FontOptions().withHeight(11.0f)));
        g.drawText (param1Name, 135, 70, 50, 25, juce::Justification::right);
        if (param2Name.isNotEmpty())
            g.drawText (param2Name, 135, 115, 50, 25, juce::Justification::right);
    }

    void resized() override
    {
        // Layout: Bypass indicator next to name, knob on left (original size), sliders on right with labels
        bypassButton.setBounds (0, 0, 36, 26);  // 20% larger (was 30x22)
        knob.setBounds (10, 40, 120, 120);  // Original larger size, positioned left
        slider1.setBounds (190, 75, 160, 25);  // Sliders positioned to the right of labels
        slider2.setBounds (190, 120, 160, 25);
    }

    void setBypassCallback (std::function<void(bool)> callback)
    {
        bypassCallback = callback;
    }

    juce::Slider& getKnob() { return knob; }
    juce::Slider& getSlider1() { return slider1; }
    juce::Slider& getSlider2() { return slider2; }
    juce::ToggleButton& getBypassButton() { return bypassButton; }

private:
    juce::String effectName;
    juce::String param1Name;
    juce::String param2Name;
    juce::Colour effectColor;

    juce::Slider knob;
    juce::Slider slider1;
    juce::Slider slider2;
    juce::ToggleButton bypassButton;

    ModularRadioLookAndFeel customLookAndFeel;

    std::function<void(float)> knobCallback;
    std::function<void(float)> param1Callback;
    std::function<void(float)> param2Callback;
    std::function<void(bool)> bypassCallback;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EffectKnobGroup)
};

/**
 * Simple Volume Knob Component
 * Just one big knob with label - no bypass button
 */
class VolumeKnob : public juce::Component
{
public:
    VolumeKnob (std::function<void(float)> onValueChange)
        : valueCallback (onValueChange)
    {
        // Setup knob
        knob.setSliderStyle (juce::Slider::Rotary);
        knob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        knob.setRange (0.0, 1.0, 0.01);
        knob.setValue (0.7);  // Default to 70% volume
        knob.setLookAndFeel (&customLookAndFeel);
        knob.onValueChange = [this] { if (valueCallback) valueCallback (knob.getValue()); };
        addAndMakeVisible (knob);
    }

    ~VolumeKnob()
    {
        knob.setLookAndFeel (nullptr);
    }

    void paint (juce::Graphics& g) override
    {
        // Draw "VOLUME" label to the left of the knob
        g.setColour (juce::Colours::black);
        g.setFont (juce::Font (juce::FontOptions().withHeight(24.0f)).boldened());
        g.drawText ("VOLUME", 0, 0, 100, getHeight(), juce::Justification::centredLeft);
    }

    void resized() override
    {
        // Position knob to the right of the label - 30% smaller than 240 = 168x168
        knob.setBounds (120, 0, 168, 168);
    }

    juce::Slider& getKnob() { return knob; }

private:
    juce::Slider knob;
    ModularRadioLookAndFeel customLookAndFeel;
    std::function<void(float)> valueCallback;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VolumeKnob)
};

/**
 * Draggable Filter Buttons Group
 * Contains the 3 filter type buttons (HP, LP, BP) that can be dragged as a group
 */
class DraggableFilterButtons : public juce::Component
{
public:
    DraggableFilterButtons (juce::ToggleButton& hp, juce::ToggleButton& lp, juce::ToggleButton& bp)
        : hpButton(hp), lpButton(lp), bpButton(bp)
    {
        addAndMakeVisible (hpButton);
        addAndMakeVisible (lpButton);
        addAndMakeVisible (bpButton);

        // Load saved position
        loadPosition();

        setSize (120, 40);  // Size to contain all 3 buttons
    }

    void paint (juce::Graphics& g) override
    {
        // Draw a subtle border when dragging to show the group
        if (isDragging)
        {
            g.setColour (juce::Colours::white.withAlpha (0.3f));
            g.drawRect (getLocalBounds(), 2);

            // Draw a small label
            g.setColour (juce::Colours::white.withAlpha (0.7f));
            g.setFont (juce::Font (juce::FontOptions().withHeight(10.0f)));
            g.drawText ("FILTER TYPE", 0, -15, getWidth(), 15, juce::Justification::centred);
        }
    }

    void resized() override
    {
        // Position the 3 buttons horizontally
        hpButton.setBounds (0, 0, 35, 35);
        lpButton.setBounds (40, 0, 35, 35);
        bpButton.setBounds (80, 0, 35, 35);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        isDragging = true;
        dragStartPos = getPosition();
        mouseDownPos = e.getPosition();
        repaint();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (isDragging)
        {
            auto newPos = dragStartPos + (e.getPosition() - mouseDownPos);

            // Constrain to parent bounds
            if (auto* parent = getParentComponent())
            {
                auto parentBounds = parent->getLocalBounds();
                newPos.x = juce::jlimit (0, parentBounds.getWidth() - getWidth(), newPos.x);
                newPos.y = juce::jlimit (0, parentBounds.getHeight() - getHeight(), newPos.y);
            }

            setTopLeftPosition (newPos);
        }
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (isDragging)
        {
            isDragging = false;
            savePosition();
            repaint();

            // Print position for debugging
            DBG ("Filter buttons positioned at: " << getX() << ", " << getY());
        }
    }

private:
    juce::ToggleButton& hpButton;
    juce::ToggleButton& lpButton;
    juce::ToggleButton& bpButton;

    bool isDragging = false;
    juce::Point<int> dragStartPos;
    juce::Point<int> mouseDownPos;

    void savePosition()
    {
        // Save position to user preferences
        juce::PropertiesFile::Options options;
        options.applicationName = "ModularRadio";
        options.filenameSuffix = ".settings";
        options.osxLibrarySubFolder = "Application Support";

        auto props = std::make_unique<juce::PropertiesFile> (options);
        props->setValue ("filterButtonsX", getX());
        props->setValue ("filterButtonsY", getY());
        props->save();
    }

    void loadPosition()
    {
        // Load position from user preferences
        juce::PropertiesFile::Options options;
        options.applicationName = "ModularRadio";
        options.filenameSuffix = ".settings";
        options.osxLibrarySubFolder = "Application Support";

        auto props = std::make_unique<juce::PropertiesFile> (options);
        int x = props->getIntValue ("filterButtonsX", 175);  // Default position
        int y = props->getIntValue ("filterButtonsY", 45);

        setTopLeftPosition (x, y);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DraggableFilterButtons)
};
