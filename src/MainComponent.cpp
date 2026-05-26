#include "MainComponent.h"

#include "BinaryData.h"

namespace
{
constexpr auto parameterCount = 4;
constexpr auto minParameterValue = 0;
constexpr auto maxParameterValue = 100;
constexpr auto requestTimeoutMs = 1500;
constexpr auto buttonGroupId = 1001;

juce::String parameterUrl (int index)
{
    return "http://localhost:8000/parameters/" + juce::String (index);
}

// Read JSON response like {"value": 42} and extract the value, returning true on success
// Parses server response, reads value and clamps it to assigned range
// Production app would be more robust: validate the index, response type, status code, ettor body, etc.
bool readParameterValue (const juce::String& response, int& value)
{
    auto parsed = juce::JSON::parse (response);

    if (auto* object = parsed.getDynamicObject())
    {
        value = static_cast<int> (object->getProperty ("value"));
        value = juce::jlimit (minParameterValue, maxParameterValue, value);
        return true;
    }

    return false;
}

// GET helper
// performs GET request, patses the response to integer value, returns
bool getRemoteParameter (int index, int& value)
{
    auto stream = juce::URL (parameterUrl (index)).createInputStream (
        juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
            .withConnectionTimeoutMs (requestTimeoutMs));

    return stream != nullptr && readParameterValue (stream->readEntireStreamAsString(), value);
}

// PUT helper
// Builds a JSON body  {"value": 42}
// Sends the request with 'Content-Type: application/json'
// Times out so the app doesn't wait forever. 

bool putRemoteParameter (int index, int value)
{
    const auto body = "{\"value\":" + juce::String (juce::jlimit (minParameterValue,
                                                                  maxParameterValue,
                                                                  value))
                      + "}";

    auto stream = juce::URL (parameterUrl (index))
                      .withPOSTData (body)
                      .createInputStream (
                          juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                              .withExtraHeaders ("Content-Type: application/json")
                              .withConnectionTimeoutMs (requestTimeoutMs)
                              .withHttpRequestCmd ("PUT"));

    return stream != nullptr;
}
}

// LAF
// Load image from compiled memory
// maps slider to a filmstip frame (129 frames, 0-128), then draws slices.
// Move down frame by 'frame*framesize' to get the correct slice for the slider position.
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

// Constructor
// create LAF (unique ptr because LookAndFeel is non-copyable and needs to be heap allocated)
// Create four buttons in loop, strap a radiogroup to them
// Set up slider, attach LAF. Callback updates the local value [lambda]
// Drag-end sends the value to the server once dragging stops (don't overload with PUT requests while dragging)
// Label displays paramer N

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

// Destructor
// slider has a raw point to LAF, so clear it before knobLookAndFeel is destroyed will avoid hangin pointer.
MainComponent::~MainComponent()
{
    parameterSlider.setLookAndFeel (nullptr);
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour::fromRGB (47, 62, 67));
}

// Layout
// Rop row 4 buttons, bottom status row, left corner rotary knob, right section parameter name and value
void MainComponent::resized()
{
    auto area = getLocalBounds().reduced (36);
    auto buttonArea = area.removeFromTop (70);
    const auto buttonWidth = buttonArea.getWidth() / parameterCount;

    for (auto& button : parameterButtons)
        button.setBounds (buttonArea.removeFromLeft (buttonWidth).reduced (6));

    statusLabel.setBounds (area.removeFromBottom (26));

    auto knobArea = area.removeFromLeft (area.getWidth() / 2).reduced (24);
    parameterSlider.setBounds (knobArea);

    auto valueArea = area.reduced (24);
    parameterNameLabel.setBounds (valueArea.removeFromTop (70));
    parameterValueLabel.setBounds (valueArea);
}

// the button only changes which stored value is shown, updateDisplayParameter refreshes knob, name and value labels...
void MainComponent::selectParameter (int index)
{
    selectedParameter = index;
    updateDisplayedParameter();
}

// Load parameters from the server on a background thread
// Update the UI when they are loaded, or show an error if the server is unavailable
// Uses juce::Component::SafePointer to aboid accessing  Maincomponent after it's deleted, 
// and juce::MessageManager::callAsync to post the update back to the message thread.
// Production app would be more robust: handle exceptions, validate server response, etc. 
// juce components should update on the message thread.
//  Then returns to UI thread (callAsync)
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

// send parameter value to server on backgroundthread. 
// Starts when user finishes dragging the slider. 
// Launches a background PUT request, and updates stauts label. 
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

// Update the display (reads selected values/parameters), updates the components
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
