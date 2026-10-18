/** @file CanonicalTheme.cpp
    Root adapter del SOLO tema. Legge gli header originali, senza copiarli.
    Le dipendenze generali Ayra non entrano nel banco e il codice JUCE resta intatto.
    Non e' il caricamento dell'intero MultiverseLookAndFeel.
    @see CMakeLists.txt, LabLookAndFeel.h */
#include "LabLookAndFeel.h"
#include <array>

namespace ayra::lab
{
Palette readCanonicalPalette(bool dark)
{
    namespace tk=ayra::tokens;
    tk::setThemeMode(dark?tk::Mode::Dark:tk::Mode::Light);
    return {tk::surfaceBase(),tk::surfaceBaseTop(),tk::surfaceBaseBot(),
            tk::surfaceRaised(),tk::surfaceRecessed(),tk::borderHairline(),
            tk::textPrimary(),tk::textSecondary(),tk::textValue(),
            tk::accent(),tk::muteIndicator(),tk::soloIndicator(),tk::playIndicator()};
}
void canonicalRaised(juce::Graphics& g,juce::Rectangle<float> bounds,float radius,juce::Colour base)
{
    ayra::tokens::fillRaisedSurface(g,bounds,radius,base);
}
}
