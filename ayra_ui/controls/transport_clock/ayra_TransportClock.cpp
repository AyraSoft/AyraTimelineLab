/*
  ==============================================================================

   This file is part of the Ayra ecosystem.
   Copyright (c) Ayra Productions.

   ayra_common / widgets / controls / TransportClock (impl)
   ------------------------------------------------------------------
   Regola moduli Ayra: questo sub-.cpp NON include nulla (ne' il proprio header).
   Tipi, token (`ayra::tokens`) e glyph (`ayra::glyphs`) sono gia' visibili via
   ayra_common.h.
  ==============================================================================
*/

namespace ayra
{

//==============================================================================
/// Tabella unica del formato: nome lungo (voce di menu) + nome corto (etichetta
/// del display) per ogni `Format`. Sorgente di verita' enumerativa consumata sia
/// da `showFormatMenu` (voci) sia da `formatLabel` (caption): aggiungere un
/// formato = una riga qui (la sola logica per-caso che resta esplicita e' in
/// `formatValue`, che differisce sostanzialmente per ramo).
namespace
{
   struct FormatEntry
   {
      TransportClock::Format format;
      const char*            longName;
      const char*            shortName;
   };

   constexpr FormatEntry kFormatTable[] =
   {
      { TransportClock::Format::barsBeats,    "Bars : Beats : Ticks", "BARS"  },
      { TransportClock::Format::milliseconds, "Milliseconds",         "MS"    },
      { TransportClock::Format::smpte,        "Timecode (SMPTE)",     "SMPTE" },
      { TransportClock::Format::samples,      "Samples",              "SMPL"  },
      { TransportClock::Format::frames,       "Frames",               "FRM"   },
   };
}

//==============================================================================
TransportClock::TransportClock()
   : showingWatcher (*this, [this] (bool) { updateTimerState(); })
{
   setInterceptsMouseClicks (true, false);   // click -> menu formato (unica interazione).
   setMouseCursor (juce::MouseCursor::PointingHandCursor);
   setTooltip ("Click to change the time display format");
}

TransportClock::~TransportClock()
{
   stopTimer();
}

//==============================================================================
void TransportClock::attachSource (std::function<Position()> newSource, int refreshHz)
{
   source = std::move (newSource);
   refreshRateHz = juce::jmax (1, refreshHz);
   refreshPosition();
   updateTimerState();
}

void TransportClock::detachSource()
{
   source = nullptr;
   stopTimer();
}

void TransportClock::updateTimerState()
{
   const bool shouldRun = source != nullptr && isShowing();
   if (! shouldRun)
   {
      stopTimer();
      return;
   }

   if (! isTimerRunning())
   {
      startTimerHz (refreshRateHz);
      timerCallback(); // riallinea subito il display al reveal.
   }
}

//==============================================================================
void TransportClock::setBeatsPerBar (int n) noexcept
{
   const int next = juce::jlimit (1, 32, n);
   if (beatsPerBar == next) { return; }
   beatsPerBar = next;
   repaint();
}
void TransportClock::setTicksPerBeat (int t) noexcept
{
   const int next = juce::jmax (1, t);
   if (ticksPerBeat == next) { return; }
   ticksPerBeat = next;
   repaint();
}
void TransportClock::setSampleRate (double sr) noexcept
{
   if (! std::isfinite (sr) || sr <= 0.0 || juce::approximatelyEqual (sampleRate, sr)) { return; }
   sampleRate = sr;
   if (format == Format::samples) { lastText = formatValue (last); repaint(); }
}
void TransportClock::setFps (double f) noexcept
{
   if (! std::isfinite (f) || f <= 0.0 || juce::approximatelyEqual (fps, f)) { return; }
   fps = f;
   if (format == Format::smpte || format == Format::frames)
   {
      lastText = formatValue (last);
      repaint();
   }
}

//==============================================================================
void TransportClock::setFormat (Format f, juce::NotificationType notify)
{
   if (f == format) { return; }

   format   = f;
   lastText = formatValue (last);   // riformatta subito (anche da fermo).
   repaint();

   if (notify != juce::dontSendNotification)
   {
      listeners.call ([this, f] (Listener& l) { l.transportClockFormatChanged (this, f); });
      if (onFormatChanged) { onFormatChanged (f); }
   }
}

//==============================================================================
void TransportClock::addListener (Listener* l)    { listeners.add (l); }
void TransportClock::removeListener (Listener* l) { listeners.remove (l); }

//==============================================================================
void TransportClock::timerCallback()
{
   if (source == nullptr || ! isShowing())
   {
      updateTimerState();
      return;
   }

   refreshPosition();
}

void TransportClock::refreshPosition()
{
   if (source == nullptr) { return; }
   const Position     p = source();
   const juce::String t = formatValue (p);

   // Cambiamento visibile = stringa diversa, oppure play/stop, oppure beat diverso
   // (la transizione play->stop e' gia' coperta dal confronto su `playing`).
   const bool visibleChange = (t != lastText) || (p.playing != last.playing)
       || (p.beat != last.beat)
       || (p.musicalPositionAvailable != last.musicalPositionAvailable);
   last = p;

   // Mentre suona ridipinge a frame-rate (anima i beat dot + lo sweep). Da fermo
   // ridipinge SOLO su un cambiamento visibile (seek / bpm / cambio formato).
   if ((p.playing && p.musicalPositionAvailable) || visibleChange)
   {
      lastText = t;
      repaint();
   }
}

//==============================================================================
void TransportClock::mouseDown (const juce::MouseEvent&)
{
   showFormatMenu();   // qualsiasi click apre il menu formato (display puro).
}

void TransportClock::addFormatMenuItems (juce::PopupMenu& menu, Format current)
{
   for (const auto& it : kFormatTable)
      menu.addItem ((int) it.format + 1, it.longName, true, it.format == current);
}

void TransportClock::showFormatMenu()
{
   juce::PopupMenu m;
   addFormatMenuItems (m, format);

   // Un popup TOP-LEVEL JUCE NON eredita il LAF dal `targetComponent`: usa il LAF
   // del PopupMenu stesso (`menu.lookAndFeel`) e, se nullo, cade sul default ->
   // grafica JUCE grezza. Lo settiamo esplicitamente col LAF che il widget eredita
   // dall'editor (l'app LAF "frosted"), cosi' il menu e' stilato. Il widget resta
   // module-safe: nessuna dipendenza dal LAF d'app, solo `getLookAndFeel()`.
   m.setLookAndFeel (&getLookAndFeel());

   juce::Component::SafePointer<TransportClock> safe (this);
   m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                    [safe] (int r)
                    {
                       if (safe != nullptr && r > 0) { safe->setFormat ((Format) (r - 1)); }
                    });
}

