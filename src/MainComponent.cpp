#include "MainComponent.h"
#include "BinaryData.h"

namespace
{
    constexpr int parameterCount = 4;
    constexpr auto minParameterValue = 0;
    constexpr auto maxParameterValue = 100;
    constexpr auto requestTimeoutMs = 1500;
    constexpr auto buttonGroupId = 1001;


    juce::String parameterUrl (int index)
    {
        return "http://localhost:8080/parameter/" + juce::String (index);
    }

    bool readParameterValue (const juce::String& response, int& value)
    {
        auto parsed = juce::JSON::parse (response);

        if (auto* object = parsed.getDynamicObject())
        {
            value = static_cast<int> (object->getProperty ("value"));
            value = juce::jlimit (minParameterValue, maxParameterValue, value);
            return true;
        }

        return false
    }

    bool getRemoteParameter (int index, int& value)
    {
        auto stream = juce::URL (parameterUrl (index)).createInputStream (
            juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                .withConnectionTimeoutMs (requestTimeoutMs));
        
        return stream != nullptr && readParameterValue (stream-> readEntireStreamAsString(), value);
    }

    bool putRemoteParameter (int index, int value)
    {
        const auto body = "{\"value\":" + juce::String (value) + "}";
        auto stream = juce::URL (parameterUrl (index))
            .withPOSTData (body)
            .createInputStream (
                juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                .withExtraHeaders ("Content-Type: application/json")
                .withConnectionTimeoutMs (requestTimeoutMs)
                .withHttpRequestCmd ("PUT"));
    }
}


class MainComponent::FilmstripKnobLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    FilmstripKnobLookAndFeel()
        : knobImage (juce::ImageFileFormat::loadFrom (AmpMiddleKnob_png,
                                                      AmpMiddleKnob_pngSize))
    {
    }

    void drawRotarySlider (juce::Graphics& g,
                           int x,
                           int y,
                           int width,
                           int height,
                           float sliderPosProportional,
                           float,
                           float,
                           juce::Slider&) override
    {
        if (! knobImage.isValid())
            return;

        const auto frameSize = static_cast<int> (AmpMiddleKnob_width);
        const auto frameCount = static_cast<int> (AmpMiddleKnob_nPictures);
        const auto frame = juce::jlimit (0,
                                         frameCount - 1,
                                         static_cast<int> (std::round (sliderPosProportional
                                                                      * static_cast<float> (frameCount - 1))));
        const auto size = juce::jmin (width, height);
        const auto destX = x + (width - size) / 2;
        const auto destY = y + (height - size) / 2;

        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (knobImage,
                     destX,
                     destY,
                     size,
                     size,
                     0,
                     frame * frameSize,
                     frameSize,
                     frameSize);
    }

private:
    juce::Image knobImage;
};

MainComponent::MainComponent()
{
    knobLookAndFeel = std::make_unique<FilmstripKnobLookAndFeel>();

    for (int i = 0; i <parameterCount; ++i)
    setSize (600, 400);
}

//==============================================================================
void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    g.setFont (juce::FontOptions (16.0f));
    g.setColour (juce::Colours::white);
    g.drawText ("Hello World!", getLocalBounds(), juce::Justification::centred, true);
}

void MainComponent::resized()
{
}
