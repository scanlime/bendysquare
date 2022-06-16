#include "BsqEditor.h"
#include "BsqProcessor.h"

BsqAudioProcessor::BsqAudioProcessor()
    : AudioProcessor(BusesProperties().withInput(
          "Input", juce::AudioChannelSet::mono(), true)) {}

BsqAudioProcessor::~BsqAudioProcessor() {}

const juce::String BsqAudioProcessor::getName() const {
  return JucePlugin_Name;
}

bool BsqAudioProcessor::acceptsMidi() const { return false; }

bool BsqAudioProcessor::producesMidi() const { return true; }

bool BsqAudioProcessor::isMidiEffect() const { return false; }

double BsqAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int BsqAudioProcessor::getNumPrograms() { return 1; }

int BsqAudioProcessor::getCurrentProgram() { return 0; }

void BsqAudioProcessor::setCurrentProgram(int index) {}

const juce::String BsqAudioProcessor::getProgramName(int index) { return {}; }

void BsqAudioProcessor::changeProgramName(int index,
                                          const juce::String &newName) {}

void BsqAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {}

void BsqAudioProcessor::releaseResources() {}

void BsqAudioProcessor::processBlock(juce::AudioBuffer<float> &buffer,
                                     juce::MidiBuffer &midiMessages) {}

bool BsqAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor *BsqAudioProcessor::createEditor() {
  return new BsqAudioProcessorEditor(*this);
}

void BsqAudioProcessor::getStateInformation(juce::MemoryBlock &destData) {}

void BsqAudioProcessor::setStateInformation(const void *data, int sizeInBytes) {
}

juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter() {
  return new BsqAudioProcessor();
}
