# Timeline senza track header — implementazione sperimentale

Base Lab: `4fadddde7631087a4466519a8b698ab30dfe3c2d`.
Il lavoro e' isolato in un branch, senza modifiche a `main`, ai provider privati o a
`ayra_multiverse`. Le clip approvate in `app/clips/` non sono state riscritte.

## Avvio

Il nuovo `AyraTimelineLab` apre la demo di pannello. `--legacy` apre il banco clip
precedente; `--session "file.json"` usa intenzionalmente il vecchio banco/codec v1.
La nuova metadata non viene esportata col vecchio JSON: la demo non espone un falso
salvataggio che perderebbe gruppi, link, owner e colori. Il progetto demo e' in memoria.

```sh
cmake -S . -B build/panel-native \
  -DJUCE_SOURCE_DIR="/percorso/JUCE" \
  -DAYRA_MODULES_DIR="/percorso/moduli-ayra" \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build build/panel-native --target AyraTimelineLab --config Debug
```

Servono JUCE 8, il modulo locale `ayra_curves`, gli altri prerequisiti gia' richiesti
dal Lab e i submodule del checkout. Il percorso personale hardcoded dei moduli e'
rimosso: passare `AYRA_MODULES_DIR` o impostare la variabile d'ambiente omonima.
Nessun download implicito. La build GUI di questo branch **NON E' STATA ESEGUITA**.

## Banco dimostrativo

Una Song, due Performer, tracce Song-owned, Voice, Keys, Guitar e Lights, due MIX e
Song Main. Diciassette lane con Audio, MIDI a 96/256/400 px, Image, Parameter,
Point/Span Keyframe e Continuous Line; una lane Audio vuota e un MIX senza lane.
Sono precaricati un gruppo temporale di due clip e un link Audio/MIDI/Image.

Selezione lane: accent sinistro, Cmd/Ctrl additivo, Shift intervallo. Trascinare
l'accent per il riordino. `Gruppo lane` raggruppa le unita' appropriate alla modalita';
`Sciogli lane` le separa. `Undo vista` e `Redo vista` non toccano il documento.

Usare `Clip...` o il menu contestuale per selezionare clip per ID anche cross-lane;
`Group tempo` / `Dissolve` gestiscono il gruppo temporale, `Link clip` / `Unlink`
il link verticale. La selezione esplicita e' elencata nello stato in basso. Gli edit
strutturali supportati dai canvas entrano nel medesimo Store e nel preflight di
propagazione; `Undo edit` annulla una transazione. Un membro bloccato o un candidato
invalido rifiuta l'operazione completa.

M/S, Monitoring e Open riguardano sempre il vero owner della lane. Open mostra un
inspector fixture, non un finto Tensor engine. `Colori tipo` modifica la palette
fixture senza alterare l'accento della traccia. La ricerca rivela owner/lane senza
riordinare. Warp/Gain pilotano le feature gia' presenti nei canvas.

## Limiti concreti di questa consegna

| Area | Stato |
|---|---|
| Controller C++ e transazioni fixture | Implementati, test eseguiti Debug/Release/Clang ASan+UBSan |
| Superficie JUCE e app dimostrativa | Sorgenti collegati in CMake, compilazione/avvio/gesture NOT_RUN |
| Paint e gesture clip gia' approvati | File originali invariati; nessuna nuova certificazione native |
| Point/Span Keyframe e Continuous Line | Mantengono editing locale originale; navigazione e comandi documentali disabilitati, non collegati a clip fittizie |
| Gruppo temporale | Relazione reversibile stessa lane; non crea ancora un unico container visuale editabile |
| Glue PCM/Bounce/Dissolve canonico | NON implementati dal banco; usare gli owner e i renderer canonici |
| Link verticali | Riferimenti espliciti a clip, propagazione atomica di geometria/delete nel modello fixture |
| Split/stretch sorgente-aware di un link | Non generalizzati; split collegato rifiutato; stati sorgente locali dei canvas non uniformati |
| Monitoring | Una fonte per traccia nel banco; adattatore runtime Multiverse da collegare |
| Virtualizzazione | Montaggio lazy e aggiornamento visibile; cache delle righe visitate non limitata |
| Persistenza demo | In memoria; nessun export/import v1 con perdita silenziosa |
| Meter, fader, trasporto/DSP | Assenti intenzionalmente |

Non e' corretto definire questa revisione il modulo finale completo: e' il nucleo
verificato e un candidato d'integrazione nativa. Prima di promuovere: compilare sul
vettore reale di provider, collegare gli eventi locali al documento, rendere il gruppo
temporale un vero container visuale, collaudare cattura/focus/scroll/LOD, completare il
binding source-aware di split/stretch e misurare la GUI reale. La cache non puo' essere
limitata distruggendo canvas finche' contengono stato non trasferito agli owner.

## Confini canonici consultati

- Multiverse `TimelineProjection.h`, `TimelineProjectionData.h`, `TimelinePanel.h`
  e README del pannello, letti nella revisione `4a23d2bb00c8e01ba1baa9e76ad84fd4bb058d98`.
- `ayra_timeline/timeline/engine/ayra_TimelineEdit.h`, blob
  `f73ec674c23a5ca2dfca757f87e26cd99ef36ced`: candidati/admission e vero Glue PCM.
- `ayra_timeline/timeline/model/ayra_ClipLink.h`, blob
  `7f558bb0bbf4575c97ea287767e70ea1743e24a6`: membership passiva e timing distinto da fade/gain.

Si riusano contratti e riferimenti, non si copiano provider privati nella repo pubblica.
La nuova namespace `ayra::timeline_panel` e' proposta per la presentazione: non pretende
di essere un nuovo owner canonico dei contenuti.
