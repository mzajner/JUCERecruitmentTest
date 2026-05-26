#pragma once
#include <array>
#include <atomic>
#include <memory>
#include "BinaryData.h"
#include <juce_gui_extra/juce_gui_extra.h>

class MainComponent final : public juce::Component,
                            private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    class FilmstripKnobLookAndFeel;
    
    void timerCallback() override;
    void sendParameterValue (int index, int value);

    std::array<juce::Slider, 4> paramSliders;
    std::array<juce::Label, 4> paramLabels;
    juce::Label statusLabel;

    std::unique_ptr<FilmstripKnobLookAndFeel> knobLookAndFeel;
    std::atomic<bool> isPolling{ false };
    std::arrat<bool, 4> isDraggingSlider {};
    std::array<bool, 4> isWritePending {};
    std::array<int, 4> parameterRevision {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
