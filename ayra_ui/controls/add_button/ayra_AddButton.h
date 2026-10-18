/*
  ==============================================================================

   This file is part of the Ayra ecosystem.
   Copyright (c) Ayra Productions.

   ayra_common / widgets / controls / AddButton
   ------------------------------------------------------------------
   Pulsante canonico per aggiungere un elemento. Centralizza glifo e aspetto;
   ogni consumer specifica soltanto tooltip e azione tramite `onClick` o il
   `juce::Button::Listener` ereditato.

  ==============================================================================
*/

#pragma once

namespace ayra
{

/** Comando atomico "+" condiviso da pannelli, menu e finestre Ayra.

    La classe e' l'unica fonte di verita' per glifo, palette, padding, sfondo e
    cursore. Non conserva stato di dominio e non e' un toggle: il consumer
    assegna il tooltip e implementa l'azione con la normale dual API JUCE
    (`addListener` oppure `onClick`). */
class AddButton final : public ShapeIconButton
{
public:
    explicit AddButton (juce::String tooltipText = {});
    ~AddButton() override = default;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AddButton)
};

} // namespace ayra