//==============================================================================
TransportClock::Format TransportClock::effectiveFormat (const Position& p) const noexcept
{
   // Alcuni wrapper host pubblicano una timeline assoluta valida ma omettono il
   // bar-count necessario a una battuta esatta. Non inventiamo la metrica: finche'
   // manca, la selezione Bars mostra la coordinata host esatta disponibile:
   // millisecondi, oppure sample assoluti se l'host omette anche i secondi.
   if (format == Format::barsBeats && ! p.musicalPositionAvailable)
   {
      if (p.secondsAvailable && std::isfinite (p.seconds)) { return Format::milliseconds; }
      if (p.samplePositionAvailable) { return Format::samples; }
   }

   return format;
}

juce::String TransportClock::formatValue (const Position& p) const
{
   const auto displayFormat = effectiveFormat (p);
   if (displayFormat == Format::barsBeats)
      return p.musicalPositionAvailable
          ? juce::String (p.bar) + ":" + juce::String (p.beat) + ":"
              + juce::String (juce::jlimit (0, 999, p.tick)).paddedLeft ('0', 3)
          : juce::String ("--");

   if (displayFormat == Format::samples && p.samplePositionAvailable)
      return juce::String (p.samples);

   if (! p.secondsAvailable || ! std::isfinite (p.seconds)) { return "--"; }
   const auto sign = p.seconds < 0.0 ? juce::String ("-") : juce::String();
   const double magnitude = std::abs (p.seconds);
   // Il confronto stretto evita il cast UB al limite 2^63, che un double
   // rappresenta ma int64 no. Pre-roll negativo e sessioni lunghe restano validi.
   auto count = [magnitude] (double scale, bool round) -> juce::Optional<juce::int64>
   {
      const double scaled = magnitude * scale;
      const double result = round ? std::floor (scaled + 0.5) : std::floor (scaled);
      if (! std::isfinite (result) || result < 0.0
          || result >= static_cast<double> (std::numeric_limits<juce::int64>::max()))
         return {};
      return static_cast<juce::int64> (result);
   };

   switch (displayFormat)
   {
      case Format::milliseconds:
      {
         const auto total = count (1000.0, true);
         if (! total) { return "--"; }
         const auto hours = *total / 3600000;
         const auto mmss = juce::String ((*total / 60000) % 60).paddedLeft ('0', 2) + ":"
                         + juce::String ((*total / 1000) % 60).paddedLeft ('0', 2) + "."
                         + juce::String (*total % 1000).paddedLeft ('0', 3);
         return sign + (hours > 0 ? juce::String (hours).paddedLeft ('0', 2) + ":" + mmss : mmss);
      }
      case Format::smpte:
      {
         const auto total = count (1.0, false);
         const double frameValue = (magnitude - std::floor (magnitude)) * fps;
         if (! total || ! std::isfinite (frameValue)
             || frameValue >= static_cast<double> (std::numeric_limits<juce::int64>::max()))
            return "--";
         return sign + juce::String (*total / 3600).paddedLeft ('0', 2) + ":"
              + juce::String ((*total / 60) % 60).paddedLeft ('0', 2) + ":"
              + juce::String (*total % 60).paddedLeft ('0', 2) + ":"
              + juce::String (static_cast<juce::int64> (frameValue)).paddedLeft ('0', 2);
      }
      case Format::samples:
      case Format::frames:
      {
         const auto total = count (displayFormat == Format::samples ? sampleRate : fps,
                                   displayFormat == Format::samples);
         return total ? sign + juce::String (*total) : juce::String ("--");
      }
      case Format::barsBeats: break;
   }
   return "--";
}

