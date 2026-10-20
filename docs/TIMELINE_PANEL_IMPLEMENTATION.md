# Timeline senza header — implementazione e integrazione nativa

Questa iterazione sostituisce il separatore-header della vista libera, non lo
riduce ulteriormente. Le lane contengono canvas a tutta altezza, controlli laterali,
accenti e nome sovrapposto. Il modulo legge direttamente il provider `ayra_timeline`.
Nessuna modifica a `ayra_multiverse` è inclusa nella consegna.

## File e responsabilità

| File | Responsabilità |
|---|---|
| `modules/ayra_timeline_panel/TimelineModel.h` | Proiezione, ID, selezione lane, ordine/hide, politiche altezza, richieste con revisione |
| `modules/ayra_timeline_panel/TimelinePanel.h` | Componenti residenti limitati, composizione senza header, input e menu visibilità |
| `app/CanvasViewportBinding.h` | Conversione viewport condivisa per paint e input degli adattatori legacy |
| `demo/PanelClipWindow.h` | Proiezione fixture delle sole clip visibili tramite indice canonico |
| `demo/TimelinePanelDemo.cpp` | App a 19 lane, collegamento canvas, cache stati, adattatore artwork Ayra e meter sintetici |
| `demo/PanelFixture.h` | Owner e transazioni del solo progetto dimostrativo |
| `app/clips/*/*Canvas.cpp` | Adattamento viewport, conservazione stato e navigazione degli editor esistenti |
| `tests/NativePanelTests.cpp` | Test del pannello JUCE reale con canvas strumentati |

I painter delle primitive clip, i loro componenti e le formule delle curve restano
quelli esistenti. I sei adattatori Canvas cambiano perché le loro coordinate fisse
`width / 32` non possono implementare uno scroll temporale reale. Ogni conversione
assoluta coinvolta usa ora il binding condiviso; la modalità `--legacy` conserva
la convenzione precedente.

## Capability del progetto di esempio

| Lane | Owner controls | Meter opzionali | Altezza |
|---|---|---|---|
| Audio / MIDI / Image / Parameter di traccia | M/S, Open, Monitoring condivisi | In/out tramite capability, esempio 2+2 | Manuale, limiti del canvas |
| DMX / OSC / Text di traccia | Gli stessi controlli dell'owner | Possibili, non esclusi dal tipo | Configurabile dal contratto |
| Automazione di MIX | Controlli del vero MIX | Possibili | Secondo la capability |
| Song Main | Mute, Open, Monitoring; niente Solo | Possibili | Manuale nell'esempio audio |
| Marker / Metrica / Counter | Nessun falso controllo di traccia; navigazione | Nessun readback configurato nella fixture | Compatta 24 px |
| Range | Navigazione degli eventi della fixture | Nessun readback configurato | Compatta 28 px |
| Tempo | Navigazione dei punti | Nessun readback configurato | Manuale nell'esempio |
| Lane ad autocalibrazione | Determinati dall'host | Determinati dall'host | Callback di misura, senza resize utente |

Questa matrice descrive la fixture, non una nuova tabella rigida dei tipi di
Multiverse. In particolare il modulo permette meter su ogni tipo quando forniti
dall'host. Il test nativo esercita esplicitamente anche l'altezza automatica.

## Grafica riutilizzata

La demo usa i riferimenti Ayra già inclusi in `ayra_ui/`: `ShapeIconButton`, glyph
rewind/window/audio, `drawFlagToolbarWell`, `drawAccentSpine` e `drawVMeter`.
Il normale Well e il disegno dei meter non vengono ricopiati. M/S sono lettere
convenzionali sul medesimo Well. Il gradiente del nome è la nuova composizione
richiesta, alimentata dalla palette del LookAndFeel. L'integrazione futura sostituisce
l'adattatore, non il significato dei comandi.

## Comportamenti

Nome sempre presente sinistra/destra; accento sempre presente sinistra/destra/entrambi.
Gli altri controlli sono nascondibili. Solo il rail seleziona/riordina lane.
Il nome lascia passare primo click, drag, rotella e menu; il doppio click sul testo
apre un editor con controllo di sessione/revisione. Il bordo manuale offre preview,
Escape, conferma/Undo. I widget automatici comunicano l'altezza, non la ricavano da
un tipo di header. Il glyph inferiore è un indicatore passivo.

La residenza grafica usa il provider, con cache idle limitata e lease per gesti/editor.
La cache degli stati degli editor è distinta: rimane legata agli ID canonici e
non viene cancellata soltanto perché una clip o una lane è fuori schermo.
La demo conserva punti/note/warp mentre i componenti vengono smontati.

## Limiti ancora aperti

- I meter sono un collaudo visivo sintetico, non DSP. Open apre un inspector fixture.
- La nuova demo è in memoria. Non esiste un export/import completo del progetto
  nuovo; il JSON v1 resta del vecchio banco.
- Gruppo temporale è relazione reversibile, non unico contenitore visuale o Glue PCM.
  I link supportano le transazioni fixture documentate, non ogni split/stretch
  source-aware cross-owner. I comandi non supportati non fingono un risultato.
- Point/Span/Continuous hanno navigazione reale dei loro eventi locali e stato
  mantenuto fra smontaggi; non hanno acquisito l'Undo/persistenza documentale canonica.
- Il progetto fittizio non replica i renderer/contratti musicali completi delle
  corsie globali di Multiverse (Range e Actions inclusi).
- Il riordino supporta lo scroll sui movimenti del mouse vicino al bordo; lo scroll
  continuo con puntatore fermo sul bordo non è ancora un contratto implementato.
- Struttura/gruppi gerarchici conservano la precedente UI di confronto; la nuova
  composizione validata è quella a lane separate.
- Non sono stati eseguiti build macOS/Windows, renderer hardware, native sanitizer
  completi né benchmark con migliaia di canvas reali popolati e meter DSP.

## Build e dipendenze

Vedere il README root. Il link a `juce::juce_audio_basics` risolve le operazioni
vettoriali richieste da Melatonin. Non si sostituisce il codice del provider per
nascondere un errore di linking. `run.command` rispetta i percorsi configurati,
riconosce l'artefatto nella sottocartella Debug/Release e mantiene le entry `--legacy`.
La fixture JSON è stata riallineata al C++ già più recente senza cambiare i byte
originali di `generated/FixtureData.cpp`; l'HTML legacy è stato rigenerato.

L'evidenza eseguita si trova in `verification/no-header/STATUS.md`.
