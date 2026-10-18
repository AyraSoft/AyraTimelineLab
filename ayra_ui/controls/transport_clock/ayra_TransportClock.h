/*
  ==============================================================================

   This file is part of the Ayra ecosystem.
   Copyright (c) Ayra Productions.

   ayra_common / widgets / controls / TransportClock
   ------------------------------------------------------------------
   Display di posizione del trasporto per la footer: legge la posizione corrente
   da una SORGENTE pull e la mostra nel formato scelto dall'utente, con una
   striscia di "beat dots" che pulsa col beat in corso (il tempo che scorre).

   ## Perche' esiste / cosa risolve
   La footer aveva play/stop/rewind/bpm ma NESSUN feedback di posizione. Questo
   widget aggiunge il "clock" canonico di una DAW: battuta corrente + beat, oppure
   millisecondi / timecode SMPTE / samples / frames, scelti col tasto destro.

   ## Decoupled (doctrine)
   Non conosce il transport: si attacca a `std::function<Position()>` che ritorna
   uno snapshot della posizione (l'app la lega a `TransportController`). beatsPerBar
   / sampleRate / fps sono CONFIG (cambiano di rado): setter dedicati, non nel pull.

   ## Costo CPU ~0 (RT-safe sul lato lettura)
   Un solo render-timer (default 30 Hz) legge la sorgente (load atomici lock-free)
   e ridipinge SOLO se cambia qualcosa di visibile (stringa / beat / fase). Da fermo
   non ridisegna. La visibilita' effettiva governa il lifecycle: a widget nascosto
   il timer e' fermo e riparte con riallineamento immediato al reveal.

   ## Uso
   @code
   ayra::TransportClock clock;
   clock.attachSource ([&t] { return ayra::TransportClock::Position {
       t.isPlaying(), t.getPositionBarsBeats(), t.getPositionSeconds() }; });
   clock.setBeatsPerBar (4);
   clock.onFormatChanged = [this] (auto f) { settings.setClockFormat ((int) f); };
   @endcode

   ## Thread
   Solo message thread (GUI). La sorgente e' invocata dal render-timer sul message
   thread: deve essere lock-free sul lato lettura (i getter del transport lo sono).
  ==============================================================================
*/

#pragma once

namespace ayra
{

class TransportClock : public juce::Component,
                       public juce::SettableTooltipClient,
                       private juce::Timer
{
public:
   //==========================================================================
   /// Colori disegnabili (range widget condivisi `0x3b000xx`). I default sono
   /// risolti live dai token/LAF; un owner puo' sovrascriverli per-istanza.
   enum ColourIds
   {
      backgroundColourId = 0x3b00070, ///< Sfondo del display (recessed).
      borderColourId     = 0x3b00071, ///< Hairline del display.
      valueColourId      = 0x3b00072, ///< Numerici (SF Mono).
      labelColourId      = 0x3b00073, ///< Etichetta formato (caption).
      beatColourId       = 0x3b00074, ///< Beat dot non attivo.
      beatActiveColourId = 0x3b00075  ///< Beat dot del beat corrente / in play.
   };

   //==========================================================================
   /// Formati di visualizzazione (scelti col tasto destro).
   enum class Format
   {
      barsBeats = 0,   ///< Battuta : Beat : Tick (musicale).
      milliseconds,    ///< mm:ss.mmm (hh:mm:ss.mmm oltre l'ora).
      smpte,           ///< HH:MM:SS:FF (timecode, usa fps).
      samples,         ///< Conteggio sample assoluto.
      frames           ///< Conteggio frame assoluto (usa fps).
   };

   //==========================================================================
   /// Snapshot di posizione PULLATO ogni frame dalla sorgente (POD, copia cheap).
   /// bar/beat sono 1-based (come `ayra::BarsBeats`).
   struct Position
   {
      bool   playing = false;
      int    bar     = 1;
      int    beat    = 1;
      int    tick    = 0;     ///< 0..ticksPerBeat-1 (per la fase del beat dot).
      double seconds = 0.0;
      bool musicalPositionAvailable = true;
      bool secondsAvailable = true;
      bool samplePositionAvailable = false;
      juce::int64 samples = 0;
   };

   //==========================================================================
   /// Listener formale (oltre alla `std::function` inline). Notificato al cambio
   /// di formato (utile per persistere la scelta nelle settings).
   class Listener
   {
   public:
      virtual ~Listener() = default;
      virtual void transportClockFormatChanged (TransportClock*, Format) = 0;
   };

   //==========================================================================
   TransportClock();
   ~TransportClock() override;

   //==========================================================================
   // Sorgente dei dati (pull).

   /** Attacca la sorgente di posizione e avvia il render-timer. */
   void attachSource (std::function<Position()> newSource, int refreshHz = 30);

   /** Stacca la sorgente e ferma il timer (l'ultimo valore resta mostrato). */
   void detachSource();
   /** Pull una tantum, anche da nascosto. Nessuna posizione viene inventata
       quando il provider dichiara indisponibile il formato selezionato. */
   void refreshPosition();
   [[nodiscard]] juce::String getDisplayedText() const { return lastText; }

   //==========================================================================
   // Config (event-driven, cambiano di rado).

   void setBeatsPerBar (int beatsPerBar) noexcept;      ///< Numero di beat dot.
   void setTicksPerBeat (int ticksPerBeat) noexcept;    ///< Per la fase del dot (default 960).
   void setSampleRate (double sampleRate) noexcept;     ///< Per il formato "samples".
   void setFps (double fps) noexcept;                   ///< Per SMPTE / frames.

   //==========================================================================
   // Formato.

   void setFormat (Format f, juce::NotificationType notify = juce::sendNotificationSync);
   [[nodiscard]] Format getFormat() const noexcept { return format; }

   /** Popola `menu` con le 5 voci di formato (id = (int)format + 1), spuntando
       `current`. Builder CONDIVISO: lo usa il click sul clock e chi vuole
       replicare lo STESSO menu altrove (es. right-click sul ruler della
       timeline) senza duplicare le stringhe. Sul risultato `r > 0` il chiamante
       applica `Format (r - 1)`. */
   static void addFormatMenuItems (juce::PopupMenu& menu, Format current);

   //==========================================================================
   void addListener (Listener*);
   void removeListener (Listener*);
   std::function<void (Format)> onFormatChanged;

   //==========================================================================
   void paint (juce::Graphics&) override;
   void mouseDown (const juce::MouseEvent&) override;   ///< click -> menu formato.

private:
   //==========================================================================
   void timerCallback() override;
   void updateTimerState();
   void showFormatMenu();
   [[nodiscard]] Format effectiveFormat (const Position&) const noexcept;
   [[nodiscard]] juce::String formatValue (const Position&) const;
   [[nodiscard]] static const char* formatLabel (Format) noexcept;

   //==========================================================================
   ShowingStateWatcher showingWatcher;
   std::function<Position()> source;
   Position last;                 ///< ultimo snapshot mostrato (per repaint-on-change).
   juce::String lastText;         ///< ultima stringa mostrata (per repaint-on-change).

   Format format        = Format::barsBeats;
   int    beatsPerBar   = 4;
   int    ticksPerBeat  = 960;
   double sampleRate    = 48000.0;
   double fps           = 30.0;
   int    refreshRateHz  = 30;

   juce::ListenerList<Listener> listeners;

   JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TransportClock)
};

} // namespace ayra
