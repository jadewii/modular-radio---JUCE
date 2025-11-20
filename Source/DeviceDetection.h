#pragma once

#include <JuceHeader.h>

/**
 * Device detection system for iOS/iPadOS adaptive layouts
 * Based on the Swift version's DeviceType system
 */
class DeviceDetection
{
public:
    enum DeviceType
    {
        iPhone16ProMax,
        iPhone16,
        iPad11,          // Air 11" & Pro 11": Perfect layout
        iPadPro13M4,     // Pro 13" M4: 1376×1032
        iPad13,          // Air 13" & Pro 12.9": 1366×1024
        iPadMini,        // Mini: 1133×744
        Mac,
        Unknown
    };

    static DeviceType getCurrentDevice()
    {
        #if JUCE_MAC
        return Mac;
        #elif JUCE_IOS
        auto bounds = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()->userArea;
        auto screenWidth = juce::jmin(bounds.getWidth(), bounds.getHeight());
        auto screenHeight = juce::jmax(bounds.getWidth(), bounds.getHeight());

        DBG("Screen dimensions: " << screenWidth << " × " << screenHeight);

        // iPad detection - check largest screens first
        if (screenWidth >= 1030 && screenHeight >= 1374)
            return iPadPro13M4;  // iPad Pro 13" M4
        else if (screenWidth >= 1020 && screenHeight >= 1360)
            return iPad13;       // iPad Air 13" & Pro 12.9"
        else if (screenWidth >= 830 && screenHeight >= 1190)
            return iPad11;       // iPad Air 11" & Pro 11" (PERFECT LAYOUT!)
        else if (screenWidth >= 740 && screenHeight >= 1130)
            return iPadMini;     // iPad mini
        else if (screenWidth >= 428 && screenHeight >= 926)
            return iPhone16ProMax; // iPhone 16 Pro Max
        else if (screenWidth >= 390 && screenHeight >= 844)
            return iPhone16;     // iPhone 16
        #endif

        return Unknown;
    }

    static juce::String getDeviceString()
    {
        switch (getCurrentDevice())
        {
            case iPhone16ProMax: return "iPhone16ProMax";
            case iPhone16: return "iPhone16";
            case iPad11: return "iPad11";
            case iPadPro13M4: return "iPadPro13M4";
            case iPad13: return "iPad13";
            case iPadMini: return "iPadMini";
            case Mac: return "Mac";
            case Unknown: return "Unknown";
        }
        return "Unknown";
    }

    static juce::String getDeviceWithOrientation()
    {
        auto deviceString = getDeviceString();

        #if JUCE_IOS
        // Only add orientation for iPads
        auto device = getCurrentDevice();
        if (device == iPad11 || device == iPadPro13M4 || device == iPad13 || device == iPadMini)
        {
            auto bounds = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()->userArea;
            bool isLandscape = bounds.getWidth() > bounds.getHeight();
            return deviceString + "_" + (isLandscape ? "landscape" : "portrait");
        }
        #endif

        return deviceString;
    }

    static bool isPhone()
    {
        auto device = getCurrentDevice();
        return device == iPhone16ProMax || device == iPhone16;
    }

    static bool isTablet()
    {
        auto device = getCurrentDevice();
        return device == iPad11 || device == iPadPro13M4 || device == iPad13 || device == iPadMini;
    }

    static float getScaleFactor()
    {
        auto device = getCurrentDevice();
        switch (device)
        {
            case iPad11: return 1.0f;  // Base scale (perfect layout)
            case iPadPro13M4: return 1.34f;  // 34% larger
            case iPad13: return 1.30f;       // 30% larger
            case iPadMini: return 0.85f;     // 15% smaller
            case iPhone16ProMax: return 0.5f; // Much smaller
            case iPhone16: return 0.45f;     // Even smaller
            case Mac: return 1.0f;
            case Unknown: return 1.0f;
        }
        return 1.0f;
    }
};