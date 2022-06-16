#pragma once

#include "BsqProcessor.h"
#include <JuceHeader.h>

class BsqAudioProcessorEditor : public juce::AudioProcessorEditor {
public:
  BsqAudioProcessorEditor(BsqAudioProcessor &);
  ~BsqAudioProcessorEditor() override;

  void paint(juce::Graphics &) override;
  void resized() override;

private:
  BsqAudioProcessor &audioProcessor;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BsqAudioProcessorEditor)
};
