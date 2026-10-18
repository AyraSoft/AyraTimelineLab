/*
  ==============================================================================

   This file is part of the Ayra ecosystem.
   Copyright (c) Ayra Productions.

   ayra_common / look_and_feel / glyphs / Glyph (HANDLE UNICO)
   ------------------------------------------------------------------
   Tipo unico per i glyph del design system. Astrae le DUE forme sorgente
   storiche dietro un'unica interfaccia, cosi' ogni consumer ne prende UNA sola
   (`const Glyph&`) e non esistono piu' API parallele "byte-array" vs "SVG"
   (origine del bug: una stringa SVG passata a un'API che voleva path-data).

     - SVG string  (`const char*`, path `d`)            -> serializzata on-demand
       in path-data binario via `svgPathBlob` (cache-ata, una volta per glyph).
     - Path-data binario JUCE (`const unsigned char[]`) -> usato tale e quale.

   ## Come si AGGIUNGE un glyph (pattern unico)
   @code
     // 1. dato grezzo (forma comoda all'autore: SVG o byte-array):
     inline constexpr const char* fooSvg = "M12 2L2 7...";
     // 2. handle pubblico (SEMPRE un Glyph, nome pulito senza suffisso):
     inline constexpr Glyph foo { fooSvg };
   @endcode

   ## Come si USA (interfaccia unica)
   @code
     glyphs::drawGlyph (g, glyphs::foo, area, colour);
     ayra::assignShapeToButton (glyphs::foo, shapeButton);
     iconButton.setGlyph (glyphs::foo);
     laf.drawAppWindowHeader (g, r, glyphs::foo, "Title");
   @endcode

   Doctrine: ZERO #include (JUCE arriva dal root del modulo prima di questo file),
   namespace ayra::glyphs, ASCII-only.

  ==============================================================================
*/

#pragma once

namespace ayra::glyphs
{

/** Forward-decl: definito in ayra_DeviceGlyphs.h (incluso piu' avanti nella
    stessa unita' di compilazione). Converte una stringa SVG `d` in path-data
    binario JUCE, cache-ato per puntatore (indirizzo del literal). */
inline const juce::MemoryBlock& svgPathBlob (const char* svg);

/** Chiave di cache da un puntatore (indirizzo stabile del blob/literal glyph).
    Gli array glyph sono `inline constexpr` -> indirizzo univoco e persistente. */
[[nodiscard]] inline juce::int64 cacheKey (const void* p) noexcept
{
    return (juce::int64) (juce::pointer_sized_int) p;
}

/** `juce::Path` parsato da un blob di path-data e cache-ato per indirizzo del
    blob. Il parsing (`loadPathFromData`) e' il costo vero e si ripeterebbe
    identico ad ogni frame: lo eseguiamo una sola volta per blob. Chiave =
    indirizzo del blob (vedi `cacheKey`). Message-thread only (paint) -> nessuna
    sincronizzazione sulla cache. `data == nullptr` -> path vuoto cache-ato. */
[[nodiscard]] inline const juce::Path& cachedPath (const unsigned char* data, size_t numBytes)
{
    // std::unordered_map: una sola lookup (find) sia sul hit che sul miss, contro
    // il contains()+getReference() del juce::HashMap. Semantica invariata:
    // data==nullptr -> path vuoto cache-ato una volta.
    static std::unordered_map<juce::int64, juce::Path> cache;
    const auto key = cacheKey (data);
    auto it = cache.find (key);
    if (it == cache.end())
    {
        juce::Path parsed;
        if (data != nullptr) { parsed.loadPathFromData (data, numBytes); }
        it = cache.emplace (key, std::move (parsed)).first;
    }
    return it->second;
}

//==============================================================================
/** @struct Glyph
 *  @brief Handle UNICO di un glyph: vista uniforme su SVG o path-data binario.
 *
 *  E' un valore leggero (3 puntatori) costruibile a compile-time da una stringa
 *  SVG o da un array `*Path` legacy. Espone `binary()` (path-data stabile per i
 *  consumer byte-based) e `path()` (juce::Path fittabile). Entrambi cache-ati. */
struct Glyph
{
    /** Glyph vuoto (nessuna sorgente): `isValid()` falso, disegno no-op. Utile come
        stato di default di un widget prima di ricevere un glyph reale. */
    constexpr Glyph() noexcept = default;

    /** Da stringa SVG (path `d`). */
    constexpr Glyph (const char* svgPathD) noexcept : svg (svgPathD) {}

    /** Da path-data binario JUCE (array legacy `*Path`, deduce la dimensione). */
    template <size_t N>
    constexpr Glyph (const unsigned char (&binaryPathData)[N]) noexcept
        : bin (binaryPathData), binBytes (N) {}

    //==========================================================================
    /** Ha una sorgente (SVG o binaria)? Un Glyph default-costruito e' invalido. */
    [[nodiscard]] constexpr bool isValid() const noexcept { return svg != nullptr || bin != nullptr; }

    /** Vista binaria stabile (data + size) in formato `juce::Path::loadPathFromData`.
        SVG -> blob serializzato cache-ato; legacy -> l'array stesso. L'indirizzo
        e' persistente, quindi usabile anche come chiave di cache nei consumer.
        Glyph vuoto -> `{nullptr, 0}`. */
    struct Binary { const unsigned char* data; size_t size; };

    [[nodiscard]] Binary binary() const
    {
        if (bin != nullptr) { return { bin, binBytes }; }
        if (svg == nullptr) { return { nullptr, 0 }; }
        const auto& blob = svgPathBlob (svg);
        return { static_cast<const unsigned char*> (blob.getData()), blob.getSize() };
    }

    /** `juce::Path` parsato e cache-ato (per indirizzo sorgente). Message-thread
        only (nessuna sincronizzazione sulla cache). Glyph vuoto -> path vuoto. */
    [[nodiscard]] const juce::Path& path() const
    {
        const auto b = binary();
        return cachedPath (b.data, b.size);
    }

    //==========================================================================
    const char*          svg      = nullptr;   ///< sorgente SVG (nullptr se binario).
    const unsigned char* bin      = nullptr;   ///< sorgente binaria (nullptr se SVG).
    size_t               binBytes = 0;         ///< dimensione di `bin`.
};

} // namespace ayra::glyphs
