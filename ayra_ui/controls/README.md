# widgets/controls

**Ruolo**: controllo **atomico** riusabile — un solo valore o una sola azione,
nessuna logica d'area. Massimo riuso => candidati a promozione in `ayra_gui`.
Una sotto-cartella per controllo (Widget Pattern Ayra). Vedi [`../README.md`](../README.md).

| Cartella | Classe | Ruolo |
|---|---|---|
| `dialogs/` | `ayra::show*Dialog` | dialog modali stilati (alert/conferma/text-entry), LAF-aware |
| `shape_icon_button/` | `ayra::ShapeIconButton` | bottone-icona da `juce::Path` (glyph del design system) |
| `add_button/` | `ayra::AddButton` | comando `+` canonico; il consumer definisce solo tooltip e azione |
| `app_window_header/` | `ayra::AppWindowHeader` | chrome canonica per superfici secondarie: icona, titolo e azioni |
| `tab_strip/` | `ayra::TabStrip` | tab strip riusabile |
| `colour_button/` | `ayra::ColourButton` | picker colore condiviso (CallOutBox) |
| `search_bar/` | `ayra::SearchBar` | barra di ricerca (Frosted Lab) |
| `mix_scope_strip/` | `ayra::MixScopeStrip` | channelstrip comune a Mixer/Project/Group: geometria e gesti unici, Pan adattivo none/scalar/XY, bank meter N-channel a dimensione preferita, Graph e slot accessori/cue |
| `macro_slot/` | `ayra::MacroSlotComponent` | slot macro (knob rotary + label) |
| `bpm_knob/` | `ayra::BpmKnob` | knob BPM |
| `time_signature/` | `ayra::TimeSignatureControl` | controllo metro |
| `drag_value_field/` | `ayra::DragValueField` | slider implicito: drag (Shift=snap intero) + doppio-click type; range o lista di valori ammessi |
| `transport_clock/` | `ayra::TransportClock` | display di trasporto |

`MixScopeStrip::HeaderStyle::bus` distingue graficamente una somma: cap
incassato e simbolo vettoriale di somma, senza alterare la geometria dei
controlli. Il consumer sceglie lo stile; il LookAndFeel lo legge tramite
`getHeaderStyle()` e personalizza la ricetta `drawMixScopeStripHeader` o i
ColourIds `busHeaderBackground/Text/BorderColourId`. Nessuno stato di routing
o di mix viene dedotto dall'aspetto.
