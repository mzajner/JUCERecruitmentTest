#pragma once

#include <array>
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
    void updateDisplayedParameter();

    std::array<juce::TextButton, 4> parameterButtons;
    juce::Slider parameterSlider;
    juce::Label parameterNameLabel;
    juce::Label parameterValueLabel;
    juce::Label statusLabel;

    std::unique_ptr<FilmstripKnobLookAndFeel> knobLookAndFeel;

    std::array<int, 4> parameterValues {};

    int selectedParameter = 0;
    bool suppressSliderCallback = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
