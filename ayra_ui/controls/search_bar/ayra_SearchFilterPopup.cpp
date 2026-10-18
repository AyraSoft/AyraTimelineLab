/*
  ==============================================================================

   This file is part of the Ayra ecosystem.
   Copyright (c) Ayra Productions.

   ayra_common / widgets / controls / SearchFilterPopup (impl)

   Doctrine: ZERO #include (tipi via ayra_common.h), namespace ayra, ASCII-only.
  ==============================================================================
*/

namespace ayra
{

//==============================================================================
// Layout (pixel logici) — condivisi fra preferredHeight() e resized().
//==============================================================================
namespace
{
    // Prefisso kSfp* (Search Filter Popup): questi costanti vivono in un
    // anonymous namespace che, nel modulo single-TU (ayra_common.cpp), viene
    // fuso con quello di ogni altro sub-.cpp. Nomi generici (kHeaderH, kPad)
    // collidono fra widget: il prefisso garantisce unicita' nel TU.
    constexpr int kSfpPad        = 12;   ///< SFP: padding esterno del popup.
    constexpr int kSfpHeaderH    = 18;   ///< SFP: altezza etichetta di sezione.
    constexpr int kSfpToggleRowH = 28;   ///< SFP: altezza riga toggle (label + pillola).
    constexpr int kSfpSectionGap = 10;   ///< SFP: gap fra sezioni.
    constexpr int kSfpSegmentedH = 30;   ///< SFP: altezza del segmented.
    constexpr int kSfpResetGap   = 12;   ///< SFP: gap sopra il bottone reset.
    constexpr int kSfpResetH     = 30;   ///< SFP: altezza del bottone reset.
}

//==============================================================================
SearchFilterPopup::SearchFilterPopup (juce::Array<ToggleSection> sectionsIn,
                                      SegmentedSection segmentedIn,
                                      std::function<void()> onChangeIn,
                                      std::function<void()> onResetIn)
    : sections (std::move (sectionsIn)),
      segmented (std::move (segmentedIn)),
      onChange (std::move (onChangeIn)),
      onReset (std::move (onResetIn))
{
    // Una label-header per sezione + una riga (label + pillola) per ogni toggle.
    // I bool* vivono nel consumer (che sopravvive al popup: e' nel callout padre).
    for (const auto& section : sections)
    {
        auto* header = sectionHeaderLabels.add (new juce::Label ({}, section.header));
        addAndMakeVisible (header);

        for (const auto& toggle : section.toggles)
        {
            auto* lb = toggleLabels.add (new juce::Label ({}, toggle.label));
            lb->setAlpha (0.72f);
            addAndMakeVisible (lb);

            auto* tg = toggleControls.add (new ayra::SettingToggle());
            tg->setToggleState (toggle.flag != nullptr && *toggle.flag, juce::dontSendNotification);
            toggleFlags.add (toggle.flag);

            bool* flag = toggle.flag;
            tg->onToggle = [this, flag] (bool on)
            {
                if (flag != nullptr) { *flag = on; }
                if (onChange) { onChange(); }
            };
            addAndMakeVisible (tg);
        }
    }

    if (hasSegmented())
    {
        segmentedHeaderLabel.setText (segmented.header, juce::dontSendNotification);
        addAndMakeVisible (segmentedHeaderLabel);

        segmentedControl.setOptions (segmented.options);
        if (segmented.getIndex) { segmentedControl.setSelectedIndex (segmented.getIndex(), juce::dontSendNotification); }
        segmentedControl.onChange = [this] (int idx)
        {
            if (segmented.setIndex) { segmented.setIndex (idx); }
            if (onChange) { onChange(); }
        };
        addAndMakeVisible (segmentedControl);
    }

    resetBtn.onClick = [this]
    {
        if (onReset) { onReset(); }   // il consumer riporta il proprio stato ai default
        syncFromState();              // riallinea i controlli del popup
        if (onChange) { onChange(); } // rifa' il filtro
    };
    addAndMakeVisible (resetBtn);
}

SearchFilterPopup::~SearchFilterPopup() = default;

//==============================================================================
void SearchFilterPopup::syncFromState()
{
    for (int i = 0; i < toggleControls.size(); ++i)
    {
        const bool on = toggleFlags[i] != nullptr && *toggleFlags[i];
        toggleControls[i]->setToggleState (on, juce::dontSendNotification);
    }

    if (hasSegmented() && segmented.getIndex)
        segmentedControl.setSelectedIndex (segmented.getIndex(), juce::dontSendNotification);
}

int SearchFilterPopup::preferredHeight() const noexcept
{
    int h = kSfpPad;   // padding superiore

    for (const auto& section : sections)
        h += kSfpHeaderH + (int) section.toggles.size() * kSfpToggleRowH + kSfpSectionGap;

    if (hasSegmented())
        h += kSfpHeaderH + kSfpSegmentedH;

    h += kSfpResetGap + kSfpResetH + kSfpPad;   // gap + reset + padding inferiore
    return h;
}

//==============================================================================
void SearchFilterPopup::paint (juce::Graphics& g)
{
    ayra::fillDarkCard (g, getLocalBounds().toFloat());
}

void SearchFilterPopup::resized()
{
    auto area = getLocalBounds().reduced (kSfpPad);

    auto placeToggleRow = [&] (juce::Label& lb, ayra::SettingToggle& tg)
    {
        auto row = area.removeFromTop (kSfpToggleRowH);
        tg.setBounds (row.removeFromRight (ayra::SettingToggle::kWidth)
                         .withSizeKeepingCentre (ayra::SettingToggle::kWidth, ayra::SettingToggle::kHeight));
        lb.setBounds (row);
    };

    // Le pillole sono flat (toggleControls/toggleLabels): un cursore avanza sezione
    // per sezione seguendo il numero di toggle di ognuna.
    int toggleCursor = 0;
    for (int s = 0; s < sections.size(); ++s)
    {
        if (auto* header = sectionHeaderLabels[s]) { header->setBounds (area.removeFromTop (kSfpHeaderH)); }

        for (int t = 0; t < (int) sections[s].toggles.size(); ++t)
        {
            if (toggleCursor < toggleControls.size())
                placeToggleRow (*toggleLabels[toggleCursor], *toggleControls[toggleCursor]);
            ++toggleCursor;
        }

        area.removeFromTop (kSfpSectionGap);
    }

    if (hasSegmented())
    {
        segmentedHeaderLabel.setBounds (area.removeFromTop (kSfpHeaderH));
        segmentedControl.setBounds (area.removeFromTop (kSfpSegmentedH));
    }

    area.removeFromTop (kSfpResetGap);
    resetBtn.setBounds (area.removeFromTop (kSfpResetH));
}

//==============================================================================
void SearchFilterPopup::launch (juce::Component& anchor, std::unique_ptr<SearchFilterPopup> popup)
{
    popup->setSize (kWidth, popup->preferredHeight());
    CallOutMenu::launchOnDesktop (std::move (popup), anchor);
}

} // namespace ayra
