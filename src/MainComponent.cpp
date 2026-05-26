#include "MainComponent.h"

class MainComponent::FilmstripKnobLookAndFeel final : public juce::LookAndFeel_V4
{
public:
   FilmstripKnobLookAndFeel()
        : knobImage (juce::ImageFileFormat::loadFrom (AmpMiddleKnob_png, AmpMiddleKnob_pngSize ))
        {
        }

        void drawRotarySlider (juce::Graphics& g,
                                int x, 
                                int y, 
                                int width, 
                                int height,
                                float sliderPosProportional, 
                                float rotaryStartAngle, 
                                float rotaryEndAngle,
                                juce::Slider& slider) override
        {
            if (knobImage.isNull())
            {
                juce::LookAndFeel_V4::drawRotarySlider (g, x, y, width, height, sliderPosProportional, rotaryStartAngle, rotaryEndAngle, slider);
                return;
            }

            const auto frame = juce::jlimit (0, static_cast<int> (AmpMiddleKnob_nPictures) - 1,
                                        juce::roundToInt (sliderPosProportional * static_cast<float>
                                        (AmpMiddleKnob_nPictures - 1)));
            const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat()
                                            .withSizeKeepingCentre (static_cast<float> (juce::jmin (width, height)),
                                                                    static_cast<float> (juce::jmin (width, height))).toNearestInt();
                            
    

            g.drawImage (knobImage, 
                        bounds.getX(), 
                        bounds.getY(), 
                        bounds.getWidth(), 
                        bounds.getHeight(),
                        0, 
                        frame * static_cast<int> (AmpMiddleKnob_width), 
                        static_cast<int> (AmpMiddleKnob_width), 
                        static_cast<int> (AmpMiddleKnob_width));
        }
private: 
    juce::Image knobImage;
};

MainComponent::MainComponent()
{
    setSize (600, 400);

    knobLookAndFeel = std::make_unique<FilmstripKnobLookAndFeel>();

    for (int i = 0; i < paramSliders.size(); ++i)
    {
        auto& label = paramLabels[(size_t) i];
        label.setText ("Param " + juce::String (i + 1), juce::NotificationType::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (label);

        auto& slider = paramSliders[(size_t) i];
        slider.setRange (0.0, 100.0, 1.0);
        slider.setSliderStyle (juce::Slider::SliderStyle::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 20);
        slider.setLookAndFeel (knobLookAndFeel.get());
        addAndMakeVisible (slider);
    }
}

//==============================================================================
void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced (24);
    auto statusArea = area.removeFromBottom (24);
    auto labelHeight = 24;
    auto columnWidth = area.getWidth() / (int) paramSliders.size();

    for (int i = 0; i < 4; ++i)
    {
        auto column = area.removeFromLeft (columnWidth).reduced (8);
        paramLabels[(size_t) i].setBounds (column.removeFromTop (labelHeight));
        paramSliders[(size_t) i].setBounds (column);
    }

    statusLabel.setBounds (statusArea);
}
