#include "BsqProcessor.h"
#include "BsqEditor.h"

BsqProcessor::BsqProcessor()
    : AudioProcessor(BusesProperties().withInput(
          "Input", juce::AudioChannelSet::mono(), true)),
      state(*this, nullptr, "state",
            {
                std::make_unique<juce::AudioParameterFloat>(
                    "pitch_bend_range", "Pitch Bend",
                    juce::NormalisableRange<float>(1.0f, 64.0f), 12.0f),
                std::make_unique<juce::AudioParameterFloat>(
                    "filter_highpass", "High Pass",
                    juce::NormalisableRange<float>(0.0f, 1000.f), 80.f),
                std::make_unique<juce::AudioParameterFloat>(
                    "filter_lowpass", "Low Pass",
                    juce::NormalisableRange<float>(0.0f, 1000.0f), 800.f),
                std::make_unique<juce::AudioParameterFloat>(
                    "trig_level", "Trigger Level",
                    juce::NormalisableRange<float>(-1.f, 1.f), -0.5f),
                std::make_unique<juce::AudioParameterFloat>(
                    "trig_hysteresis", "Hysteresis",
                    juce::NormalisableRange<float>(0.f, 1.f), 0.2f),
            }) {}

BsqProcessor::~BsqProcessor() {}

bool BsqProcessor::hasEditor() const { return true; }
const juce::String BsqProcessor::getName() const { return JucePlugin_Name; }
bool BsqProcessor::acceptsMidi() const { return false; }
bool BsqProcessor::producesMidi() const { return true; }
bool BsqProcessor::isMidiEffect() const { return false; }
double BsqProcessor::getTailLengthSeconds() const { return 0.0; }
int BsqProcessor::getNumPrograms() { return 1; }
int BsqProcessor::getCurrentProgram() { return 0; }
void BsqProcessor::setCurrentProgram(int index) {}
const juce::String BsqProcessor::getProgramName(int index) { return {}; }
void BsqProcessor::changeProgramName(int index, const juce::String &newName) {}
void BsqProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {}
void BsqProcessor::releaseResources() {}

void BsqProcessor::processBlock(juce::AudioBuffer<float> &buffer,
                                juce::MidiBuffer &midiMessages) {
  auto startSample = 0;
  auto numSamples = buffer.getNumSamples();
  midiState.processNextMidiBuffer(midiMessages, startSample, numSamples, true);
}

juce::AudioProcessorEditor *BsqProcessor::createEditor() {
  return new BsqEditor(*this);
}

void BsqProcessor::getStateInformation(juce::MemoryBlock &destData) {
  if (auto xml = state.copyState().createXml())
    copyXmlToBinary(*xml, destData);
}

void BsqProcessor::setStateInformation(const void *data, int sizeInBytes) {
  if (auto xml = getXmlFromBinary(data, sizeInBytes))
    state.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter() {
  return new BsqProcessor();
}
