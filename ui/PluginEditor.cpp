#include "PluginEditor.h"

#include "stankface/Params.h"

using namespace stankface;

namespace {

constexpr int kColumns = 5;
constexpr int kCellWidth = 132;
constexpr int kCellHeight = 108;
constexpr int kMargin = 12;
constexpr int kTitleHeight = 30;

} // namespace

StankfaceAudioProcessorEditor::StankfaceAudioProcessorEditor(
    StankfaceAudioProcessor& owner)
    : AudioProcessorEditor(&owner), processor_(owner)
{
    title_.setText("Stankface", juce::dontSendNotification);
    title_.setFont(juce::FontOptions(20.0f, juce::Font::bold));
    title_.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(title_);

    juce::AudioProcessorValueTreeState& state = processor_.parameters();

    for (int i = 0; i < kNumParams; ++i)
    {
        const ParamId id = static_cast<ParamId>(i);
        const ParamDescriptor& descriptor = paramDescriptor(id);

        auto control = std::make_unique<Control>();

        control->label.setText(descriptor.name, juce::dontSendNotification);
        control->label.setJustificationType(juce::Justification::centred);
        control->label.setFont(juce::FontOptions(12.0f));
        addAndMakeVisible(control->label);

        if (isChoiceParameter(id))
        {
            control->comboBox = std::make_unique<juce::ComboBox>();

            if (auto* choice = dynamic_cast<juce::AudioParameterChoice*>(
                    state.getParameter(descriptor.id)))
            {
                control->comboBox->addItemList(choice->choices, 1);
            }

            addAndMakeVisible(*control->comboBox);
            control->comboAttachment =
                std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
                    state, descriptor.id, *control->comboBox);
        }
        else
        {
            control->slider = std::make_unique<juce::Slider>(
                juce::Slider::RotaryHorizontalVerticalDrag,
                juce::Slider::TextBoxBelow);
            control->slider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 78, 18);
            control->slider->setTextValueSuffix(
                juce::String(descriptor.unit).isEmpty()
                    ? juce::String()
                    : " " + juce::String(descriptor.unit));

            addAndMakeVisible(*control->slider);
            control->sliderAttachment =
                std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                    state, descriptor.id, *control->slider);
        }

        controls_.push_back(std::move(control));
    }

    const int rows = (kNumParams + kColumns - 1) / kColumns;
    setSize(kColumns * kCellWidth + 2 * kMargin,
            rows * kCellHeight + kTitleHeight + 2 * kMargin);
}

void StankfaceAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void StankfaceAudioProcessorEditor::resized()
{
    juce::Rectangle<int> area = getLocalBounds().reduced(kMargin);
    title_.setBounds(area.removeFromTop(kTitleHeight));

    for (std::size_t i = 0; i < controls_.size(); ++i)
    {
        const int column = static_cast<int>(i) % kColumns;
        const int row = static_cast<int>(i) / kColumns;

        juce::Rectangle<int> cell(area.getX() + column * kCellWidth,
                                  area.getY() + row * kCellHeight,
                                  kCellWidth,
                                  kCellHeight);
        cell.reduce(4, 4);

        Control& control = *controls_[i];
        control.label.setBounds(cell.removeFromTop(16));

        if (control.comboBox != nullptr)
            control.comboBox->setBounds(cell.removeFromTop(24).reduced(4, 0));
        else
            control.slider->setBounds(cell);
    }
}
