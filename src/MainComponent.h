#pragma once
#include <array>
#include <memory>
#include "BinaryData.h"
#include <juce_gui_extra/juce_gui_extra.h>

class MainComponent final : public juce::Component
{
public:
    MainComponent();

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    class FilmstripKnobLookAndFeel;
    
    std::array<juce::Slider, 4> paramSliders;
    std::array<juce::Label, 4> paramLabels;

    juce::Label statusLabel;

    std::unique_ptr<FilmstripKnobLookAndFeel> knobLookAndFeel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
