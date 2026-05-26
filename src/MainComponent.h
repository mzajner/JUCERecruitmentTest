#pragma once

#include <array>
#include <atomic>
#include <memory>
#include <juce_gui_extra/juce_gui_extra.h>

class MainComponent final : public juce::Component
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;


private:

    class FilmstripKnobLookAndFeel;

    void selectParameter (int index);
    void loadRemoteParameters();
    void sendParameterValue (int index, int value);
    void updateDisplayParameter();

    std::array<juce::TextButton, 4> parameterButtons;
    juce::Slider parameterSlider;
    juce::Label parameterNameLabel;
    juce::Label parameterValueLabel;
    juce::Label statusLabel;

    std::unique_ptr<FilmstripKnobLookAndFeel> knobLookAndFeel;

    std::array<int, 4> parameterValues {};
    std::array<int, 4> parameterRevisions {};
    std::array<bool, 4> pendingWrites {};

    int selectedParameter = 0;
    bool suppressSliderCallback = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
