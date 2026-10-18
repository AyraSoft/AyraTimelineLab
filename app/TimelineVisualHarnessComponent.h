#pragma once
#include "ayra_ui.h"

namespace ayra::lab
{

class LabComponent;

/** Banco scrollabile che istanzia le sei implementazioni clip reali del Lab.
    Le canvas mantengono ownership di hit-test e gesture; il banco fornisce solo
    composizione, dimensioni di collaudo e aggiornamento dalla fixture condivisa. */
class TimelineVisualHarnessComponent final : public juce::Component
{
public:
    explicit TimelineVisualHarnessComponent (LabComponent&);
    ~TimelineVisualHarnessComponent() override;

    void refreshFromModel();
    void resized() override;

private:
    class Content;

    juce::Viewport viewport;
    std::unique_ptr<Content> content;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TimelineVisualHarnessComponent)
};

} // namespace ayra::lab
