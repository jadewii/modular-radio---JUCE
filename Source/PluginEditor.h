#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
class ModularRadioAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    ModularRadioAudioProcessorEditor (ModularRadioAudioProcessor&);
    ~ModularRadioAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    ModularRadioAudioProcessor& audioProcessor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModularRadioAudioProcessorEditor)
};
