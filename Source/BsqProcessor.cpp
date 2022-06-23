#include "BsqProcessor.h"
#include "BsqEditor.h"

BsqProcessor::BsqProcessor()
    : AudioProcessor(BusesProperties().withInput(
          "Input", juce::AudioChannelSet::mono(), true)),
      state(*this, nullptr, "state",
            {
                std::make_unique<juce::AudioParameterFloat>(
                    "pitch_bend_range", "Pitch Bend",
                    juce::NormalisableRange<float>(1, 64), 2.0f),
                std::make_unique<juce::AudioParameterFloat>(
                    "filter_highpass", "High Pass",
                    juce::NormalisableRange<float>(0, 500), 30.f),
                std::make_unique<juce::AudioParameterFloat>(
                    "filter_lowpass", "Low Pass",
                    juce::NormalisableRange<float>(0, 5000), 800.f),
                std::make_unique<juce::AudioParameterFloat>(
                    "gain_db", "Gain dB", juce::NormalisableRange<float>(0, 50),
                    10.f),
                std::make_unique<juce::AudioParameterFloat>(
                    "trig_level", "Trigger Level",
                    juce::NormalisableRange<float>(-1, 1), -0.03f),
                std::make_unique<juce::AudioParameterFloat>(
                    "trig_hysteresis", "Hysteresis",
                    juce::NormalisableRange<float>(0, 1), 0.05f),
            }) {
  attachToState();
}

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
void BsqProcessor::releaseResources() {}

void BsqProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
  currentSampleRate = sampleRate;
  updateFromState();
}

void BsqProcessor::processBlock(juce::AudioBuffer<float> &buffer,
                                juce::MidiBuffer &midiMessages) {
  // Mix down to mono if necessary
  for (int ch = 1; ch < buffer.getNumChannels(); ch++) {
    buffer.addFrom(0, 0, buffer, ch, 0, buffer.getNumSamples());
  }

  highPassFilter.processSamples(buffer.getWritePointer(0),
                                buffer.getNumSamples());
  lowPassFilter.processSamples(buffer.getWritePointer(0),
                               buffer.getNumSamples());

  processEdges(midiMessages, buffer.getReadPointer(0), buffer.getNumSamples());
  midiState.processNextMidiBuffer(midiMessages, 0, buffer.getNumSamples(),
                                  true);
}

void BsqProcessor::processEdges(juce::MidiBuffer &midi, const float *samples,
                                int numSamples) {
  EdgeDetector det = detector;
  float triggerHysteresis =
      state.getParameterAsValue("trig_hysteresis").getValue();
  float triggerLevel = state.getParameterAsValue("trig_level").getValue();

  for (int sample = 0; sample < numSamples; sample++) {
    float signal = gain * samples[sample];
    float threshold = det.state ? (triggerLevel - triggerHysteresis)
                                : (triggerLevel + triggerHysteresis);

    if ((sample % TraceBuffer::samplesPerColumn) == 0) {
      int c = trace.writeColumn;
      trace.buffer[c] = {signal, threshold};
      trace.writeColumn = (c + 1) % TraceBuffer::numColumns;
    }

    int nextState = signal > threshold;
    bool edgeDetected = (nextState != det.state);
    det.state = nextState;

    // Time both rising and falling edges separately
    for (int edge = 0; edge < 2; edge++) {
      if (edgeDetected && det.state == edge) {
        det.edges[edge].period = det.edges[edge].timer;
        det.edges[edge].timer = 0;
      } else {
        det.edges[edge].timer++;
      }
    }

    // Scope trigger
    if (edgeDetected && det.state) {
      trace.syncColumn = trace.writeColumn;
    }

    // Every edge can generate a pitch change event
    if (edgeDetected) {
      updatePitch(midi, sample,
                  currentSampleRate / (1 + det.edges[det.state].period));
    } else if (det.edges[det.state].timer > det.edges[det.state].period * 2) {
      signalLost(midi, sample);
    }
  }

  detector = det;
}

void BsqProcessor::updatePitch(juce::MidiBuffer &midi, int atSample, float hz) {
  float pitchBendRange =
      state.getParameterAsValue("pitch_bend_range").getValue();

  auto note = std::log2(hz / 440.f) * 12.f + 69.f;
  if (note <= 0.f || note >= 127.f) {
    return;
  }

  bool noteOn =
      (currentNote < 0 || std::abs(note - currentNote) >= pitchBendRange);
  bool noteOff = (noteOn && currentNote >= 0);

  if (noteOff) {
    midi.addEvent(juce::MidiMessage::noteOff(midiChannel, currentNote, 0.f),
                  atSample);
  }
  if (noteOn) {
    currentNote = std::round(note);
  }

  midi.addEvent(juce::MidiMessage::pitchWheel(
                    midiChannel, juce::MidiMessage::pitchbendToPitchwheelPos(
                                     note - currentNote, pitchBendRange)),
                atSample);

  if (noteOn) {
    midi.addEvent(juce::MidiMessage::noteOn(midiChannel, currentNote, 1.f),
                  atSample);
  }
}

void BsqProcessor::signalLost(juce::MidiBuffer &midi, int atSample) {
  if (currentNote >= 0) {
    midi.addEvent(juce::MidiMessage::noteOff(midiChannel, currentNote, 0.f),
                  atSample);
    currentNote = -1;
  }
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
  attachToState();
  updateFromState();
}

void BsqProcessor::attachToState() { state.state.addListener(this); }

void BsqProcessor::updateFromState() {
  gain = juce::Decibels::decibelsToGain<float>(
      state.getParameterAsValue("gain_db").getValue());
  highPassFilter.setCoefficients(juce::IIRCoefficients::makeHighPass(
      currentSampleRate,
      state.getParameterAsValue("filter_highpass").getValue()));
  lowPassFilter.setCoefficients(juce::IIRCoefficients::makeLowPass(
      currentSampleRate,
      state.getParameterAsValue("filter_lowpass").getValue()));
}

void BsqProcessor::valueTreePropertyChanged(juce::ValueTree &,
                                            const juce::Identifier &) {
  updateFromState();
}

juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter() {
  return new BsqProcessor();
}
