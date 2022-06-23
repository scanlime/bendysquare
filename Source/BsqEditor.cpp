#include "BsqEditor.h"
#include "BsqProcessor.h"

class ParamKnob : public juce::Component {
public:
  ParamKnob(BsqProcessor &processor, const juce::String &paramId) {
    auto param = processor.state.getParameter(paramId);
    if (param == nullptr) {
      jassertfalse;
    } else {
      slider = std::make_unique<juce::Slider>();
      slider->setSliderStyle(juce::Slider::Rotary);
      slider->setTextBoxStyle(juce::Slider::TextEntryBoxPosition::TextBoxBelow,
                              false, 70, 18);
      addAndMakeVisible(*slider);

      label = std::make_unique<juce::Label>();
      label->setText(param->name, juce::NotificationType::sendNotification);
      label->setJustificationType(juce::Justification::centred);
      addAndMakeVisible(*label);

      attach = std::make_unique<
          juce::AudioProcessorValueTreeState::SliderAttachment>(
          processor.state, paramId, *slider);
    }
  }

  void resized() override {
    juce::FlexBox box;
    box.flexDirection = juce::FlexBox::Direction::column;
    box.justifyContent = juce::FlexBox::JustifyContent::center;
    box.items.add(juce::FlexItem().withMinHeight(2));
    box.items.add(juce::FlexItem(*label).withMinHeight(15));
    box.items.add(juce::FlexItem(*slider).withFlex(1));
    box.items.add(juce::FlexItem().withMinHeight(2));
    box.performLayout(getLocalBounds().toFloat());
  }

private:
  std::unique_ptr<juce::Label> label;
  std::unique_ptr<juce::Slider> slider;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attach;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ParamKnob)
};

class ParamPanel : public juce::Component {
public:
  ParamPanel(BsqProcessor &processor,
             const juce::Array<juce::String> knobOrder) {
    for (auto &paramId : knobOrder) {
      auto knob = new ParamKnob(processor, paramId);
      addAndMakeVisible(*knob);
      knobs.add(knob);
    }
  }

  void resized() override {
    juce::FlexBox row;
    row.justifyContent = juce::FlexBox::JustifyContent::center;
    for (auto item : knobs) {
      row.items.add(juce::FlexItem(*item).withFlex(1).withMaxWidth(100));
    }
    row.performLayout(getLocalBounds().toFloat());
  }

private:
  juce::OwnedArray<ParamKnob> knobs;
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ParamPanel)
};

class WavePanel : public juce::Component, private juce::Timer {
public:
  WavePanel(BsqProcessor &processor) : processor(processor) {
    startTimerHz(20);
  }

  void paint(juce::Graphics &g) override {
    auto &tr = processor.trace;
    juce::Path signal, threshold;

    auto yAxis = [h = getHeight()](float y) { return h * (-y * 0.5f + 0.5f); };

    for (int i = 0; i < juce::jmin<int>(getWidth(), tr.numColumns); i++) {
      auto &column =
          tr.buffer[(tr.syncColumn + tr.numColumns - getWidth() + i) %
                    tr.numColumns];
      if (i == 0) {
        signal.startNewSubPath(0, yAxis(column.signal));
        threshold.startNewSubPath(0, yAxis(column.threshold));
      } else {
        signal.lineTo(i, yAxis(column.signal));
        threshold.lineTo(i, yAxis(column.threshold));
      }
    }

    auto bg =
        getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId);
    g.fillAll(bg);
    g.setColour(bg.contrasting(0.5f));
    g.strokePath(threshold, juce::PathStrokeType(1.2f));
    g.setColour(bg.contrasting(1.f));
    g.strokePath(signal, juce::PathStrokeType(1.6f));
  }

private:
  void timerCallback() override { repaint(); }

  BsqProcessor &processor;
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WavePanel)
};

struct BsqEditor::Parts {
  WavePanel wave;
  juce::OwnedArray<ParamPanel> params;
  juce::MidiKeyboardComponent keyboard;

  Parts(BsqProcessor &p)
      : wave(p), params({
                     new ParamPanel(p,
                                    {
                                        "pitch_bend_range",
                                        "filter_highpass",
                                        "filter_lowpass",
                                        "gain_db",
                                        "trig_level",
                                        "trig_hysteresis",
                                    }),
                 }),
        keyboard(p.midiState, juce::MidiKeyboardComponent::horizontalKeyboard) {
  }
};

BsqEditor::BsqEditor(BsqProcessor &p)
    : AudioProcessorEditor(&p), parts(std::make_unique<Parts>(p)) {

  addAndMakeVisible(parts->wave);
  addAndMakeVisible(parts->keyboard);
  for (auto &part : parts->params) {
    addAndMakeVisible(part);
  }

  parts->keyboard.setKeyPressBaseOctave(4);
  parts->keyboard.setLowestVisibleKey(3 * 12);
  parts->keyboard.setVelocity(0.7, true);

  setSize(450, 220);
}

BsqEditor::~BsqEditor() {}

void BsqEditor::paint(juce::Graphics &g) {
  g.fillAll(
      getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void BsqEditor::resized() {
  juce::FlexBox box;
  box.flexDirection = juce::FlexBox::Direction::column;
  box.items.add(juce::FlexItem(parts->wave).withMinHeight(30).withFlex(1));
  for (auto &part : parts->params) {
    box.items.add(juce::FlexItem(*part).withMinHeight(70).withFlex(1));
  }
  box.items.add(juce::FlexItem(parts->keyboard).withMinHeight(30).withFlex(1));
  box.performLayout(getLocalBounds().toFloat());
}
