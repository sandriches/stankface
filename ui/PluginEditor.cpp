#include "PluginEditor.h"

#include "stankface/Params.h"
#include "stankface/Presets.h"

using namespace stankface;

namespace {

// One section per row, so the width follows whichever section has the most
// controls. Read off the descriptor table rather than hardcoded, so it stays
// right as parameters are added.
constexpr int kCellWidth = 128;
constexpr int kCellHeight = 96;
constexpr int kHeadingHeight = 18;
constexpr int kGroupGap = 6;
constexpr int kMargin = 12;
constexpr int kTitleHeight = 28;
constexpr int kPresetBoxWidth = 180;
constexpr int kPresetButtonWidth = 24;
constexpr int kPresetControlHeight = 22;
constexpr int kDisplayHeight = 210;
constexpr int kDisplayGap = 10;

// One octave, C2 up to C3 inclusive, which is the register this instrument is
// played in. Inclusive of the top C so it reads as a whole octave.
constexpr int kLowestKey = 36;
constexpr int kHighestKey = 48;
constexpr int kKeyboardHeight = 76;
constexpr int kKeyboardGap = 10;

/** White keys in a note range.

    Counted rather than hardcoded, so that changing the range above does not
    silently leave the keys the wrong width for the panel. */
int whiteKeyCount(int lowestNote, int highestNote)
{
    static const bool black[12] = { false, true, false, true, false, false,
                                    true, false, true, false, true, false };

    int count = 0;
    for (int note = lowestNote; note <= highestNote; ++note)
        if (!black[note % 12])
            ++count;

    return count;
}

} // namespace

