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

        // LAYER 1: Outer light grey ring
        auto outerRadius = radius * 0.85f;
        g.setColour (juce::Colour (0xffe0e0e0));  // Very light grey
        g.fillEllipse (centerX - outerRadius, centerY - outerRadius, outerRadius * 2, outerRadius * 2);

        // LAYER 2: Middle darker grey ring
        auto middleRadius = outerRadius * 0.75f;
        g.setColour (juce::Colour (0xffc0c0c0));  // Medium grey
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
        for (int i = 0; i < 40; ++i)  // 40 ticks
        {
            auto tickAngle = juce::degreesToRadians (i * 9.0f);  // Every 9 degrees

            // Major ticks every 5th mark - THIN!
            auto tickLength = (i % 5 == 0) ? 12.0f : 6.0f;
            auto tickWidth = (i % 5 == 0) ? 1.5f : 1.0f;  // Much thinner!

            // Ticks are INSIDE - start at outer edge and go inward
            auto startRadius = outerRadius - 2.0f;  // Start at outer ring edge
            auto endRadius = startRadius - tickLength;  // Go inward

            auto startX = centerX + startRadius * std::sin (tickAngle);
            auto startY = centerY - startRadius * std::cos (tickAngle);
            auto endX = centerX + endRadius * std::sin (tickAngle);
            auto endY = centerY - endRadius * std::cos (tickAngle);

            g.drawLine (startX, startY, endX, endY, tickWidth);
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

        // Draw circular indicator button (retro style)
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

    void drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat();

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

        g.setColour (juce::Colours::black);

        // Draw transport icons matching SwiftUI
        if (button.getButtonText() == "Play" || button.getButtonText() == "Pause")
        {
            // Play/Pause icon (size 24 in SwiftUI)
            auto iconSize = 24.0f;
            if (button.getButtonText() == "Play")
            {
                // Play triangle
                juce::Path play;
                play.addTriangle (centerX - iconSize/3 + 2, centerY - iconSize/2,
                                 centerX - iconSize/3 + 2, centerY + iconSize/2,
                                 centerX + iconSize/2 + 2, centerY);
                g.fillPath (play);
            }
            else
            {
                // Pause bars
                auto barWidth = iconSize * 0.25f;
                auto barHeight = iconSize * 0.8f;
                auto spacing = iconSize * 0.2f;
                g.fillRect (centerX - spacing - barWidth, centerY - barHeight/2, barWidth, barHeight);
                g.fillRect (centerX + spacing, centerY - barHeight/2, barWidth, barHeight);
            }
        }
        else if (button.getButtonText() == "Previous")
        {
            // Previous arrow (backward.fill, size 28)
            auto iconSize = 28.0f;
            juce::Path prev;
            // Left triangle
            prev.addTriangle (centerX - iconSize/3, centerY - iconSize/3,
                            centerX - iconSize/3, centerY + iconSize/3,
                            centerX - iconSize/2, centerY);
            // Right triangle
            prev.addTriangle (centerX + iconSize/6, centerY - iconSize/3,
                            centerX + iconSize/6, centerY + iconSize/3,
                            centerX - iconSize/6, centerY);
            g.fillPath (prev);
        }
        else if (button.getButtonText() == "Next")
        {
            // Next arrow (forward.fill, size 28)
            auto iconSize = 28.0f;
            juce::Path next;
            // Left triangle
            next.addTriangle (centerX - iconSize/6, centerY - iconSize/3,
                            centerX - iconSize/6, centerY + iconSize/3,
                            centerX + iconSize/6, centerY);
            // Right triangle
            next.addTriangle (centerX + iconSize/3, centerY - iconSize/3,
                            centerX + iconSize/3, centerY + iconSize/3,
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
            if (bypassCallback) bypassCallback (!bypassButton.getToggleState());  // Inverted: green = NOT bypassed
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
        g.setFont (juce::Font (juce::FontOptions().withHeight(14.0f)).boldened());
        g.drawText (effectName.toUpperCase(), 30, 0, getWidth() - 30, 20, juce::Justification::left);

        // Draw parameter labels
        g.setFont (juce::Font (juce::FontOptions().withHeight(9.0f)));
        g.drawText (param1Name, 90, 45, 40, 20, juce::Justification::left);
        g.drawText (param2Name, 90, 73, 40, 20, juce::Justification::left);
    }

    void resized() override
    {
        // Layout: Bypass indicator next to name, BIGGER knob on left, LONGER sliders on right
        bypassButton.setBounds (0, 0, 25, 20);  // Small circular button next to effect name
        knob.setBounds (0, 25, 120, 120);  // BIGGER! Was 80x80, now 120x120
        slider1.setBounds (145, 58, 160, 20);  // MUCH LONGER! Was 100, now 160
        slider2.setBounds (145, 96, 160, 20);  // MUCH LONGER! Was 100, now 160
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
