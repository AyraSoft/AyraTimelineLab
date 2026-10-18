/** @file Main.cpp
    Applicazione GUI separata. Nessun dispositivo audio, rete o plugin scanner.
    @see README.md, LabComponent */
#include "LabComponent.h"

class TimelineLabApplication final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName()override{return "Ayra Timeline Lab";}
    const juce::String getApplicationVersion()override{return "0.1.0";}
    bool moreThanOneInstanceAllowed()override{return true;}
    void initialise(const juce::String& commandLine)override
    {
        window=std::make_unique<Window>();
        if(commandLine.startsWith("--session "))
            window->lab().loadFile(juce::File(commandLine.fromFirstOccurrenceOf("--session ",false,false).trim().unquoted()));
    }
    void shutdown()override{window.reset();}
    void systemRequestedQuit()override{quit();}
private:
    class Window final : public juce::DocumentWindow
    {
    public:
        Window():juce::DocumentWindow("Ayra Timeline Lab",juce::Colour(0xff1b1f26),juce::DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar(true);auto* component=new ayra::lab::LabComponent;
            setContentOwned(component,true);setResizable(true,true);setResizeLimits(1200,720,3840,2400);
            centreWithSize(1480,1040);setVisible(true);
        }
        ayra::lab::LabComponent& lab(){return *static_cast<ayra::lab::LabComponent*>(getContentComponent());}
        void closeButtonPressed()override{juce::JUCEApplication::getInstance()->systemRequestedQuit();}
    };
    std::unique_ptr<Window> window;
};
START_JUCE_APPLICATION(TimelineLabApplication)