StankfaceAudioProcessorEditor::StankfaceAudioProcessorEditor(
    StankfaceAudioProcessor& owner)
    : AudioProcessorEditor(&owner), processor_(owner), display_(owner),
      keyboard_(owner.keyboardState(),
                juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel(&lookAndFeel_);
    addAndMakeVisible(content_);

    keyboard_.setAvailableRange(kLowestKey, kHighestKey);
    keyboard_.setScrollButtonsVisible(false);
    keyboard_.setVelocity(1.0f, false);

    // Flat blocks separated by the background colour and no shadow, so the
    // keyboard sits in the panel rather than on top of it. A held key takes the
    // accent, which is the same signal the rotaries use. Set here rather than in
    // the look and feel, which would otherwise need the audio modules just for
    // these colour IDs.
    keyboard_.setColour(juce::MidiKeyboardComponent::whiteNoteColourId,
                        juce::Colour(0xffc9d0d9));
    keyboard_.setColour(juce::MidiKeyboardComponent::blackNoteColourId,
                        juce::Colour(0xff1a1e26));
    keyboard_.setColour(juce::MidiKeyboardComponent::keySeparatorLineColourId,
                        stankface_ui::colours::background);
    keyboard_.setColour(juce::MidiKeyboardComponent::shadowColourId,
                        juce::Colours::transparentBlack);
    keyboard_.setColour(juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId,
                        stankface_ui::colours::accent.withAlpha(0.35f));
    keyboard_.setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId,
                        stankface_ui::colours::accent.withAlpha(0.85f));
    keyboard_.setColour(juce::MidiKeyboardComponent::textLabelColourId,
                        juce::Colour(0xff4a525c));

    content_.addAndMakeVisible(keyboard_);

    title_.setText("STANKFACE", juce::dontSendNotification);
    title_.setFont(juce::FontOptions(17.0f, juce::Font::bold));
    title_.setJustificationType(juce::Justification::centredLeft);
    title_.setColour(juce::Label::textColourId, stankface_ui::colours::text);
    content_.addAndMakeVisible(title_);

    content_.addAndMakeVisible(display_);

    for (int i = 0; i < kNumPresets; ++i)
        presetBox_.addItem(presetName(i), i + 1);

    presetBox_.setSelectedId(processor_.getCurrentProgram() + 1,
                             juce::dontSendNotification);
    presetBox_.onChange = [this] {
        const int index = presetBox_.getSelectedId() - 1;
        if (index >= 0 && index != processor_.getCurrentProgram())
            processor_.setCurrentProgram(index);
    };
    content_.addAndMakeVisible(presetBox_);

    previousPreset_.onClick = [this] { stepPreset(-1); };
    nextPreset_.onClick = [this] { stepPreset(1); };
    content_.addAndMakeVisible(previousPreset_);
    content_.addAndMakeVisible(nextPreset_);

    processor_.addChangeListener(this);

    // Sections first, in the declared order, so that the panel reads in signal
    // order rather than in the order parameters happened to be added.
    for (int i = 0; i < kNumParamGroups; ++i)
        groupFor(kParamGroupOrder[i]);

    juce::AudioProcessorValueTreeState& state = processor_.parameters();

    for (int i = 0; i < kNumParams; ++i)
    {
        const ParamId id = static_cast<ParamId>(i);
        const ParamDescriptor& descriptor = paramDescriptor(id);

        auto control = std::make_unique<Control>();

        control->label.setText(descriptor.name, juce::dontSendNotification);
        control->label.setJustificationType(juce::Justification::centred);
        control->label.setFont(juce::FontOptions(12.0f));
        content_.addAndMakeVisible(control->label);

        if (isChoiceParameter(id))
        {
            control->comboBox = std::make_unique<juce::ComboBox>();

            if (auto* choice = dynamic_cast<juce::AudioParameterChoice*>(
                    state.getParameter(descriptor.id)))
            {
                control->comboBox->addItemList(choice->choices, 1);
            }

            content_.addAndMakeVisible(*control->comboBox);
            control->comboAttachment =
                std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
                    state, descriptor.id, *control->comboBox);
        }
        else
        {
            control->slider = std::make_unique<juce::Slider>(
                juce::Slider::RotaryHorizontalVerticalDrag,
                juce::Slider::TextBoxBelow);

            // A wide sweep with a gap at the bottom, so the pointer's travel
            // reads at a glance instead of nearly closing the circle.
            control->slider->setRotaryParameters(
                juce::MathConstants<float>::pi * 1.25f,
                juce::MathConstants<float>::pi * 2.75f,
                true);

            control->slider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 82, 16);

            // Both readouts go through the same formatter the host uses, so a
            // value never reads one way here and another in an automation lane.
            control->slider->textFromValueFunction = [id](double value) {
                return formatParamValue(id, static_cast<float>(value));
            };
            control->slider->valueFromTextFunction = [id](const juce::String& text) {
                return static_cast<double>(parseParamValue(id, text));
            };

            if (descriptor.minValue < 0.0f)
                control->slider->getProperties().set(
                    stankface_ui::kBipolarProperty, true);

            content_.addAndMakeVisible(*control->slider);
            control->sliderAttachment =
                std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                    state, descriptor.id, *control->slider);

            control->slider->updateText();
        }

        groupFor(descriptor.group).controls.push_back(control.get());
        controls_.push_back(std::move(control));
    }

    int widestGroup = 1;
    for (const std::unique_ptr<Group>& group : groups_)
        widestGroup = juce::jmax(widestGroup, static_cast<int>(group->controls.size()));

    const int groupHeight = kHeadingHeight + kCellHeight + kGroupGap;

    logicalWidth_ = widestGroup * kCellWidth + 2 * kMargin;
    logicalHeight_ = static_cast<int>(groups_.size()) * groupHeight
                   + kTitleHeight + kDisplayHeight + kDisplayGap
                   + kKeyboardHeight + kKeyboardGap + 2 * kMargin;

    content_.setSize(logicalWidth_, logicalHeight_);
    layoutContent();

    setResizable(true, true);
    setResizeLimits(logicalWidth_ * 6 / 10, logicalHeight_ * 6 / 10,
                    logicalWidth_ * 3 / 2, logicalHeight_ * 3 / 2);

    // Locked, because the contents are scaled rather than reflowed.
    if (juce::ComponentBoundsConstrainer* constrainer = getConstrainer())
        constrainer->setFixedAspectRatio(static_cast<double>(logicalWidth_)
                                         / static_cast<double>(logicalHeight_));

    // Open small enough to fit the screen. At full size the panel is taller
    // than a laptop display, and a standalone window that opens already cut off
    // cannot be dragged back into range.
    double scale = 1.0;
    if (const juce::Displays::Display* display =
            juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
    {
        const int available = display->userBounds.getHeight() - 100;
        if (available > 0 && available < logicalHeight_)
            scale = static_cast<double>(available) / logicalHeight_;
    }

    scale = juce::jlimit(0.6, 1.0, scale);

    setSize(juce::roundToInt(logicalWidth_ * scale),
            juce::roundToInt(logicalHeight_ * scale));
}