const char* TransportClock::formatLabel (Format f) noexcept
{
   for (const auto& it : kFormatTable)
      if (it.format == f)
         return it.shortName;
   return "";
}

//==============================================================================
void TransportClock::paint (juce::Graphics& g)
{
   const auto area = getLocalBounds().toFloat();

   // Display "inset" (recessed) con hairline: legge come un readout d'uno strumento.
   g.setColour (tokens::componentColour (*this, backgroundColourId, tokens::surfaceRecessed()));
   g.fillRoundedRectangle (area, 4.0f);
   g.setColour (tokens::componentColour (*this, borderColourId, tokens::borderHairline()));
   g.drawRoundedRectangle (area.reduced (0.5f), 4.0f, 1.0f);

   auto inner = area.reduced (7.0f, 4.0f);

   // --- Striscia beat-dot (in basso): l'animazione del tempo che scorre. ---
   auto dotsStrip = inner.removeFromBottom (6.0f);
   inner.removeFromBottom (2.0f);   // gap

   // --- Riga valore + etichetta formato (compatta a destra). ---
   auto labelArea = inner.removeFromRight (26.0f);
   g.setColour (tokens::componentColour (*this, labelColourId, tokens::textSecondary()));
   g.setFont (tokens::type::caption().withHeight (8.0f));
   g.drawText (formatLabel (effectiveFormat (last)), labelArea.toNearestInt(),
               juce::Justification::centredRight, false);

   // Valore tabular (SF Mono), ridotto un filo per far stare il timecode SMPTE.
   g.setColour (tokens::componentColour (*this, valueColourId, tokens::textValue()));
   g.setFont (tokens::type::value().withHeight (11.0f));
   g.drawText (lastText, inner.toNearestInt(), juce::Justification::centredLeft, false);

   if (! last.musicalPositionAvailable) { return; }

   // --- Beat dots: beat corrente evidenziato; mentre suona pulsa col tick. ---
   const int   n      = juce::jmax (1, beatsPerBar);
   const float dotD   = juce::jmin (5.0f, dotsStrip.getHeight());
   const float phase  = (float) juce::jlimit (0, ticksPerBeat, last.tick) / (float) ticksPerBeat;
   const auto  idle   = tokens::componentColour (*this, beatColourId, tokens::textDisabled());
   const auto  active = getCommonLAF (*this).accentOr (tokens::componentColour (*this, beatActiveColourId, tokens::accent()));

   for (int i = 0; i < n; ++i)
   {
      const float cx = dotsStrip.getX() + ((float) i + 0.5f) / (float) n * dotsStrip.getWidth();
      const bool  isCurrent = (i == last.beat - 1);

      juce::Colour c = idle;
      float d = dotD * 0.7f;
      if (isCurrent)
      {
         // Pulse: pieno a inizio beat, sfuma verso il successivo.
         c = active.withMultipliedAlpha (last.playing ? (1.0f - 0.45f * phase) : 1.0f);
         d = dotD;
      }
      g.setColour (c);
      g.fillEllipse (cx - d * 0.5f, dotsStrip.getCentreY() - d * 0.5f, d, d);
   }

   // Sweep: sottile tacca che attraversa la battuta (posizione assoluta nel bar).
   if (last.playing)
   {
      const float barFrac = juce::jlimit (0.0f, 1.0f, ((float) (last.beat - 1) + phase) / (float) n);
      const float sx = dotsStrip.getX() + barFrac * dotsStrip.getWidth();
      g.setColour (active.withAlpha (0.55f));
      g.fillRect (sx - 0.5f, dotsStrip.getY() - 1.0f, 1.0f, dotsStrip.getHeight() + 2.0f);
   }
}

} // namespace ayra
