/** @file TrackHeader.h
    Un solo widget per SongTrack, PerformerTrack, PerformerMix e SongMain.
    Riceve una proiezione e invia intenti; non possiede Store, routing o processor.
    I bottoni/slider JUCE reali offrono focus e azioni accessibili.
    @see LabComponent, LabLookAndFeel, core/LabModel.h */
#pragma once
#include "LabLookAndFeel.h"

namespace ayra::lab
{
enum class HeaderAction
{
    select, mute, solo, enabled, bypass, buffer, oversample, expanded, pinned,
    gain, pan, monitor, graph, routing, automations, settings, rename,
    resizePreview, resizeCommit, resizeCancel, stepUp, stepDown
};
struct HeaderIntent
{
    HeaderAction action=HeaderAction::select;
    std::string id, text;
    double value=0;
    Selection selection=Selection::replace;
};
struct HeaderLayout
{
    juce::Rectangle<int> grip,disclosure,name;
    std::array<juce::Rectangle<int>,7> controls;
    juce::Rectangle<int> gain,pan,monitor,automations,settings,resize,pin;
    std::array<juce::Rectangle<int>,2> advanced;
    static HeaderLayout calculate(int width,int height,bool compact,bool hasSolo);
};

class TrackHeader final : public juce::Component
{
public:
    explicit TrackHeader(LabLookAndFeel&);
    ~TrackHeader() override;
    void setModel(const Track&,Mode,bool selected);
    std::function<void(HeaderIntent)> onIntent;
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    bool keyPressed(const juce::KeyPress&) override;
    void focusLost(FocusChangeType) override;
    bool isEditing() const noexcept;
private:
    LabLookAndFeel& laf;
    Track model;
    Mode mode=Mode::grouped;
    bool selected=false,syncing=false,resizing=false;
    int startY=0,startHeight=0,previewHeight=0;
    juce::Label name;
    juce::TextButton grip{"::"},disclosure{">"},mute{"M"},solo{"S"},power{"ON"},bypass{"BYP"},graph{"Graph"},routing{"Routing"},automations{"Autom."},settings{"..."},pin{"PIN"},buffer{"Buffer"},oversample{"2x"};
    juce::Slider gain,pan;
    juce::ComboBox monitor;
    void send(HeaderAction,double value=0,std::string text={},Selection=Selection::replace);
    void cancelResize();
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TrackHeader)
};
}
