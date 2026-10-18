#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace ayra
{

/**
 * @class MarqueeSelector
 * @brief Helper riutilizzabile per selezione marquee ad area rettangolare.
 * Importato dal modulo ayra_selection_engine.
 */
class MarqueeSelector
{
public:
    enum class ModifierMode
    {
        Replace, ///< Sostituisce la selezione corrente (nessun modificatore)
        Add,     ///< Aggiunge alla selezione (Shift)
        Subtract,///< Rimuove dalla selezione (Alt)
        Toggle   ///< Toggle: inverte selezione (Cmd/Ctrl)
    };

    static constexpr int kDragThreshold = 3;

    explicit MarqueeSelector (juce::Component& ownerComponent) : owner (ownerComponent) {}

    bool handleMouseDown (const juce::MouseEvent& e)
    {
        if (! e.mods.isLeftButtonDown() || e.mods.isAltDown())
        {
            isActive = false;
            dragging = false;
            return false;
        }

        isActive = true;
        dragging = false;
        startPos = e.getPosition();
        marqueeRect.setBounds (e.x, e.y, 0, 0);
        currentMode = getModifierMode (e.mods);
        return true;
    }

    bool handleMouseDrag (const juce::MouseEvent& e)
    {
        if (! isActive)
        {
            dragging = false;
            return false;
        }

        if (! dragging && e.getDistanceFromDragStart() > kDragThreshold)
            dragging = true;

        if (dragging)
        {
            auto pos = e.getPosition();
            marqueeRect.setBounds (juce::jmin (startPos.x, pos.x), juce::jmin (startPos.y, pos.y),
                                   std::abs (pos.x - startPos.x), std::abs (pos.y - startPos.y));
            owner.repaint();
        }

        return dragging;
    }

    bool handleMouseUp (const juce::MouseEvent& e)
    {
        if (! isActive)
        {
            dragging = false;
            return false;
        }

        if (dragging)
        {
            for (const auto& listener : marqueeSelectorListeners.getListeners())
                listener->performMarqueeSelection (marqueeRect, currentMode);

            if (onPerformMarqueeSelection)
                onPerformMarqueeSelection (marqueeRect, currentMode);
        }
        else
        {
            for (const auto& listener : marqueeSelectorListeners.getListeners())
                listener->performClickSelection (startPos, currentMode);

            if (onPerformClickSelection)
                onPerformClickSelection (startPos, currentMode);
        }

        isActive = false;
        dragging = false;
        marqueeRect = {};
        owner.repaint();
        return true;
    }

    [[nodiscard]] bool isDragging() const noexcept { return dragging; }
    [[nodiscard]] juce::Rectangle<int> getMarqueeRect() const noexcept { return marqueeRect; }
    [[nodiscard]] ModifierMode getCurrentMode() const noexcept { return currentMode; }

    void drawMarquee (juce::Graphics& g) const
    {
        if (! dragging || marqueeRect.isEmpty())
            return;

        g.setColour (fillColour);
        g.fillRect (marqueeRect);
        g.setColour (outlineColour);
        g.drawRect (marqueeRect, 1.0f);
    }

    void setFillColour (juce::Colour c) noexcept { fillColour = c; }
    [[nodiscard]] juce::Colour getFillColour() const noexcept { return fillColour; }

    void setOutlineColour (juce::Colour c) noexcept { outlineColour = c; }
    [[nodiscard]] juce::Colour getOutlineColour() const noexcept { return outlineColour; }

    struct Listener
    {
        virtual ~Listener() = default;
        virtual void performMarqueeSelection (const juce::Rectangle<int>& area, ModifierMode mode) = 0;
        virtual void performClickSelection (juce::Point<int> pos, ModifierMode mode) = 0;
    };

    void addSelectionListener (Listener* newListener) noexcept { marqueeSelectorListeners.add (newListener); }
    void removeSelectionListener (Listener* listener) noexcept { marqueeSelectorListeners.remove (listener); }

    std::function<void(const juce::Rectangle<int>& area, ModifierMode mode)> onPerformMarqueeSelection;
    std::function<void(juce::Point<int> pos, ModifierMode mode)> onPerformClickSelection;

private:
    juce::Component& owner;
    bool isActive = false;
    bool dragging = false;
    juce::Point<int> startPos;
    juce::Rectangle<int> marqueeRect;
    ModifierMode currentMode = ModifierMode::Replace;

    juce::Colour fillColour { juce::Colour (0x2800d084) };
    juce::Colour outlineColour { juce::Colour (0xc000d084) };

    static ModifierMode getModifierMode (const juce::ModifierKeys& mods)
    {
        if (mods.isCommandDown() || mods.isCtrlDown())
            return ModifierMode::Toggle;
        if (mods.isShiftDown())
            return ModifierMode::Add;
        if (mods.isAltDown())
            return ModifierMode::Subtract;
        return ModifierMode::Replace;
    }

    juce::ListenerList<Listener> marqueeSelectorListeners;
};

} // namespace ayra
