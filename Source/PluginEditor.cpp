#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
ModularRadioAudioProcessorEditor::ModularRadioAudioProcessorEditor (ModularRadioAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    // Add the MainComponent as a child
    if (auto* mainComp = audioProcessor.getMainComponent())
    {
        addAndMakeVisible (mainComp);
        setSize (1400, 900);  // Match MainComponent size
    }
}

ModularRadioAudioProcessorEditor::~ModularRadioAudioProcessorEditor()
{
}

//==============================================================================
void ModularRadioAudioProcessorEditor::paint (juce::Graphics& g)
{
    // MainComponent handles all painting
}

void ModularRadioAudioProcessorEditor::resized()
{
    // Make MainComponent fill the entire editor
    if (auto* mainComp = audioProcessor.getMainComponent())
        mainComp->setBounds (getLocalBounds());
}
