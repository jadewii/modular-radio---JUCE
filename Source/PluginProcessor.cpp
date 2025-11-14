#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
ModularRadioAudioProcessor::ModularRadioAudioProcessor()
     : AudioProcessor (BusesProperties()
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    mainComponent = std::make_unique<MainComponent>();
}

ModularRadioAudioProcessor::~ModularRadioAudioProcessor()
{
}

//==============================================================================
void ModularRadioAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    if (mainComponent != nullptr)
        mainComponent->prepareToPlay (samplesPerBlock, sampleRate);
}

void ModularRadioAudioProcessor::releaseResources()
{
    if (mainComponent != nullptr)
        mainComponent->releaseResources();
}

bool ModularRadioAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Only stereo supported
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // Input and output layout must match
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    return true;
}

void ModularRadioAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (mainComponent != nullptr)
    {
        juce::AudioSourceChannelInfo info;
        info.buffer = &buffer;
        info.startSample = 0;
        info.numSamples = buffer.getNumSamples();

        mainComponent->getNextAudioBlock (info);
    }
}

//==============================================================================
juce::AudioProcessorEditor* ModularRadioAudioProcessor::createEditor()
{
    return new ModularRadioAudioProcessorEditor (*this);
}

//==============================================================================
void ModularRadioAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // Save state if needed
}

void ModularRadioAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // Restore state if needed
}

//==============================================================================
// This creates new instances of the plugin
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ModularRadioAudioProcessor();
}
