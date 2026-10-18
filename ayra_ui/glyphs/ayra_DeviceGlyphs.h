/*
  ==============================================================================

   This file is part of the Ayra ecosystem.
   Copyright (c) Ayra Productions.

   ayra_common / look_and_feel / glyphs / DeviceGlyphs
   ------------------------------------------------------------------
   Glyph del DEVICE MANAGER (header + voci sidebar Audio/MIDI/DMX/OSC/Camera +
   azioni Import/Export). A differenza degli altri file di questa cartella
   (path-data BINARI portati da ayra_Shapes.h), qui i glyph sono memorizzati
   come stringhe SVG `path d="..."` reali e convertiti a runtime in
   `juce::Path` -> blob binario compatibile con `loadPathFromData`, cosi' che
   le ricette LAF esistenti (`drawAppWindowHeader`, `drawSidebarNavItem`), che
   accettano `(const unsigned char*, size_t)`, li disegnino IDENTICI a come
   disegnano il gear del Settings.

   ## Perche' SVG e non blob binari
   I blob binari (`writePathToStream`) si autorano solo eseguendo JUCE offline.
   Le stringhe SVG sono editabili a mano e provengono da icon-set reali: nessuna
   icona "inventata". La conversione `parseSVGPath -> writePathToStream` avviene
   UNA volta per glyph (cache keyed sul puntatore della stringa), message-thread
   only (paint), quindi zero costo a regime.

   ## Provenienza icone (NON inventate)
   Material Design Icons (Pictogrammers), licenza Apache-2.0, viewBox 0 0 24 24,
   single-path fill. Nomi originali:
     deviceManager <- "expansion-card-variant"   (scheda hardware = device I/O)
     audio         <- "volume-high"
     midi          <- "piano"
     dmx           <- "spotlight-beam"            (faro = lighting DMX)
     osc           <- "access-point-network"      (broadcast di rete UDP)
      camera        <- "video"
      feedbackLoop  <- "autorenew"                 (feedback loop hardware / ritorno al controller)
      importAll     <- "import"
      exportAll     <- "export"

   @note JUCE arriva dal root del modulo (ayra_common.h) prima di questo file:
         nessun #include qui (regola moduli Ayra).

  ==============================================================================
*/

#pragma once

