# Lane compatte, hit-test e raggruppamento orizzontale

Questa specifica sostituisce le descrizioni parziali della PR #1. Le altezze
sono pixel logici della **lane**, non del corpo clip o dell'area della curva.

## Geometria e zoom

`TimelineVisualMetrics.h` condivide le metriche con il packer delle lane evento:
3 px sopra + corpo evento/clip di 18 px + estensione della barra di taglio di
8 px + 3 px sotto = **32 px**. Le lane di punti, prive della barra, restano 24 px;
le sovrapposizioni degli eventi continuano a produrre righe automatiche aggiuntive.
Il minimo manuale non aggiunge un header; la barra inferiore non invade il resize.

Il pannello e i canvas usano un'unica policy di altezza di presentazione:

- Audio, immagine/video e linea: 32 ↔ 400.
- MIDI: 32 → 256 (note editabili) → 400 (note + velocity a steli) → 32.
- Linea continua: lo stesso ciclo a due livelli tramite gli accenti laterali.

I limiti espliciti dell'host restano rispettati. Un massimo intermedio non deve
intrappolare la lane espansa. Il ciclo non scrive durate o posizioni musicali.
L'hit-test della clip resta separato dall'azione: titolo e header superiore
senza marker, oppure corpo libero sotto 48 px di corpo, attivano il ciclo;
marker, punti, curvature, note, estremi e sovrapposizioni conservano la propria
azione di doppio clic. Il corpo audio espanso conserva l'apertura editor.
Non viene aggiunto un secondo intercettore di doppio clic nel pannello.

## Warp, curve e strumenti di bordo

Da **64 px di lane** compaiono triangoli e rail superiore warp; sotto la soglia
rimangono le linee verticali, ma non i bersagli invisibili o il margine da 12 px.
Questa regola vale per audio, MIDI, linea e immagini.

Le curve gain/alpha/linea hanno stati distinti: disattivate, preview, editing.
Sotto la soglia di editing la curva reale resta come hairline da 0,75 px, senza
riempimento, punti o maniglie sensibili al mouse. Le lane continue compatte
non accettano neppure inserimenti invisibili tramite doppio clic.
Il gain disabilitato dall'utente resta disabilitato anche nella preview.

La curva centrale e i segmenti di fade sono path distinti: solo fade-in e
fade-out vengono tratteggiati, senza un successivo tratto continuo sovrapposto.
La geometria matematica e il divieto di inserire punti in quei segmenti non cambiano.

Sotto il precedente livello compatto di 96 px, entrambi i bordi risolvono lo
strumento **Slip / Sizing Moves Contents**, il terzo partendo dal basso fra
Loop, Slip, Trim, Stretch. Si riusa l'algoritmo esistente: a destra resta fisso
il termine della finestra sorgente; a sinistra il suo inizio. Per le linee ciò
si esprime nella finestra dei punti, per i media nell'offset sorgente.
Hover, parentesi, tooltip e mouse-down ricevono la stessa zona semantica.

## Shift sulla barra inferiore delle clip

La policy del cursore passa immediatamente da forbici a colla quando viene
premuto Shift, anche col mouse fermo. Senza Shift il taglio normale resta immediato.

| Gesto | Operazione |
| --- | --- |
| Shift + clic | Raggruppa le clip selezionate nella stessa lane, se almeno due; altrimenti la clip premuta e la successiva in ordine temporale. |
| Shift + doppio clic | Raggruppa tutte le clip della lane nel documento, incluse quelle fuori viewport. |
| Drag oltre 4 px / Escape / cambio revisione del documento | Annulla il raggruppamento pendente senza tagliare. |

Il singolo clic aspetta l'intervallo di doppio clic configurato in JUCE; il
secondo annulla il singolo pendente. Ogni gesto produce al massimo **una**
transazione e un solo Undo. Il callback cattura identita', revisione e selezione
prima che altri input possano modificarle; la riga mantiene la residenza durante
l'attesa. La selezione multi-clip e' quella gia' usata dal pannello (`Clip...` e
menu contestuale), non la selezione dei punti di automazione o delle note MIDI.
La richiesta senza clip successiva, con membri bloccati o riferimenti obsoleti
viene rifiutata, non convertita in split.

Il comando e' `temporalGroup`: non `clipLink`, non `groupLayout`, non rendering
PCM e non concatenazione distruttiva dei media. I gruppi orizzontali esistenti
interessati vengono uniti, evitando appartenenze duplicate; i link verticali
preesistenti non vengono modificati. Ripetere un gruppo identico e' un no-op.
Si riusa `preparePanelFixture`: propagazione delle operazioni solidali,
validazione, rollback e Undo/Redo restano dello Store. Le restrizioni esistenti
sullo split/duplicazione di clip gia' collegate non sono rimosse da questa modifica.
Le lane evento keyframe, che non espongono clip documentali, conservano le loro
operazioni locali: non si raggruppano fittiziamente gli eventi usando ID di clip diverse.

## Card

Il materiale ha un nucleo piu' scuro e il dissolve e' limitato al margine esterno.
Fade e blur sono speculari: card a sinistra, lato pieno a sinistra; card a destra,
lato pieno a destra. Il margine destro e' arretrato senza superare il viewport.
L'espansione del titolo tiene conto di tutti i padding realmente usati.

Il titolo non rinominabile (per esempio Tempo) fa scomparire immediatamente la
card, anche sopra una preview non editabile. Titolo rinominabile e navigazione
restano protetti; le altre aree passive conservano il comportamento precedente.

## Build e verifica

Si usa JUCE 8.0.12. Il provider curve canonico puo' essere `ayra_curves` oppure
le foglie `ayra_math/curves/ayra_CurveSegment.h` e `ayra_FadeCurves.h`, lette dal
checkout esterno. L'adapter contiene solo include: nessun algoritmo privato o
font viene copiato in questa repository.

`CanvasInteractionTests.cpp` compila la stessa composition TU dell'applicazione:
canvas reali, bridge documentale, timer e disegnatori. Non sostituisce i canvas
con mock. Gli altri test coprono policy, transazioni, controller, layout e card.
Le prove eseguite, il profilo visuale e i limiti di piattaforma sono dichiarati
in `verification/compact-delivery/STATUS.md`.
