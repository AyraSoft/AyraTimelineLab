/** @file LabComponent.h
    Host del banco: un solo Store alimenta i due consumer Timeline e Mixer.
    Il pannello destro espone fixture di Graph/Routing/automation e l'editor clip.
    Le righe sono riconciliate per ID: riordino non distrugge i controlli.
    @see TrackHeader, LabModel, README.md */
#pragma once
#include "TrackHeader.h"
#include "SessionCodec.h"
#include "clips/ayra_clips.h"
#include <map>

namespace ayra::lab
{
class TimelineLaneRow;
class TimelineVisualHarnessComponent;
class LabComponent final : public juce::Component,
                           public juce::DragAndDropContainer,
                           private juce::AsyncUpdater
{
public:
    LabComponent();
    ~LabComponent() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress&) override;
    void handle(HeaderIntent);
    void openClip(const std::string& track,const std::string& clip);
    void move(const std::string& dragged,const std::string& anchor,bool after);
    void scrollWhileDragging(juce::Point<int> screen);
    Store& store()noexcept{return model;}
    LabLookAndFeel& look()noexcept{return laf;}
    void selectClip(const std::string&);
    [[nodiscard]] bool deleteClip(const std::string& trackId,
                                  const std::string& clipId);
    void requestTimelineContextMenu(
        const clips::TimelinePointerContext& context) const;
    std::function<void(const clips::TimelinePointerContext&)>
        onTimelineContextMenuRequested;
    void updateClipFade(const std::string& trackId, const std::string& clipId, double fadeIn, double fadeOut);
    /** Commits the structural split and returns the committed right-hand clip id.
        An empty id means the model rejected the operation. */
    [[nodiscard]] std::string splitClip(const std::string& trackId,
                                        const std::string& clipId,
                                        double splitTime);
    void moveClip(const std::string& trackId, const std::string& clipId, double newStart);
    void resizeClip(const std::string& trackId, const std::string& clipId, double newStart, double newLength);
    /** Duplicates a clip in loop fashion, creating count copies either after or before.
        Returns the committed clip IDs. */
    std::vector<std::string> duplicateClipLoop(const std::string& trackId,
                                               const std::string& clipId,
                                               int count,
                                               bool appendAfter);
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void adjustVerticalZoom(float delta);
    void setVerticalZoom(float factor);
    float getVerticalZoom() const noexcept { return verticalZoomFactor; }
    void setWarpFeatureEnabled(bool enabled);
    [[nodiscard]] bool isWarpFeatureEnabled() const noexcept;
    void setGainFeatureEnabled(bool enabled);
    [[nodiscard]] bool isGainFeatureEnabled() const noexcept;
    void loadFile(const juce::File&);
private:
    LabLookAndFeel laf;
    Store model;
    juce::TooltipWindow tooltips{this, 350};
    juce::TextButton grouped{"Raggruppata"},free{"Libera"},theme{"Tema"},undoButton{"Undo"},redoButton{"Redo"},saveButton{"Esporta JSON"},loadButton{"Importa JSON"},resetButton{"Reset"},collapse{"Ripiega gruppo"};
    juce::TextButton warpFeatureButton{"Warp"}, gainFeatureButton{"Gain"};
    juce::Label zoomVLabel;
    juce::TextButton zoomOutVButton{"-"}, zoomInVButton{"+"}, zoomResetButton{"100%"};
    juce::Slider zoomVSlider;
    float verticalZoomFactor = 1.0f;
    std::map<std::string, int> baseTrackHeights;
    juce::Label status, globalLanes;
    juce::Viewport timelineViewport;
    juce::Component timelineContent;
    std::unique_ptr<TimelineVisualHarnessComponent> harnessView;
    std::map<std::string,std::unique_ptr<TimelineLaneRow>> rows;
    std::unique_ptr<juce::FileChooser> chooser;
    std::map<std::string,int> resizePreviews;
    void handleAsyncUpdate()override;
    void refresh();
    void layoutRows();
    void chooseFile(bool save);
    void cancelInput();
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LabComponent)
};
}
