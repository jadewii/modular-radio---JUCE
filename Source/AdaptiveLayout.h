#pragma once

#include <JuceHeader.h>
#include "DeviceDetection.h"

/**
 * Adaptive layout system that provides device-specific positioning
 * Mimics the Swift version's dynamic layout system
 */
class AdaptiveLayout
{
public:
    struct LayoutBounds
    {
        juce::Point<float> position;
        float width;
        float height;
        float scale;
    };

    enum EffectType
    {
        Filter,
        Delay,
        Reverb,
        Chorus,
        Distortion,
        Phaser
    };

    enum PhoneLayoutElement
    {
        PhoneMainModule,
        PhoneTransport,
        PhoneEffectsTab
    };

    static void initializeForDevice(juce::Rectangle<int> bounds)
    {
        screenBounds = bounds;
        scaleFactor = DeviceDetection::getScaleFactor();
        centerX = bounds.getCentreX();
        centerY = bounds.getCentreY();

        DBG("Adaptive layout initialized for " << DeviceDetection::getDeviceString()
            << " with scale " << scaleFactor << " and bounds " << bounds.toString());
    }

    // Main module positioning (central hub)
    static LayoutBounds getModuleBounds()
    {
        float moduleWidth = 480.0f * scaleFactor;
        float moduleHeight = 640.0f * scaleFactor;

        return {
            {centerX - moduleWidth/2, centerY - moduleHeight/2},
            moduleWidth,
            moduleHeight,
            scaleFactor
        };
    }

    // Center pitch knob (inside module)
    static LayoutBounds getPitchKnobBounds()
    {
        auto module = getModuleBounds();
        float knobSize = 140.0f * scaleFactor;

        return {
            {module.position.x + 170.0f * scaleFactor, module.position.y + 200.0f * scaleFactor},
            knobSize,
            knobSize,
            scaleFactor
        };
    }

    // Transport controls (play/pause/next/prev)
    static LayoutBounds getTransportBounds()
    {
        auto module = getModuleBounds();

        return {
            {module.position.x + 140.0f * scaleFactor, module.position.y + 480.0f * scaleFactor},
            200.0f * scaleFactor,
            50.0f * scaleFactor,
            scaleFactor
        };
    }

    // Effect knob groups - positioned around the edges
    static LayoutBounds getEffectKnobBounds(EffectType effect)
    {
        float knobGroupSize = 220.0f * scaleFactor;

        switch (effect)
        {
            case Filter:
                return {{20.0f * scaleFactor, 80.0f * scaleFactor}, knobGroupSize, 140.0f * scaleFactor, scaleFactor};
            case Delay:
                return {{20.0f * scaleFactor, 380.0f * scaleFactor}, knobGroupSize, 140.0f * scaleFactor, scaleFactor};
            case Reverb:
                return {{20.0f * scaleFactor, 620.0f * scaleFactor}, knobGroupSize, 140.0f * scaleFactor, scaleFactor};
            case Chorus:
                return {{screenBounds.getWidth() - 240.0f * scaleFactor, 80.0f * scaleFactor}, knobGroupSize, 140.0f * scaleFactor, scaleFactor};
            case Distortion:
                return {{screenBounds.getWidth() - 240.0f * scaleFactor, 380.0f * scaleFactor}, knobGroupSize, 140.0f * scaleFactor, scaleFactor};
            case Phaser:
                return {{screenBounds.getWidth() - 240.0f * scaleFactor, 620.0f * scaleFactor}, knobGroupSize, 140.0f * scaleFactor, scaleFactor};
        }

        return {{0, 0}, knobGroupSize, 140.0f * scaleFactor, scaleFactor};
    }

    // iPhone-specific layout (different approach)
    static LayoutBounds getPhoneLayoutBounds(PhoneLayoutElement element)
    {
        if (!DeviceDetection::isPhone())
            return {{0, 0}, 0, 0, 1.0f};

        float phoneScale = 0.8f;

        switch (element)
        {
            case PhoneMainModule:
                return {{centerX - 200.0f, centerY - 250.0f}, 400.0f, 500.0f, phoneScale};
            case PhoneTransport:
                return {{centerX - 100.0f, centerY + 150.0f}, 200.0f, 50.0f, phoneScale};
            case PhoneEffectsTab:
                // iPhone uses tabbed interface - effects go on separate page
                return {{50.0f, 100.0f}, screenBounds.getWidth() - 100.0f, screenBounds.getHeight() - 200.0f, phoneScale};
        }

        return {{0, 0}, 0, 0, phoneScale};
    }

    // Dynamic positioning based on saved user preferences
    static juce::Point<float> getSavedPosition(const juce::String& elementKey, juce::Point<float> defaultPos)
    {
        // For now, just return defaults since we need proper settings management
        // This can be implemented later with a proper settings system
        DBG("Using default position for " << elementKey << ": " << defaultPos.toString());
        return defaultPos;
    }

    // Save user-customized positions
    static void savePosition(const juce::String& elementKey, juce::Point<float> position)
    {
        // Placeholder for future settings persistence
        DBG("Would save position for " << elementKey << ": " << position.toString());
    }

private:
    inline static juce::Rectangle<int> screenBounds;
    inline static float scaleFactor = 1.0f;
    inline static float centerX = 0.0f;
    inline static float centerY = 0.0f;
};