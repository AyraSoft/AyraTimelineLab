/** @file Main.cpp
    Native timeline showcase by default. --legacy / --session retain the original
    clip-design Lab and JSON v1. No device, decoder, plugin scanner or network. */
#include "LabComponent.h"
#include "../demo/TimelinePanelDemo.h"

class TimelineLabApplication final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName()override{return "Ayra Timeline Lab";}
    const juce::String getApplicationVersion()override{return "0.1.0";}
    bool moreThanOneInstanceAllowed()override{return true;}
    void initialise(const juce::String& commandLine)override
    {
        const bool loadSession=commandLine.startsWith("--session ");
        window=std::make_unique<Window>(loadSession || commandLine.trim()=="--legacy");
        if(loadSession && window->legacyLab!=nullptr)
            window->legacyLab->loadFile(juce::File(commandLine.fromFirstOccurrenceOf("--session ",false,false).trim().unquoted()));
    }
    void shutdown()override{window.reset();}
    void systemRequestedQuit()override{quit();}
private:
    class Window final : public juce::DocumentWindow
    {
    public:
        explicit Window(bool legacy):juce::DocumentWindow("Ayra Timeline Lab",juce::Colour(0xff1b1f26),juce::DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar(true);
            if(legacy){legacyLab=new ayra::lab::LabComponent;setContentOwned(legacyLab,true);}
            else setContentOwned(ayra::lab::makeTimelinePanelDemo().release(),true);
            setResizable(true,true);setResizeLimits(1200,720,3840,2400);
            centreWithSize(1480,1040);setVisible(true);
        }
        ayra::lab::LabComponent* legacyLab=nullptr;
        void closeButtonPressed()override{juce::JUCEApplication::getInstance()->systemRequestedQuit();}
    };
    std::unique_ptr<Window> window;
};
START_JUCE_APPLICATION(TimelineLabApplication)
