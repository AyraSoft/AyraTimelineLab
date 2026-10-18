/*
  ==============================================================================

   This file is part of the Ayra ecosystem.
   Copyright (c) Ayra Productions.

   ayra_common / widgets / controls / ColourButton (impl)
   ------------------------------------------------------------------
   Implementazione del bottone di scelta colore + il picker standardizzato
   (file-locale, riusato da OGNI istanza). Vedi l'header per la doctrine.
   Regola moduli Ayra: nessun include qui (tipi via ayra_common.h).
  ==============================================================================
*/

namespace ayra
{

//==============================================================================
// Picker standardizzato: riga di swatch quick-pick + ColourSelector, dentro un
// CallOutBox ancorato al bottone. UNICO punto di scelta colore dell'app.
//==============================================================================
namespace
{
    class ColourPickerCallout final : public juce::Component,
                                      private juce::ChangeListener
    {
    public:
        ColourPickerCallout (juce::Colour initial, const juce::Array<juce::Colour>& palette)
        {
            selector.setColour (juce::ColourSelector::backgroundColourId, juce::Colours::transparentBlack);
            selector.setCurrentColour (initial, juce::dontSendNotification);
            selector.addChangeListener (this);
            addAndMakeVisible (selector);

            // Quick-pick: ColourButton in modalita' swatch (niente picker annidato).
            for (const juce::Colour c : palette)
            {
                auto chip = std::make_unique<ColourButton>();
                chip->setColour (c);
                chip->setPickerEnabled (false);
                chip->onClick = [this, c] { selector.setCurrentColour (c); };
                addAndMakeVisible (*chip);
                chips.add (std::move (chip));
            }

            setSize (300, 360);
        }

        ~ColourPickerCallout() override { selector.removeChangeListener (this); }

        /// Notificato live a ogni cambio (drag incluso); alpha forzato opaco.
        std::function<void (juce::Colour)> onColourChosen;

        void resized() override
        {
            auto area = getLocalBounds().reduced (10);

            if (! chips.isEmpty())
            {
                constexpr int chipSz = 22, gap = 6;
                const int perRow = juce::jmax (1, (area.getWidth() + gap) / (chipSz + gap));
                const int rows   = (chips.size() + perRow - 1) / juce::jmax (1, perRow);

                auto strip   = area.removeFromTop (rows * (chipSz + gap));
                auto rowArea = strip.removeFromTop (chipSz);
                int col = 0;
                for (auto* chip : chips)
                {
                    chip->setBounds (rowArea.removeFromLeft (chipSz));
                    rowArea.removeFromLeft (gap);
                    if (++col >= perRow) { col = 0; strip.removeFromTop (gap); rowArea = strip.removeFromTop (chipSz); }
                }
                area.removeFromTop (6);
            }

            selector.setBounds (area);
        }

    private:
        void changeListenerCallback (juce::ChangeBroadcaster*) override
        {
            if (onColourChosen) { onColourChosen (selector.getCurrentColour().withAlpha (1.0f)); }
        }

        juce::ColourSelector selector { juce::ColourSelector::showColourspace | juce::ColourSelector::showSliders };
        juce::OwnedArray<ColourButton> chips;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ColourPickerCallout)
    };
}

//==============================================================================
ColourButton::ColourButton()
{
    swatches = AppearanceSettings::getDefaultSwatches();
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setTooltip ("Choose colour (right-click to reset)");
}

ColourButton::~ColourButton()
{
    pickerEnabled = false;
    dismissPicker();
}

void ColourButton::dismissPicker() noexcept
{
    auto picker = activePicker;
    activePicker = nullptr;
    if (picker == nullptr) { return; }
    // JUCE puo' reclamare il CallOutBox dopo la LAF scoped del suo owner.
    picker->setLookAndFeel (nullptr);
    picker->setVisible (false);
    picker->dismiss();
}

void ColourButton::setColour (juce::Colour newColour)
{
    if (colour == newColour) { return; }
    colour = newColour;
    repaint();
}

void ColourButton::setOverridden (bool isOverridden)
{
    if (overridden == isOverridden) { return; }
    overridden = isOverridden;
    repaint();
}

void ColourButton::setSwatches (juce::Array<juce::Colour> palette)
{
    swatches = std::move (palette);
}

void ColourButton::setPickerEnabled (bool shouldOpenPickerOnClick) noexcept
{
    pickerEnabled = shouldOpenPickerOnClick;
    if (! pickerEnabled) { dismissPicker(); }
}

void ColourButton::openPicker()
{
    if (! isEnabled() || ! pickerEnabled) { return; }
    if (activePicker != nullptr && activePicker->isVisible())
    {
        activePicker->toFront (true);
        return;
    }

    juce::Component::SafePointer<ColourButton> safe (this);
    if (onPickerOpened) { onPickerOpened(); }
    if (safe == nullptr || ! safe->isEnabled() || ! safe->pickerEnabled) { return; }

    auto content = std::make_unique<ColourPickerCallout> (colour, swatches);
    content->onColourChosen = [safe] (juce::Colour c)
    {
        // L'autorita' puo' cambiare fra apertura del picker e callback asincrona.
        // Non cambiare neppure lo swatch locale quando l'editing e' stato revocato.
        if (safe == nullptr || ! safe->isEnabled() || ! safe->pickerEnabled) { return; }
        safe->setColour (c);
        if (safe->onColourPicked) { safe->onColourPicked (c); }
    };

    auto& box = CallOutMenu::launchOnDesktop (std::move (content), *this);
    if (safe != nullptr)
    {
        safe->activePicker = &box;
    }
    else
    {
        box.dismiss();
    }
}

void ColourButton::paint (juce::Graphics& g)
{
    const auto r      = getLocalBounds().toFloat().reduced (1.0f);
    const float corner = r.getHeight() * 0.35f;

    g.setColour (colour);
    g.fillRoundedRectangle (r, corner);

    // Dual-rim neumorfico (scuro fuori, chiaro dentro) — coerente col design system.
    g.setColour (tokens::componentColour (*this, rimDarkColourId, tokens::rimDark()));
    g.drawRoundedRectangle (r, corner, 1.0f);
    g.setColour (tokens::componentColour (*this, rimLightColourId, tokens::rimLight()));
    g.drawRoundedRectangle (r.reduced (1.0f), corner - 1.0f, 1.0f);

    if (overridden)
    {
        const float d = juce::jmin (6.0f, r.getHeight() * 0.45f);
        const juce::Rectangle<float> dot (r.getRight() - d - 3.0f, r.getY() + 3.0f, d, d);
        const auto marker = tokens::componentColour (*this, markerColourId, tokens::accent());
        g.setColour (marker);
        g.fillEllipse (dot);
        g.setColour (marker.contrasting (0.85f).withAlpha (0.85f));
        g.drawEllipse (dot, 1.0f);
    }
}

void ColourButton::mouseUp (const juce::MouseEvent& e)
{
    if (! isEnabled() || e.mouseWasDraggedSinceMouseDown()
        || ! getLocalBounds().contains (e.getPosition()))
        return;

    if (e.mods.isPopupMenu())
    {
        if (onReset) { onReset(); }
        return;
    }

    if (pickerEnabled) { openPicker(); }
    else if (onClick)  { onClick(); }
}

} // namespace ayra
