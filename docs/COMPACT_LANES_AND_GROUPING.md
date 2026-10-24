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
| Shift + clic | Raggruppa immediatamente al rilascio le clip selezionate nella stessa lane, se almeno due; altrimenti la clip premuta e la successiva in ordine temporale. |
| Drag oltre 4 px / Escape / cambio revisione del documento prima del rilascio | Annulla il raggruppamento senza tagliare. |

Non esistono piu' temporizzazione o variante a doppio clic; un secondo clic rapido
viene consumato per evitare un Glue annidato accidentale. Ogni gesto produce al
massimo **una** transazione e un solo Undo. Il callback cattura identita', revisione
e selezione durante il mouse-down e le rivalida al rilascio. La selezione multi-clip
e' quella gia' usata dal pannello (`Clip...` e menu contestuale), non la selezione
dei punti di automazione o delle note MIDI.
La richiesta senza clip successiva, con membri bloccati o riferimenti obsoleti
viene rifiutata, non convertita in split.

Il comando e' `glueClips`: non `clipLink`, non `groupLayout` e non una relazione
soltanto grafica. Le clip sorelle vengono rimosse dalla lane corrente e inserite,
con tempi relativi e identita' invariate, in una nuova clip container. Il container
espone i propri controlli Warp e Gain, oltre a bordi, Cut rail e Glue rail; puo'
quindi essere tagliato, ridimensionato o inserito in un altro container. Lo split
partiziona ricorsivamente il sotto-arrangement: una clip interna intera appartiene
a una sola meta', mentre una clip attraversata dal taglio viene divisa.
`dissolveContainer` ripristina i figli diretti ed e' una singola transazione Undo.

Nel Lab il Warp del container deforma l'intero sotto-arrangement: posizione,
durata e contenuto visuale dei figli vengono proiettati dalla stessa mappa, quindi
anche i vuoti si comprimono o si espandono. La trasformazione si compone
ricorsivamente con i Warp dei container annidati senza riscrivere lo stato interno
dei figli. Il Gain del container e' un inviluppo esterno moltiplicativo: scala
waveform audio, velocity MIDI, valori Line e opacita' Image; nei container annidati
i Gain degli antenati vengono moltiplicati. I punti estremi restano ancorati,
Shift vincola il drag in verticale, Alt rimuove un punto interno e il doppio clic
sulla linea ne inserisce uno. Sciogliere il Glue rimuove soltanto le trasformazioni
esterne; un eventuale bake/flatten resta un comando separato ed esplicito.

Nella timeline chiusa ogni figlio usa il proprio renderer reale ridotto: waveform
audio, note MIDI, curva Line o filmstrip Image. I vuoti temporali restano vuoti e
i container annidati vengono proiettati ricorsivamente. I figli non espongono
Warp/Gain ne' hit-zone; Warp e Gain visibili appartengono al container. Un container
MIDI non espone la scrollbar pitch ne' la sua hit-zone: le preview scelgono il range
automaticamente, mentre le clip aperte nel canvas interno conservano il proprio scroll.
Il doppio
click sull'header conserva il ciclo d'altezza; quello su una zona libera del corpo
apre un `CallOutBox` temporaneo con il canvas reale dei figli. Qui le clip tornano
editabili normalmente, incluso Glue annidato; `Sciogli Glue` dissolve il container
aperto. Lo stato visuale resta indicizzato dagli ID canonici anche
quando i figli non sono montati nella timeline principale.

Una clip gia' parte di un `clipLink` verticale non puo' attraversare il confine di
un container: Glue viene rifiutato atomicamente invece di lasciare relazioni tra
documenti sorgente diversi. Nella fixture questa struttura ricorsiva modella il
contratto di `ayra::timeline::EditSource`; non introduce un secondo owner prodotto.
Lo split di una clip collegata e' invece atomico: tutte le clip della componente
relazionale che attraversano il punto vengono divise nella stessa transazione e
la topologia viene clonata per le meta' destre. Un membro bloccato o che non
attraversa il punto rifiuta l'intera operazione, senza mutare curve locali. I canvas
non originanti riconciliano lo stato derivato della nuova meta' dal proprio stato
sinistro, evitando reset di note, warp o automazione.
Le lane evento keyframe, che non espongono clip documentali, conservano le loro
operazioni locali: non si raggruppano fittiziamente gli eventi usando ID di clip diverse.

I gruppi di layout delle lane restano `ViewState::layoutGroups`, separati dai
gruppi musicali. Il Controller prepara una proiezione O(1) per riga e il pannello
disegna parentesi continue ai due bordi, colore stabile e badge `LANE GROUP N`.
Il feedback segue riordino, righe nascoste, cambio modalita', Undo e Ungroup senza
creare una seconda source of truth.

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
