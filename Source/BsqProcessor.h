#pragma once

#include <JuceHeader.h>

class BsqProcessor : public juce::AudioProcessor,
                     private juce::ValueTree::Listener {
public:
  class TraceBuffer {
  public:
    struct Column {
      float signal, threshold;
    };

    Column getDisplayColumn(int x, int width) const;
    void store(const Column &c, int atSample);
    void sync();

  private:
    static constexpr int samplesPerColumn = 2;
    static constexpr int numColumns = 1024;
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
  float tuningFeedback;

private:
  void attachToState();
  void updateFromState();
  void valueTreePropertyChanged(juce::ValueTree &,
                                const juce::Identifier &) override;
  void processEdges(juce::MidiBuffer &midi, const float *samples,
                    int numSamples);
  void edgeDetected(juce::MidiBuffer &midi, int atSample, int period);
  void signalLost(juce::MidiBuffer &midi, int atSample);

  class EdgeDetector {
  public:
    bool next(int newState, int &period);
    int getState();
    int getTimer();

  private:
    int timers[2]{0, 0};
    int state{0};
  };

  struct PitchFilter {
  public:
    void resize(int);
    void next(int);
    float getAverage();
    bool isFull();
    void clear();

  private:
    std::mutex mutex;
    std::vector<int> buffer;
    int counter{0};
    int total{0};
  };

  double currentSampleRate{0};
  int currentNote{-1};
  int currentWheelPos{-1};
  int midiChannel{1};
  float gain{1}, pitchBendRange{1}, triggerLevel{0}, triggerHysteresis{0};
  juce::IIRFilter highPassFilter, lowPassFilter;
  PitchFilter pitchFilter;
  EdgeDetector edgeDetector;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BsqProcessor)
};
