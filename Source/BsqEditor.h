#pragma once

#include "BsqProcessor.h"
#include <JuceHeader.h>

class BsqEditor : public juce::AudioProcessorEditor {
public:
  BsqEditor(BsqProcessor &);
  ~BsqEditor() override;

  void paint(juce::Graphics &) override;
  void resized() override;

private:
  struct Parts;
  std::unique_ptr<Parts> parts;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BsqEditor)
};