StankfaceAudioProcessorEditor::Group&
StankfaceAudioProcessorEditor::groupFor(const juce::String& name)
{
    for (const std::unique_ptr<Group>& group : groups_)
        if (group->name == name)
            return *group;

    auto group = std::make_unique<Group>();
    group->name = name;
    group->heading.setText(name.toUpperCase(), juce::dontSendNotification);
    group->heading.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    group->heading.setJustificationType(juce::Justification::centredLeft);
    group->heading.setColour(juce::Label::textColourId,
                             stankface_ui::colours::accent);
    content_.addAndMakeVisible(group->heading);

    groups_.push_back(std::move(group));
    return *groups_.back();
}

void StankfaceAudioProcessorEditor::stepPreset(int delta)
{
    const int count = processor_.getNumPrograms();
    const int index = ((processor_.getCurrentProgram() + delta) % count + count) % count;

    processor_.setCurrentProgram(index);
}

void StankfaceAudioProcessorEditor::changeListenerCallback(juce::ChangeBroadcaster*)
{
    presetBox_.setSelectedId(processor_.getCurrentProgram() + 1,
                             juce::dontSendNotification);
}

StankfaceAudioProcessorEditor::~StankfaceAudioProcessorEditor()
{
    processor_.removeChangeListener(this);

    // Detach before the look and feel goes out of scope; JUCE asserts on a
    // component still pointing at a destroyed one.
    setLookAndFeel(nullptr);
}

void StankfaceAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(stankface_ui::colours::background);
}

void StankfaceAudioProcessorEditor::layoutContent()
{
    juce::Rectangle<int> area = content_.getLocalBounds().reduced(kMargin);
    juce::Rectangle<int> titleRow = area.removeFromTop(kTitleHeight);

    // Presets on the right of the title row: previous, list, next.
    const int presetY = titleRow.getCentreY() - kPresetControlHeight / 2;
    juce::Rectangle<int> presets = titleRow.removeFromRight(
        kPresetBoxWidth + 2 * (kPresetButtonWidth + 4));
    presets = presets.withY(presetY).withHeight(kPresetControlHeight);

    nextPreset_.setBounds(presets.removeFromRight(kPresetButtonWidth));
    presets.removeFromRight(4);
    previousPreset_.setBounds(presets.removeFromLeft(kPresetButtonWidth));
    presets.removeFromLeft(4);
    presetBox_.setBounds(presets);

    title_.setBounds(titleRow);

    display_.setBounds(area.removeFromTop(kDisplayHeight));
    area.removeFromTop(kDisplayGap);

    // Taken off the bottom before the sections are laid out, so the keyboard
    // stays pinned there whatever the sections do.
    juce::Rectangle<int> keys = area.removeFromBottom(kKeyboardHeight);
    area.removeFromBottom(kKeyboardGap);

    // Sized so the octave spans the full width exactly.
    keyboard_.setKeyWidth(static_cast<float>(keys.getWidth())
                          / static_cast<float>(whiteKeyCount(kLowestKey, kHighestKey)));
    keyboard_.setBounds(keys);

    for (const std::unique_ptr<Group>& group : groups_)
    {
        juce::Rectangle<int> row = area.removeFromTop(kHeadingHeight + kCellHeight);
        group->heading.setBounds(row.removeFromTop(kHeadingHeight).withTrimmedLeft(4));

        for (Control* control : group->controls)
        {
            juce::Rectangle<int> cell = row.removeFromLeft(kCellWidth).reduced(4, 2);
            control->label.setBounds(cell.removeFromTop(15));

            if (control->comboBox != nullptr)
                control->comboBox->setBounds(
                    cell.removeFromTop(24).reduced(6, 0));
            else
                control->slider->setBounds(cell);
        }

        area.removeFromTop(kGroupGap);
    }
}

void StankfaceAudioProcessorEditor::resized()
{
    const float scale = static_cast<float>(getWidth())
                      / static_cast<float>(juce::jmax(1, logicalWidth_));

    content_.setTransform(juce::AffineTransform::scale(scale));
    content_.setBounds(0, 0, logicalWidth_, logicalHeight_);
}
