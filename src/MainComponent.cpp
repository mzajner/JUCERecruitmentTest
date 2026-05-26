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
    startTimer (2000);

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
        slider.onDragStart = [this, i]
        {
            isDraggingSlider[(size_t) i] = true;
            statusLabel.setText ("Dragging Param " + juce::String (i + 1) + "...", juce::NotificationType::dontSendNotification);
        };
        slider.onDragEnd = [this, i]
        {
            const auto value = static_cast<int> (paramSliders[(size_t) i].getValue());

            isDraggingSlider[static_cast<size_t> (i)] = false;
            isWritePending[(size_t) i] = false;
            sendParameterValue (i, value);
        };
        addAndMakeVisible (slider);
    }

    statusLabel.setText ("Connecting...", juce::dontSendNotification);
    statusLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (statusLabel);
}

MainComponent::~MainComponent()
{
    stopTimer();

    for (auto& slider : paramSliders)
        slider.setLookAndFeel (nullptr);
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

void MainComponent::timerCallback()
{
    if (isPolling.exchange (true))
        return;

    // Launch a new thread to do the polling so that we don't block the message thread while waiting for the server response.
    juce::Thread::launch ([this]
    {
        std::array<int, 4> values {};
        juce::String errorMessage;

        for (int i = 0; i < static_cast<int> (values.size()); ++i)
        {
            auto url = juce::URL ("http://127.0.0.1:8000/parameters/" + juce::String (i));

            auto inputStream = url.createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                                                    .withConnectionTimeoutMs (1500));
            
            if (inputStream == nullptr)
            {
                errorMessage= "Server unavailable";
                break;
            }

            auto response = inputStream->readEntireStreamAsString();
            auto parsed = juce::JSON::parse (response);

            if (! parsed.isObject())
            {
                errorMessage = "Invalid response";
                return;
            }

            auto* object = parsed.getDynamicObject();
            values[static_cast<size_t> (i)] = static_cast<int> (object->getProperty ("value"));
        }

        juce::MessageManager::callAsync ([this, values, errorMessage]
        {
            if (errorMessage.isNotEmpty())
            {
                statusLabel.setText (errorMessage, juce::NotificationType::dontSendNotification);
                return;
            }
            else
            {
                for (int i = 0; i < static_cast<int> (paramSliders.size()); ++i)
                    paramSliders[static_cast<size_t> (i)].setValue (values[static_cast<size_t> (i)], juce::NotificationType::dontSendNotification);

                        statusLabel.setText ("Connected", juce::NotificationType::dontSendNotification);

            }
            
            isPolling = false;
        });
    });
}

void MainComponent::sendParameterValue (int index, int value)
{
    juce::Thread::launch ([this, index, value]
    {
        auto body = juce::String ("{\"value\": ") + juce::String (value) + "}";
        auto url = juce::URL ("http://127.0.0.1:8000/parameters/" + juce::String (index)).withPOSTData (body);

        auto inputStream = url.createInputStream (
            juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                .withExtraHeaders ("Content-Type: application/json")
                .withConnectionTimeoutMs (1500)
                .withNumRedirectsToFollow (0)
                .withHttpRequestCmd ("PUT"));
        
        juce::String statusMessage;

        if (inputStream == nullptr)
        {
            statusMessage= "Server unavailable";
        }

        else
        {
            auto response = inputStream->readEntireStreamAsString();
            auto parsed = juce::JSON::parse (response);

            statusMessage = parsed.isObject()
                                ? "Parameter " + juce::String (index + 1) + " set to " + juce::String (value)
                                : "Invalid response";
        }

        juce::MessageManager::callAsync ([this, statusMessage]
        {
            statusLabel.setText (statusMessage, juce::NotificationType::dontSendNotification);
        });
    });
}