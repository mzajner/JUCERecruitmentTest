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
