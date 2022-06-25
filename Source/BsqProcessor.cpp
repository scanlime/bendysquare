#include "BsqProcessor.h"
#include "BsqEditor.h"

BsqProcessor::BsqProcessor()
    : AudioProcessor(BusesProperties().withInput(
          "Input", juce::AudioChannelSet::mono(), true)),
      state(*this, nullptr, "state",
            {
                std::make_unique<juce::AudioParameterInt>(
                    "midi_ch", "MIDI Channel", 1, 16, 2),
                std::make_unique<juce::AudioParameterInt>(
                    "pitch_filter", "Pitch Filter", 1, 100, 10),
                std::make_unique<juce::AudioParameterFloat>(
                    "pitch_bend_range", "Pitch Bend Range",
                    juce::NormalisableRange<float>(1, 64), 2.0f),
                std::make_unique<juce::AudioParameterFloat>(
                    "filter_highpass", "High Pass Hz",
                    juce::NormalisableRange<float>(0, 500), 20.f),
                std::make_unique<juce::AudioParameterFloat>(
                    "filter_lowpass", "Low Pass Hz",
                    juce::NormalisableRange<float>(0, 5000), 800.f),
                std::make_unique<juce::AudioParameterFloat>(
                    "gain_db", "Gain dB", juce::NormalisableRange<float>(0, 50),
                    10.f),
                std::make_unique<juce::AudioParameterFloat>(
                    "trig_level", "Trigger Level",
                    juce::NormalisableRange<float>(-1, 1), -0.1f),
                std::make_unique<juce::AudioParameterFloat>(
                    "trig_hysteresis", "Hysteresis",
                    juce::NormalisableRange<float>(0, 1), 0.1f),
            }) {
  attachToState();
}

BsqProcessor::~BsqProcessor() {}

BsqProcessor::TraceBuffer::Column
BsqProcessor::TraceBuffer::getDisplayColumn(int x, int width) const {
  return buffer[juce::jmax<int>(0, syncColumn + numColumns - width + x) %
                numColumns];
}

void BsqProcessor::TraceBuffer::store(const Column &c, int atSample) {
  if ((atSample % samplesPerColumn) == 0) {
    auto slot = juce::jlimit(0, TraceBuffer::numColumns - 1, writeColumn);
    buffer[slot] = c;
    writeColumn = (slot + 1) % TraceBuffer::numColumns;
  }
}

void BsqProcessor::TraceBuffer::sync() { syncColumn = writeColumn; }

bool BsqProcessor::EdgeDetector::next(int newState, int &period) {
  jassert(newState == 0 || newState == 1);
  bool edgeDetected = newState != state;
  state = newState;
  for (int edge = 0; edge < 2; edge++) {
    if (edgeDetected && state == edge) {
      period = 1 + timers[edge];
      timers[edge] = 0;
    } else {
      timers[edge]++;
    }
  }
  return edgeDetected;
}

int BsqProcessor::EdgeDetector::getState() { return state; }

int BsqProcessor::EdgeDetector::getTimer() { return timers[state]; }

void BsqProcessor::PitchFilter::resize(int newSize) {
  std::lock_guard<std::mutex> guard(mutex);
  buffer.resize(newSize);
  total = 0;
  for (int i = 0; i < newSize; i++) {
    total += buffer[i];
  }
}

void BsqProcessor::PitchFilter::next(int latestPeriod) {
  std::lock_guard<std::mutex> guard(mutex);
  int slot = ++counter % buffer.size();
  if (counter > buffer.size()) {
    total -= buffer[slot];
  }
  total += buffer[slot] = latestPeriod;
}

float BsqProcessor::PitchFilter::getAverage() {
  std::lock_guard<std::mutex> guard(mutex);
  if (counter >= buffer.size()) {
    return float(total) / float(buffer.size());
  } else if (counter > 0) {
    return float(total) / counter;
  } else {
    return 0.f;
  }
}

bool BsqProcessor::PitchFilter::isFull() {
  std::lock_guard<std::mutex> guard(mutex);
  return counter >= buffer.size();
}

void BsqProcessor::PitchFilter::clear() {
  counter = 0;
  total = 0;
}

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
  EdgeDetector det = edgeDetector;
  for (int sample = 0; sample < numSamples; sample++) {
    int period;
    float signal = gain * samples[sample];
    float threshold = det.getState() ? (triggerLevel - triggerHysteresis)
                                     : (triggerLevel + triggerHysteresis);
    trace.store({signal, threshold}, sample);
    if (det.next(signal > threshold, period)) {
      edgeDetected(midi, sample, period);
      if (det.getState()) {
        trace.sync();
      }
    } else if (pitchFilter.isFull() &&
               det.getTimer() > pitchFilter.getAverage() * 2) {
      signalLost(midi, sample);
    }
  }
  edgeDetector = det;
}

void BsqProcessor::edgeDetected(juce::MidiBuffer &midi, int atSample,
                                int period) {
  pitchFilter.next(period);
  if (!pitchFilter.isFull()) {
    return;
  }
  float hz = currentSampleRate / pitchFilter.getAverage();
  auto note = std::log2(hz / 440.f) * 12.f + 69.f;
  if (note <= 0.f || note >= 127.f) {
    return;
  }

  tuningFeedback = note - std::round(note);

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

  int nextWheelPos = juce::MidiMessage::pitchbendToPitchwheelPos(
      note - currentNote, pitchBendRange);
  if (nextWheelPos != currentWheelPos) {
    currentWheelPos = nextWheelPos;
    midi.addEvent(juce::MidiMessage::pitchWheel(midiChannel, nextWheelPos),
                  atSample);
  }

  if (noteOn) {
    midi.addEvent(juce::MidiMessage::noteOn(midiChannel, currentNote, 1.f),
                  atSample);
  }
}

void BsqProcessor::signalLost(juce::MidiBuffer &midi, int atSample) {
  if (currentNote >= 0) {
    midi.addEvent(juce::MidiMessage::noteOff(midiChannel, currentNote, 0.f),
                  atSample);
  }
  pitchFilter.clear();
  currentNote = -1;
  tuningFeedback = 0;
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
  pitchFilter.resize(state.getParameterAsValue("pitch_filter").getValue());
  triggerHysteresis = state.getParameterAsValue("trig_hysteresis").getValue();
  triggerLevel = state.getParameterAsValue("trig_level").getValue();
  pitchBendRange = state.getParameterAsValue("pitch_bend_range").getValue();
  midiChannel = state.getParameterAsValue("midi_ch").getValue();
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
