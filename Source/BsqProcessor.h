#pragma once

#include <JuceHeader.h>

class BsqProcessor : public juce::AudioProcessor,
                     private juce::ValueTree::Listener {
public:
  struct TraceBuffer {
    static constexpr int samplesPerColumn = 2;
    static constexpr int numColumns = 1024;

    struct Column {
      float signal, threshold;
    };

    Column buffer[numColumns];
    int writeColumn, syncColumn;
  };

  BsqProcessor();
  ~BsqProcessor() override;

  void prepareToPlay(double sampleRate, int samplesPerBlock) override;
  void releaseResources() override;

  void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;

  juce::AudioProcessorEditor *createEditor() override;
  bool hasEditor() const override;

  const juce::String getName() const override;

  bool acceptsMidi() const override;
  bool producesMidi() const override;
  bool isMidiEffect() const override;
  double getTailLengthSeconds() const override;

  int getNumPrograms() override;
  int getCurrentProgram() override;
  void setCurrentProgram(int index) override;
  const juce::String getProgramName(int index) override;
  void changeProgramName(int index, const juce::String &newName) override;

  void getStateInformation(juce::MemoryBlock &destData) override;
  void setStateInformation(const void *data, int sizeInBytes) override;

  juce::AudioProcessorValueTreeState state;
  juce::MidiKeyboardState midiState;
  TraceBuffer trace;

private:
  void attachToState();
  void updateFromState();
  void valueTreePropertyChanged(juce::ValueTree &,
                                const juce::Identifier &) override;
  void processEdges(juce::MidiBuffer &midi, const float *samples,
                    int numSamples);
  void updatePitch(juce::MidiBuffer &midi, int atSample, float hz);
  void signalLost(juce::MidiBuffer &midi, int atSample);

  struct EdgeDetector {
    struct Edge {
      int timer{0}, period{0};
    };
    Edge edges[2];
    int state{0};
  };

  double currentSampleRate{0};
  int currentNote{-1};
  int currentWheelPos{-1};
  int midiChannel{1};
  float gain{1};
  juce::IIRFilter highPassFilter, lowPassFilter;
  EdgeDetector detector;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BsqProcessor)
};
