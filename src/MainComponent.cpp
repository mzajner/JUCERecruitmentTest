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

    for (int i = 0; i < parameterCount; ++i)
    {
        auto& button = parameterButtons[static_cast<size_t> (i)];
        button.setButtonText ("Parameter " + juce::String (i + 1));
        button.setClickingTogglesState (true);
        button.setRadioGroupId (buttonGroupId);
        button.onClick = [this, i] { selectParameter (i); };
        addAndMakeVisible (button);
    }

    parameterButtons[0].setToggleState (true, juce::dontSendNotification);

    parameterSlider.setRange (minParameterValue, maxParameterValue, 1.0);
    parameterSlider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    parameterSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    parameterSlider.setLookAndFeel (knobLookAndFeel.get());
    parameterSlider.onValueChange = [this]
    {
        if (suppressSliderCallback)
            return;

        parameterValues[static_cast<size_t> (selectedParameter)] =
            static_cast<int> (std::round (parameterSlider.getValue()));
        updateDisplayedParameter();
    };
    parameterSlider.onDragEnd = [this]
    {
        const auto value = static_cast<int> (std::round (parameterSlider.getValue()));
        parameterValues[static_cast<size_t> (selectedParameter)] = value;
        sendParameterValue (selectedParameter, value);
    };
    addAndMakeVisible (parameterSlider);

    parameterNameLabel.setJustificationType (juce::Justification::centredLeft);
    parameterNameLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (parameterNameLabel);

    parameterValueLabel.setJustificationType (juce::Justification::centred);
    parameterValueLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (parameterValueLabel);

    statusLabel.setJustificationType (juce::Justification::centred);
    statusLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible (statusLabel);

    setSize (640, 480);
    updateDisplayedParameter();
    loadRemoteParameters();
}

MainComponent::~MainComponent()
{
    parameterSlider.setLookAndFeel (nullptr);
}


//==============================================================================
void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::darkgrey);
}

voidvoid MainComponent::selectParameter (int index)
{
    selectedParameter = index;
    updateDisplayedParameter();
}

void MainComponent::resized()
{
        auto area = getLocalBounds().reduced (36);
    auto buttonArea = area.removeFromTop (70);
    const auto buttonWidth = buttonArea.getWidth() / parameterCount;

    for (auto& button : parameterButtons)
        button.setBounds (buttonArea.removeFromLeft (buttonWidth).reduced (6));
    
    statusLabel.setBounds (area.removeFromBottom (30));

    auto knobArea = area.removeFromLeft (area.getWidth() / 2).reduced (24);
    parameterSlider.setBounds (knobArea);   

    auto valueArea = area.reduced (24);
    parameterNameLabel.setBounds (valueArea.removeFromTop (70));
    parameterValueLabel.setBounds (valueArea);
}


void MainComponent::loadRemoteParameters()
{
    statusLabel.setText ("Loading...", juce::dontSendNotification);

    const juce::Component::SafePointer<MainComponent> safeThis (this);

    juce::Thread::launch ([safeThis]
    {
        std::array<int, parameterCount> values {};
        auto ok = true;

        for (int i = 0; i < parameterCount; ++i)
            ok = getRemoteParameter (i, values[static_cast<size_t> (i)]) && ok;

        juce::MessageManager::callAsync ([safeThis, values, ok]
        {
            if (safeThis == nullptr)
                return;

            if (ok)
            {
                safeThis->parameterValues = values;
                safeThis->updateDisplayedParameter();
                safeThis->statusLabel.setText ("Loaded", juce::dontSendNotification);
            }
            else
            {
                safeThis->statusLabel.setText ("Server unavailable", juce::dontSendNotification);
            }
        });
    });
}

void MainComponent::sendParameterValue (int index, int value)
{
    statusLabel.setText ("Updating...", juce::dontSendNotification);

    const juce::Component::SafePointer<MainComponent> safeThis (this);

    juce::Thread::launch ([safeThis, index, value]
    {
        const auto ok = putRemoteParameter (index, value);

        juce::MessageManager::callAsync ([safeThis, index, ok]
        {
            if (safeThis != nullptr)
                safeThis->statusLabel.setText (ok ? "Updated Parameter " + juce::String (index + 1)
                                                  : "Server unavailable",
                                               juce::dontSendNotification);
        });
    });
}

void MainComponent::updateDisplayedParameter()
{
    const auto value = parameterValues[static_cast<size_t> (selectedParameter)];

    parameterNameLabel.setText ("Parameter " + juce::String (selectedParameter + 1) + ":",
                                juce::dontSendNotification);
    parameterNameLabel.setFont (juce::FontOptions (40.0f));

    parameterValueLabel.setText (juce::String (value), juce::dontSendNotification);
    parameterValueLabel.setFont (juce::FontOptions (96.0f));

    suppressSliderCallback = true;
    parameterSlider.setValue (value, juce::dontSendNotification);
    suppressSliderCallback = false;
}
