#include "BsqEditor.h"
#include "BsqProcessor.h"

BsqAudioProcessorEditor::BsqAudioProcessorEditor(BsqAudioProcessor &p)
    : AudioProcessorEditor(&p), audioProcessor(p) {
  setSize(500, 200);
}

BsqAudioProcessorEditor::~BsqAudioProcessorEditor() {}

void BsqAudioProcessorEditor::paint(juce::Graphics &g) {
  g.fillAll(
      getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void BsqAudioProcessorEditor::resized() {}
