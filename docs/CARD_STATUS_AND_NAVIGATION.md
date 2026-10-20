> Aggiornamento: la passivita del nome e la disposizione degli stati descritti qui
> sono sostituiti da [card adattiva e rinomina](ADAPTIVE_CARD_AND_TITLE_EDIT.md).
> Questo documento conserva il contratto della precedente consegna.

# Card: navigazione e stato della traccia

Questa revisione sostituisce il piano della pulsantiera/ostacolo dentro la lane.
Non introduce deformazioni delle clip, cambia solo la composizione dei controlli.

## Default e compatibilità

I sei controlli laterali, i gruppi recessed, il relativo dispatch e le API sono
conservati. `ViewState::chrome` li nasconde per default insieme ai meter e al
marcatore grafico del resize. Il menu Visibilità li può riattivare singolarmente.
`ViewState::identityNavigation` controlla separatamente le frecce nella card ed è
true al primo avvio; `TimelinePanel::setIdentityNavigation` aggiorna anche la GUI.
Le altezze non includono spazio per gruppi nascosti. Nessuna nuova fascia header.

La lente conserva centro verticale, materiale, blur reale, bordo sinistro sfumato,
Performer a sinistra e nome/tipo a destra. Il massimo rimane 229 pixel logici, senza
terza riga. Due chevron singoli, con target 16x18 e gap 2, seguono il tipo. Le icone
sono le stesse della precedente navigazione, non rewind/fast-forward.

## Readback, non nuovi interruttori

`Owner::status` (`TimelineTrackStatus.h`) è una proiezione read-only fornita dall'host.
Nessun comando di processing, polling DSP o motore è aggiunto al modulo UI.
M/S sono i campi esistenti dell'owner e appaiono, soltanto se attivi, dopo il nome,
con il medesimo font. Le altre sette icone occupano al massimo 60 px nella riga già
esistente, prima del nome: ciascuna è 6x6, vicina alle dimensioni del pallino da 5 px.
I rettangoli degli stati spenti sono vuoti. I nomi lunghi usano ellissi, non font
compressi. Nessuna icona passiva intercetta i gesti di editing.

| Stato | Glyph / colore |
| --- | --- |
| Mute | M / `tokens::muteIndicator()` (giallo) |
| Solo | S / `tokens::soloIndicator()` (rosso) |
| Shutdown | `nodeButtonGlyph(shutdown)` / power-off rosso |
| Bypass | `nodeButtonGlyph(bypass)` / giallo canonico |
| Canvas attivo | `nodeButtonGlyph(window)` / verde canonico |
| Monitoring attivo | glyph Audio già usato dal Lab / `powerOnIndicator()` |
| Bufferize | `nodeButtonGlyph(buffer)` / viola canonico |
| Oversampling | `nodeButtonGlyph(oversample)` / arancio canonico |
| Latency compensation | `nodeButtonGlyph(latency)` / ciano canonico |

`TimelineStatusArtwork` delega ai selettori Ayra, non duplica i path delle icone.
Il mixer confrontato usa gli stessi selettori per shutdown/bypass/buffer/oversampling
e gli stessi token M/S. Per canvas e monitoring la tabella dichiara l'adattamento
host del Lab: non inventa un corrispondente readback già disponibile nel mixer.
In particolare, `Monitoring::automatic` non implica ON né OFF: `monitoringActive`
viene risolto dall'host. `canvasActive` non è `isShowing()` della riga.

Gli stati non disponibili rimangono spenti. La demo ne espone combinazioni
**sintetiche dichiarate**, condivise dalle lane sorelle; non finge un processore
Tensor attivo. Il port userà i readback di documento/runtime del prodotto.
Menu di authoring con il tasto destro restano responsabilità dell'app, come richiesto.

## Hover: un solo coordinatore del pannello

- Un bersaglio editabile sotto la card avvia una scadenza di 500 ms.
- Movimento all'interno non ricomincia il conteggio ad ogni pixel.
- Entrare nella coppia di frecce annulla la scadenza; il gap fra le due protegge
  il passaggio da una freccia all'altra. Uscirne riavvia 500 ms se il contenuto
  sottostante è ancora editabile.
- Alla scadenza si ricontrollano puntatore e geometria, poi si nascondono card e
  figli. Nessuna hit-zone delle frecce resta sopra le clip quando sono invisibili.
- Dopo la scomparsa si resta nascosti fino all'uscita dal footprint, anche sopra
  vuoti interni. Un vero inizio di editing fuori dalle frecce può nascondere subito.
- Nomi e stati non ritardano né inoltrano artificialmente input: il target JUCE è
  sempre il canvas reale. Solo i due bottoni ricevono input proprio.

`TimelineIdentityHoverState` è una macchina a stati pura con tempo iniettato.
Il pannello riusa il suo unico Timer per la singola scadenza candidata e per gli
optional meter visibili. Senza scadenza/meter resta fermo. Nessun timer per lane,
nessuna scansione di tutte le lane in ogni tick. Hide, eviction, session change e
shutdown cancellano la richiesta; la durata è di UI, non sample-accurate.

`LaneCanvas::containsEditableAt` allarga il contratto a curve, punti, segmenti e
span. I canvas Continuous e Keyframe delegano ai loro hit-test già usati per input;
i canvas a clip mantengono il callback compatibile `containsClipAt`. La factory
rifiuta a compile-time adattatori senza alcun contratto pubblico di hit geometrico.
Per le linee continue il resize manuale resta attivo, il doppio click non cambia
altezza; i doppi click musicali sui punti non vengono rimossi.

## Fonte e confini della consegna

Riferimento Multiverse letto: `459d8c5cfc4e06bd849db12799e447ec90be3feb`,
`MixerChannelStrip.cpp` e `modules/look_and_feel/ayra_NodeButtons.h`.
`AYRA_SOURCE_DIR` continua a compilare in place le foglie visuali originali.
Il profilo snapshot rimane un opt-out esplicito: questa revisione non cambia
impostazioni di build o pubblica sorgenti privati/font.

Vedere [verifiche correnti](../verification/card-status/STATUS.md).
I nuovi preset di altezza, il focus MIDI e il layout ad ostacoli discussi in
precedenza non sono applicati implicitamente da questa semplificazione.