namespace ayra::glyphs
{

//==============================================================================
// SVG path-data (attributo `d`) — viewBox 0 0 24 24, fill solido.
//==============================================================================

/// Header "Device Manager": scheda di espansione hardware (= device fisico).
inline constexpr const char* deviceManagerSvg =
    "M2 7H4.5V17H3V8.5H2M22 7V16H14V17H7V16H6V7M10 9H8V12H10M13 9H11V12H13M20 9H15V14H20V9Z";

/// Sidebar AUDIO: altoparlante con onde sonore.
inline constexpr const char* audioSvg =
    "M14,3.23V5.29C16.89,6.15 19,8.83 19,12C19,15.17 16.89,17.84 14,18.7V20.77"
    "C18,19.86 21,16.28 21,12C21,7.72 18,4.14 14,3.23M16.5,12C16.5,10.23 15.5,8.71 14,7.97"
    "V16C15.5,15.29 16.5,13.76 16.5,12M3,9V15H7L12,20V4L7,9H3Z";

/// Sidebar MIDI: tastiera di pianoforte.
inline constexpr const char* midiSvg =
    "M20 2H4C2.9 2 2 2.9 2 4V20C2 21.11 2.9 22 4 22H20C21.11 22 22 21.11 22 20V4"
    "C22 2.9 21.11 2 20 2M14.74 14H15V20H9V14H9.31C9.86 14 10.3 13.56 10.3 13V4H13.75V13"
    "C13.75 13.56 14.19 14 14.74 14M4 4H6.8V13C6.8 13.56 7.24 14 7.79 14H8V20H4V4"
    "M20 20H16V14H16.26C16.81 14 17.25 13.56 17.25 13V4H20V20Z";

/// Sidebar DMX: faro/spotlight (lighting da palco).
inline constexpr const char* dmxSvg =
    "M9,16.5L9.91,15.59L15.13,20.8L14.21,21.71L9,16.5M15.5,10L16.41,9.09L21.63,14.3"
    "L20.71,15.21L15.5,10M6.72,2.72L10.15,6.15L6.15,10.15L2.72,6.72C1.94,5.94 1.94,4.67 2.72,3.89"
    "L3.89,2.72C4.67,1.94 5.94,1.94 6.72,2.72M14.57,7.5L15.28,8.21L8.21,15.28L7.5,14.57"
    "L6.64,11.07L11.07,6.64L14.57,7.5Z";

/// Sidebar OSC: access point con onde broadcast (messaggistica UDP di rete).
inline constexpr const char* oscSvg =
    "M4.93,3.93C3.12,5.74 2,8.24 2,11C2,13.76 3.12,16.26 4.93,18.07L6.34,16.66"
    "C4.89,15.22 4,13.22 4,11C4,8.79 4.89,6.78 6.34,5.34L4.93,3.93M19.07,3.93L17.66,5.34"
    "C19.11,6.78 20,8.79 20,11C20,13.22 19.11,15.22 17.66,16.66L19.07,18.07"
    "C20.88,16.26 22,13.76 22,11C22,8.24 20.88,5.74 19.07,3.93M7.76,6.76"
    "C6.67,7.85 6,9.35 6,11C6,12.65 6.67,14.15 7.76,15.24L9.17,13.83"
    "C8.45,13.11 8,12.11 8,11C8,9.89 8.45,8.89 9.17,8.17L7.76,6.76M16.24,6.76L14.83,8.17"
    "C15.55,8.89 16,9.89 16,11C16,12.11 15.55,13.11 14.83,13.83L16.24,15.24"
    "C17.33,14.15 18,12.65 18,11C18,9.35 17.33,7.85 16.24,6.76M12,9A2,2 0 0,0 10,11"
    "A2,2 0 0,0 12,13A2,2 0 0,0 14,11A2,2 0 0,0 12,9M11,15V19H10A1,1 0 0,0 9,20H2V22H9"
    "A1,1 0 0,0 10,23H14A1,1 0 0,0 15,22H22V20H15A1,1 0 0,0 14,19H13V15H11Z";

/// Sidebar CAMERA: videocamera.
inline constexpr const char* cameraSvg =
    "M17,10.5V7A1,1 0 0,0 16,6H4A1,1 0 0,0 3,7V17A1,1 0 0,0 4,18H16A1,1 0 0,0 17,17V13.5"
    "L21,17.5V6.5L17,10.5Z";

/// Azione IMPORT ALL: freccia che entra nel box.
inline constexpr const char* importAllSvg =
    "M14,12L10,8V11H2V13H10V16M20,18V6C20,4.89 19.1,4 18,4H6A2,2 0 0,0 4,6V9H6V6H18V18H6V15"
    "H4V18A2,2 0 0,0 6,20H18A2,2 0 0,0 20,18Z";

/// Azione EXPORT ALL: freccia che esce dal box.
inline constexpr const char* exportAllSvg =
    "M23,12L19,8V11H10V13H19V16M1,18V6C1,4.89 1.9,4 3,4H15A2,2 0 0,1 17,6V9H15V6H3V18H15V15"
    "H17V18A2,2 0 0,1 15,20H3A2,2 0 0,1 1,18Z";

/// Azione ADD (universo / generico): croce. MDI "plus".
inline constexpr const char* plusSvg =
    "M19,13H13V19H11V13H5V11H11V5H13V11H19V13Z";

/// Azione CONFIG: ingranaggio. MDI "cog".
inline constexpr const char* cogSvg =
    "M12,15.5A3.5,3.5 0 0,1 8.5,12A3.5,3.5 0 0,1 12,8.5A3.5,3.5 0 0,1 15.5,12A3.5,3.5 0 0,1 12,15.5"
    "M19.43,12.97C19.47,12.65 19.5,12.33 19.5,12C19.5,11.67 19.47,11.34 19.43,11L21.54,9.37"
    "C21.73,9.22 21.78,8.95 21.66,8.73L19.66,5.27C19.54,5.05 19.27,4.96 19.05,5.05L16.56,6.05"
    "C16.04,5.66 15.5,5.32 14.87,5.07L14.5,2.42C14.46,2.18 14.25,2 14,2H10C9.75,2 9.54,2.18 9.5,2.42"
    "L9.13,5.07C8.5,5.32 7.96,5.66 7.44,6.05L4.95,5.05C4.73,4.96 4.46,5.05 4.34,5.27L2.34,8.73"
    "C2.21,8.95 2.27,9.22 2.46,9.37L4.57,11C4.53,11.34 4.5,11.67 4.5,12C4.5,12.33 4.53,12.65 4.57,12.97"
    "L2.46,14.63C2.27,14.78 2.21,15.05 2.34,15.27L4.34,18.73C4.46,18.95 4.73,19.03 4.95,18.95"
    "L7.44,17.94C7.96,18.34 8.5,18.68 9.13,18.93L9.5,21.58C9.54,21.82 9.75,22 10,22H14"
    "C14.25,22 14.46,21.82 14.5,21.58L14.87,18.93C15.5,18.67 16.04,18.34 16.56,17.94L19.05,18.95"
    "C19.27,19.03 19.54,18.95 19.66,18.73L21.66,15.27C21.78,15.05 21.73,14.78 21.54,14.63L19.43,12.97Z";

/// Azione REMOVE (device): cestino. MDI "delete".
inline constexpr const char* deleteSvg =
    "M19,4H15.5L14.5,3H9.5L8.5,4H5V6H19M6,19A2,2 0 0,0 8,21H16A2,2 0 0,0 18,19V7H6V19Z";

/// Azione CLOSE/REMOVE (universo / endpoint): croce. MDI "close".
inline constexpr const char* closeSvg =
    "M19,6.41L17.59,5L12,10.59L6.41,5L5,6.41L10.59,12L5,17.59L6.41,19L12,13.41L17.59,19L19,17.59"
    "L13.41,12L19,6.41Z";

/// Feedback loop (hardware feedback / External Map): frecce circolari di ritorno. MDI "autorenew".
inline constexpr const char* feedbackLoopSvg =
    "M12 6v3l4-4-4-4v3c-4.42 0-8 3.58-8 8 0 1.57.46 3.03 1.24 4.26L6.7 14.8c-.45-.8-.7-1.7-.7-2.8 0-3.31 2.69-6 6-6"
    "zm6.76 1.74L17.3 9.2c.44.84.7 1.79.7 2.8 0 3.31-2.69 6-6 6v-3l-4 4 4 4v-3c4.42 0 8-3.58 8-8 0-1.57-.46-3.03-1.24-4.26z";

//==============================================================================
/** Converte una stringa SVG `d` in un blob binario di path-data (formato
    `juce::Path::loadPathFromData`), cache-ato per puntatore. Il blob vive in
    storage statico stabile, quindi `getData()` e' un indirizzo persistente:
    ottimo come chiave anche per la cache di `drawGlyph`.

    @note Primitiva interna: la usa `Glyph::binary()` per servire i consumer
          byte-based partendo dalla sorgente SVG. Il codice applicativo NON la
          chiama direttamente: usa i `glyphs::Glyph` del registry.
    @note Solo message-thread (paint). La parsimonia e' garantita: parsing SVG +
          serializzazione avvengono UNA sola volta per glyph. */
inline const juce::MemoryBlock& svgPathBlob (const char* svg)
{
    struct BlobCache final
    {
        // Inizializza il leak detector JUCE prima che la cache statica registri
        // il proprio distruttore. A shutdown `entries` viene cosi' svuotata
        // prima del controllo globale, evitando falsi leak dei blob residenti.
        juce::MemoryBlock lifetimeAnchor;
        std::unordered_map<juce::int64, juce::MemoryBlock> entries;
    };

    // Chiave = indirizzo del literal SVG (gli `inline constexpr const char*`
    // hanno indirizzo stabile e univoco per glyph).
    // std::unordered_map: una sola lookup (find) su hit e miss, contro il
    // contains()+getReference() del juce::HashMap. Parsing+serializzazione UNA volta.
    static BlobCache cache;
    const auto key = cacheKey (svg);

    auto it = cache.entries.find (key);
    if (it == cache.entries.end())
    {
        const juce::Path path = juce::Drawable::parseSVGPath (juce::String (svg));
        juce::MemoryOutputStream os;
        path.writePathToStream (os);
        it = cache.entries.emplace (key, os.getMemoryBlock()).first;
    }

    return it->second;
}

} // namespace ayra::glyphs
