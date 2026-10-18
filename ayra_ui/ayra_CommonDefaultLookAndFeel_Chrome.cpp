/*
  ==============================================================================

   This file is part of the Ayra ecosystem.
   Copyright (c) Ayra Productions.

  ==============================================================================
*/

/**
 * @file ayra_CommonDefaultLookAndFeel_Chrome.cpp
 * @brief Ricette CHROME (Frosted Lab): fondo app, pannelli, glass strip,
 *        header di sezione, icon chip, logo badge.
 *
 * Partial di `CommonDefaultLookAndFeel`. NESSUN #include qui: questo file e'
 * incluso da `ayra_CommonDefaultLookAndFeel.cpp` (convenzione moduli Ayra).
 */

namespace ayra
{

namespace tk = tokens;

//==============================================================================
void CommonDefaultLookAndFeel::drawAppBackground (juce::Graphics& g, juce::Rectangle<int> bounds)
{
    // CACHE del fondo (gradiente + dot-grid). La dot-grid nuda sarebbe ~(W/28)x(H/28)
    // fillEllipse a OGNI paint (1920x1080 -> ~2600 ellissi/frame): costo che esplode
    // durante resize/drag. Renderizziamo una volta per (dimensione, tema) e blittiamo.
    // La cache e' un MEMBRO d'istanza (vedi .h), non un function-local static.
    // Una stessa LAF viene pero' applicata a main window, finestre e callout: la
    // piccola LRU evita che dimensioni diverse si espellano a vicenda, rilasciando
    // CoreGraphicsPixelData ancora a ogni repaint alternato.
    const int w = bounds.getWidth(), h = bounds.getHeight();
    if (w <= 0 || h <= 0) { return; }

    // Renderer condiviso (gradiente + dots) con origine esplicita: usato sia per
    // riempire l'immagine di cache (origine 0,0) sia come fallback diretto.
    auto renderBg = [] (juce::Graphics& gg, float ox, float oy, int ww, int hh)
    {
        tk::fillWindowBackground (gg, juce::Rectangle<float> (ox, oy, (float) ww, (float) hh));
        gg.setColour (tk::borderHairline().withAlpha (0.35f));
        for (int y = 28; y < hh; y += 28)
            for (int x = 28; x < ww; x += 28)
                gg.fillEllipse (ox + (float) x - 1.0f, oy + (float) y - 1.0f, 2.0f, 2.0f);
    };

    // Superfici enormi (multi-monitor/HiDPI): niente immagine di cache (un RGB
    // 8192x8192 ~= 192 MB allocati nel paint). Si disegna direttamente, ancorato
    // all'origine reale di bounds.
    constexpr int kMaxCachedDim = 4096;
    constexpr std::int64_t kMaxCachedPixels = 5 * 1024 * 1024;
    if (w > kMaxCachedDim || h > kMaxCachedDim
        || static_cast<std::int64_t> (w) * h > kMaxCachedPixels)
    {
        renderBg (g, (float) bounds.getX(), (float) bounds.getY(), w, h);
        return;
    }

    const bool dark = tk::detail::dark();
    AppBackgroundCacheEntry* selected = nullptr;
    AppBackgroundCacheEntry* leastRecentlyUsed = &appBackgroundCaches.front();
    for (auto& entry : appBackgroundCaches)
    {
        if (entry.width == w && entry.height == h && entry.dark == dark
            && ! entry.image.isNull())
        {
            selected = &entry;
            break;
        }
        if (entry.image.isNull() || entry.lastUse < leastRecentlyUsed->lastUse)
            leastRecentlyUsed = &entry;
    }

    if (selected == nullptr)
    {
        selected = leastRecentlyUsed;
        // RGB (non ARGB): il gradiente copre tutto -> opaco, blit senza alpha e
        // 3 byte/pixel invece di 4.
        selected->image = juce::Image (juce::Image::RGB, w, h, true);
        juce::Graphics ig (selected->image);
        renderBg (ig, 0.0f, 0.0f, w, h);
        selected->width = w;
        selected->height = h;
        selected->dark = dark;
    }
    selected->lastUse = ++appBackgroundUseCounter;

    g.drawImageAt (selected->image, bounds.getX(), bounds.getY());
}

void CommonDefaultLookAndFeel::drawAppPanel (juce::Graphics& g, juce::Rectangle<float> r,
                                             bool raised, float corner)
{
    // RICETTA COERENTE (vedi tokens/ayra_ChromePrimitives): un pannello che puo' essere
    // un juce::Component a se' NON usa drop-shadow esterna per il rilievo (melatonin
    // la clippa ai bounds rettangolari -> "piastra" squadrata su tema chiaro). Il
    // raised viene da gradiente + bevel (zero clip, identico light/dark); il
    // recessed da inner-shadow (che rende dentro il path tondo, mai squadrata).
    if (raised) tk::fillRaisedSurface (g, r, corner, tk::surfaceRaised());
    else        fillRecessedSurface  (g, r, corner);
}

void CommonDefaultLookAndFeel::drawAppGlassStrip (juce::Graphics& g, juce::Rectangle<float> r,
                                                  bool hairlineBottom)
{
    juce::ColourGradient grad (tk::surfaceBaseTop(), r.getX(), r.getY(),
                               tk::surfaceBaseBot(), r.getX(), r.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRect (r);
    g.setColour (tk::borderHairline());
    if (hairlineBottom) g.fillRect (r.getX(), r.getBottom() - 1.0f, r.getWidth(), 1.0f);
    else                g.fillRect (r.getX(), r.getY(), r.getWidth(), 1.0f);
}

void CommonDefaultLookAndFeel::drawAppSectionHeader (juce::Graphics& g, juce::Rectangle<int> r,
                                                     const juce::String& text)
{
    g.setColour (tk::textSecondary());
    g.setFont (tk::type::section());
    g.drawText (text.toUpperCase(), r.reduced (2, 0), juce::Justification::centredLeft, true);
    g.setColour (tk::borderHairline().withAlpha (0.7f));
    g.fillRect (r.getX(), r.getBottom() - 1, r.getWidth(), 1);
}

void CommonDefaultLookAndFeel::drawAppIconChip (juce::Graphics& g, juce::Rectangle<float> r, bool active)
{
    juce::Path p; p.addRoundedRectangle (r, tk::radius::control * 0.7f);
    g.setColour (active ? accentSoftOr (tk::accentSoft()) : tk::surfaceRaised());
    g.fillPath (p);
    g.setColour (active ? accentOr (tk::accent()).withAlpha (0.7f) : tk::borderHairline());
    g.strokePath (p, juce::PathStrokeType (1.0f));
}

void CommonDefaultLookAndFeel::drawAppLogoBadge (juce::Graphics& g, juce::Rectangle<float> r)
{
    auto badge = r.withSizeKeepingCentre (juce::jmin (r.getWidth(), r.getHeight()),
                                          juce::jmin (r.getWidth(), r.getHeight()));
    g.setColour (juce::Colours::white);
    g.fillEllipse (badge);
    g.setColour (tk::accent());
    const float er = badge.getWidth() * 0.14f;
    g.fillEllipse (badge.getCentreX() - badge.getWidth() * 0.22f, badge.getCentreY() - er, er, er);
    g.fillEllipse (badge.getCentreX() + badge.getWidth() * 0.08f, badge.getCentreY() - er, er, er);
}

//==============================================================================
void CommonDefaultLookAndFeel::drawAppWindowHeader (juce::Graphics& g, juce::Rectangle<float> r,
                                                    const unsigned char* iconData, size_t iconLen,
                                                    const juce::String& title)
{
    // Fondo: gradiente verticale + hairline bottom, nessun angolo arrotondato
    // (la strip abbraccia l'intera larghezza del pannello host).
    fillFlatBackground (g, r);

    // Icona accent a sinistra + titolo bold.
    auto inner = r.reduced (18.0f, 0.0f);
    const auto gear = inner.removeFromLeft (28.0f).withSizeKeepingCentre (20.0f, 20.0f);
    glyphs::drawGlyph (g, iconData, iconLen, gear, accentOr (tk::accent()));

    inner.removeFromLeft (8.0f);
    g.setColour (tk::textPrimary());
    g.setFont (tk::type::title());
    g.drawText (title, inner.toNearestInt(), juce::Justification::centredLeft);
}

void CommonDefaultLookAndFeel::drawSidebarNavItem (juce::Graphics& g, juce::Rectangle<float> r,
                                                    const unsigned char* iconData, size_t iconLen,
                                                    const juce::String& label,
                                                    bool selected, bool hover)
{
    const float corner = tk::radius::button;

    if (selected)
    {
        // Pill selezionata: fillDarkCard (dark recessed, bordi arrotondati)
        // come base strutturale — garantisce lo stesso vocabolario visivo del
        // pannello sidebar che la contiene. Sopra va il fill accentSoft on-brand.
        fillDarkCard (g, r);
        juce::Path pill;
        pill.addRoundedRectangle (r, corner);
        // Fill CHIARO (soft, translucido) tinto col track colour.
        g.setColour (accentSoftOr (tk::accentSoft()));
        g.fillPath (pill);
        // Bordo ACCENTUATO (crisp) attorno al letto soft: ripristina il contrasto
        // "fill chiaro + bordo accentuato" del design (oltre alla barra a sinistra).
        g.setColour (accentOr (tk::accent()).withAlpha (0.55f));
        g.strokePath (pill, juce::PathStrokeType (1.0f));
        // Barra accent verticale a sinistra: il "crisp" sul letto soft (regola
        // "soft is the bed, crisp is the signal" del design language).
        g.setColour (accentOr (tk::accent()));
        g.fillRoundedRectangle (r.getX() + 2.0f, r.getCentreY() - 8.0f, 3.0f, 16.0f, 1.5f);
    }
    else if (hover)
    {
        g.setColour (tk::surfaceRaised().withAlpha (0.6f));
        g.fillRoundedRectangle (r, corner);
    }

    const auto txtColour = selected ? tk::textPrimary() : tk::textSecondary();

    auto iconArea = r.removeFromLeft (34.0f).withSizeKeepingCentre (16.0f, 16.0f);
    glyphs::drawGlyph (g, iconData, iconLen, iconArea, txtColour);

    g.setColour (txtColour);
    g.setFont (selected ? tk::type::body().boldened() : tk::type::body());
    g.drawText (label, r.reduced (4.0f, 0.0f).toNearestInt(), juce::Justification::centredLeft);
}

void CommonDefaultLookAndFeel::drawSplitPanelBackground (juce::Graphics& g, juce::Rectangle<int> bounds,
                                                          int headerHeight, int sidebarWidth)
{
    // Salta l'area header (gia' disegnata da drawAppWindowHeader).
    auto body = bounds.withTrimmedTop (headerHeight).toFloat();

    // Sidebar sinistra: fillDarkCard (dark recessed, bordi arrotondati).
    // Il padding di 8px crea il margine visivo tra il bordo finestra e il pannello.
    constexpr float kPad = 8.0f;
    fillDarkCard (g, body.removeFromLeft ((float) sidebarWidth).reduced (kPad));

    // Area di contenuto destra: fillLightCard (light raised, bordi arrotondati).
    // Le sezioni attive scrollano sopra questa superficie come pannelli secondari.
    fillLightCard (g, body.reduced (kPad));
}

} // namespace ayra
