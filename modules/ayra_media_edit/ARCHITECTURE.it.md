# Ayra Video Editor — architettura e piano di lavoro

Data: 22 ottobre 2026. Nome di lavoro, non nome commerciale definitivo.
Baseline esaminata: `AyraSoft/AyraTimelineLab`, commit
`48502a95c92ce10a8a974dc792bae45027ab0ab0`.

**Stato:** progetto architetturale con un primo nucleo C++20 eseguibile e testato.
Non è ancora un editor utilizzabile. Questa consegna non modifica la GUI del Lab,
non introduce dipendenze multimediali e non contiene codice privato di Multiverse.

## 1. Obiettivo e decisione di riuso

Realizzare un editor desktop locale per montaggio social e musicale, con timeline
Ayra, progetti riapribili, audio curato, sottotitoli animati e procedure automatiche.
Non replicare l'intero catalogo o i formati proprietari di CapCut.

Il Lab offre un pannello C++20/JUCE, lane virtualizzate, identità stabili,
selezione, navigazione, canvas e contratti di comando. La documentazione e il codice
separano esplicitamente presentazione da documento, trasporto, media e DSP.
La demo nuova è in memoria e non riproduce audio; il codec v1 riguarda il banco
legacy. Il gruppo temporale non è una composizione annidata. Questi sono limiti di
partenza, non funzioni da dichiarare già pronte.

Riferimenti interni: `../../README.md`, `../ayra_timeline_panel/README.md`,
`../ayra_timeline_panel/TimelineModel.h`,
`../../docs/TIMELINE_PANEL_IMPLEMENTATION.md`, `../../CMakeLists.txt`.

**Decisione:** conservare la timeline nativa. Non convertirla in React e non
sostituirla con Remotion Editor Starter. Il progetto canonico rimane C++; Remotion
è un renderer opzionale di sottotitoli/grafica, separato attraverso un protocollo
versionato. Il CMake corrente disabilita il browser JUCE: nessuna integrazione
WebView esiste già. L'eventuale abilitazione richiederà una scelta esplicita.

## 2. Interfaccia prevista

Libreria media/preset a sinistra; monitor video e controlli di trasporto al centro;
inspector a destra; timeline Ayra ridimensionabile in basso. Gli inspector seguono
la selezione e distinguono valori comuni, misti e proprietà non applicabili.

Pagine: **Clip**, **Video**, **Colore**, **Audio**, **Voiceover/Sidechain**,
**Sottotitoli**, **Animazioni**, **Transizioni**, **Progetto/Export**.
Il testo dei sottotitoli resta correggibile e navigabile dalla trascrizione.
La timeline conserva la presentazione approvata, senza introdurre track header
estranei. Anteprime, operazioni in coda e media mancanti hanno stati espliciti.

Il gruppo Alt/Option+G sarà una scorciatoia configurabile del nuovo prodotto;
non si sovrascrivono scorciatoie esistenti prima dell'audit dei conflitti.

## 3. Moduli e confini

| Modulo previsto | Responsabilità |
|---|---|
| `ProjectCore` | Identità, asset, clip, sequenze, relazioni, revisioni, comandi e Undo |
| `TimelineAdapter` | Proietta il documento nei veri Snapshot/CommandSink del pannello |
| `ProjectStore` | Schema versionato, salvataggio atomico, autosave, recupero e relink |
| `MediaService` | Probe, decoder, proxy, miniature, waveform e cache con budget |
| `PlaybackEngine` | Clock audio, scheduling, seek, sincronismo e compositing video |
| `AudioEngine` | Registrazione, processing, bus, detector sidechain e mix |
| `CaptionService` | Trascrizione, allineamento, correzioni e proiezione source-aware |
| `GraphicsService` | Trasformazioni, correzione colore, effetti e keyframe |
| `RenderService` | Snapshot immutabile, esportazione, fotogrammi e preflight |
| `JobService` | Coda, annullamento, avanzamento, retry e scarto risultati obsoleti |

La GUI invia intenzioni, non mutazioni ai vettori delle clip. Il proprietario del
documento prepara e valida una transazione, applica tutti i cambiamenti o nessuno,
incrementa la revisione e pubblica lo snapshot. Lo stesso confine servirà ad
un'integrazione REA: comandi tipizzati e autorizzati, non una shell indiscriminata.
Le capacità effettive di REA andranno verificate nella sessione di integrazione.

Il realtime audio non alloca, non accede al disco e non attende trascrizione,
FFmpeg o rendering JavaScript. Worker separati preparano asset e grafi immutabili.
Processi locali avviati con argomenti strutturati, senza concatenazione shell;
percorsi autorizzati, limiti di risorse, timeout e cancellazione dell'intero job.
Nessun upload dei media per default; eventuali servizi cloud sono opt-in.

## 4. Modello del progetto

Un asset descrive un file immutabile, fingerprint, localizzazione, durata, stream,
rotazione, pixel aspect, time base, fps razionale e informazioni colore. Una clip
è un'istanza: non il file. Audio e video dello stesso contenitore mantengono identità
separate e un link esplicito. Staccare l'audio cambia il link, non la sua identità.
Un'immagine è una clip visuale con durata impostata dall'utente.

Si distinguono quattro concetti:

- **Link:** conserva sincronismo tra istanze, anche audio/immagine; unlink non distrugge i media.
- **Gruppo di editing:** selezione e modifiche coordinate, senza modificare il mix.
- **Compound clip:** sequenza interna con tempo locale, riapribile senza flatten distruttivo.
- **Bus audio:** somma e processing delle clip assegnate; può essere pilotato da un sidechain.

Le cartelle o i gruppi di disposizione della timeline non diventano automaticamente
bus. Un compound non deve necessariamente creare un bus. Si impediscono cicli
nelle sequenze annidate e nel routing. Il primo rilascio può limitare la profondità
di annidamento con errore esplicito, mai con perdita silenziosa di contenuto.

Gli effetti sono catene ordinate, con parametri, bypass e keyframe nel tempo locale
della clip. Preset serializzabili, versionati e importabili; validazione di tipo,
intervallo e versione. Gli import non eseguono codice JavaScript, script o plugin.
Font/LUT mancanti richiedono un fallback dichiarato o bloccano il render finale.

### Tempo

Il nucleo iniziale usa interi a 282240000 tick/s, intervalli semiaperti `[inizio,fine)`
e aritmetica controllata. Questa base rappresenta esattamente i comuni sample rate
e frame rate interi/NTSC. Conservare comunque i PTS e le time base originali per
VFR e sorgenti non rappresentabili esattamente; niente somma ripetuta di secondi
floating point. La GUI converte in secondi soltanto per la vista.

Nel protocollo JSON gli int64 vanno serializzati come stringhe decimali o valori
razionali, non come Number JavaScript non controllati. Il compilatore di render
applica una politica unica di arrotondamento a frame/sample. Stretch, reverse e
speed ramp richiederanno TimeMap esplicite; **non sono implementati nel primo nucleo**.

## 5. Sottotitoli agganciati all'audio

La trascrizione conserva parole con identità e tempi della sorgente, stream audio,
lingua, modello/versione e, quando disponibili, confidence. Un binding collega
trascrizione e istanza audio con stile e correzioni. I tempi in timeline sono una
proiezione, mai una seconda verità scollegata.

A velocità unitaria: `tempoTimeline = inizioClip + tempoSorgente - sourceIn`.
Si interseca prima la parola con l'intervallo sorgente usato dalla clip.
Con ritiming o compound si compongono le relative mappe, senza ritrascrivere ad
ogni spostamento. Il riconoscimento automatico e l'allineamento delle parole sono
problemi distinti; l'accuratezza va misurata sui vostri audio italiani reali.

| Operazione | Comportamento richiesto |
|---|---|
| Move | Testo e animazioni seguono la clip |
| Trim | Si vedono solo parole/intervalli ancora presenti |
| Split | Ogni figlia mantiene il binding alla stessa sorgente |
| Delete audio | Rimozione atomica dei binding dipendenti; niente testi orfani |
| Unlink audio/video | Il binding resta all'istanza audio |
| Duplicate | Nuova istanza, binding distinto; trascrizione sorgente riutilizzabile |
| Group/compound | Proiezione nelle coordinate del contenitore, senza flatten |
| Undo/redo | Ripristino congiunto di clip, relazioni, testo e stile |
| Mute | Non equivale a delete: testo conservato, visibilità controllabile separatamente |

Un taglio dentro una parola produce frammenti temporali con segnalazione: non
possiamo ricostruire una sillaba audio rimossa. L'automazione preferisce confini
tra parole, ma non sposta di nascosto un taglio manuale. Il raggruppamento in frasi
non attraversa tagli o vuoti della clip. La stessa trascrizione può alimentare più
istanze senza confonderne cancellazioni e stili. Le correzioni manuali non devono
essere sovrascritte da una nuova trascrizione: versioni e confronto espliciti.

Whisper.cpp è il primo provider locale da collaudare; un aligner separato resta
sostituibile. Nessuna promessa di trascrizione perfetta, soprattutto con musica,
riverbero, voci sovrapposte o canto. Rendere accessibili correzione testo e timing.

## 6. Audio, voiceover e silenzi

Voiceover comprende due funzioni distinte: registrare una voce nella timeline e
abbassare automaticamente un altro segnale quando quella voce parla.

Esempio di prodotto: più clip vocali alimentano **bus Voce**, più clip musicali
alimentano **bus Musica**. Il detector Voce controlla un compressore sul bus Musica;
Voce e Musica arrivano poi al master come percorsi audio separati. Il sidechain
non aggiunge una seconda copia udibile della voce. Selezionare due clip usa lo
stesso contratto di routing su ingressi singoli; selezionare gruppi usa i bus.

Controlli: sorgente detector, destinazione, soglia, ratio, attack, release, knee,
riduzione massima, detector pre/post-fader e bypass. V1: detector post-pulizia,
pre-fader e continuità del suo stato attraverso i confini delle clip. Silenzi e
buchi producono rilascio, non reset casuali. Misurare e compensare latenze.

Pipeline proposta: pulizia opzionale -> gain/EQ/compressione -> bus/ducking -> master
-> analisi loudness/normalizzazione finale -> controllo true peak. Analisi
normalizzazione separata dall'applicazione; cache invalidata dopo cambi di mix.
Preset distinti voce/musica: niente denoiser vocale o normalizzazione distruttiva
applicati automaticamente agli strumenti. Ascolto A/B e regolazione dell'intensità.

FFmpeg è un candidato per denoise, loudness, sidechain e render offline. Non è da
solo il motore di editing realtime. Prima implementare anteprime del processing
precalcolate dallo stesso grafo di export; poi DSP live con test di equivalenza.
Una preview non aggiornata deve essere segnalata, non spacciata per definitiva.
Registrazione: dispositivo selezionabile, countdown, monitoring e compensazione
latenza, buffer realtime verso writer dedicato.

Taglio silenzi: detector di livello/VAD selezionabile, durata minima, isteresi,
padding iniziale/finale, protezione dei respiri e crossfade anti-click. Prima
mostrare i tagli proposti; applicazione unica annullabile. Lo scope ripple specifica
quali lane spostare, incluse dipendenze AV/caption, e come trattare lane bloccate.
Non togliere le pause di una performance musicale per default. Auto-Editor può
essere valutato come analizzatore, non come proprietario di una timeline alternativa.

## 7. Video, grafica, animazioni e transizioni

V1: trasformazione, crop, scala, posizione, rotazione, opacità; esposizione,
contrasto, saturazione, temperatura/tinta e nitidezza. Catene deterministiche,
bypass, copia/incolla impostazioni, keyframe e preset import/export. LUT e controlli
avanzati entrano solo con test colore e validazione file. Pipeline SDR Rec.709
iniziale; un file HDR va riconosciuto, gestito esplicitamente o rifiutato, non
reinterpretato silenziosamente. Evitare differenze fra filtri GUI ed export.

Catalogo iniziale proposto di sottotitoli: classico, box arrotondato, parola attiva,
evidenziatore mobile, pop e due varianti di brand. Non è il catalogo CapCut copiato.
Preset descrive font, contorno, ombra, posizione, linee massime, spaziatura, parole
per pagina, palette, durata/curva di animazione e margini di sicurezza.

Remotion fornisce utilities e componenti di partenza. Worker Node/Chromium locale,
versioni bloccate, riceve i cue già proiettati e genera overlay con alpha. Cache per
segmenti su hash di testo, timing, stile, font, renderer e risoluzione. Preview e
export usano il medesimo componente e gli stessi asset, non due animazioni
riscritte indipendentemente. Benchmark obbligatorio prima di adottare l'integrazione:
refresh degli stili, scrubbing, consumo RAM, testo italiano, alpha e font.
Una WebView con Player è alternativa da valutare, non integrazione già disponibile.

Transizioni audio e video indipendenti o collegate: fade, dissolvenza/crossfade e
poi slide/wipe. Rappresentazione con due clip, durata, curva e policy dei media
handles. Mancanza di fotogrammi oltre il trim: riduzione della durata o freeze
esplicitamente richiesto, non accesso fuori sorgente. Supportare entrate/uscite
audio diverse dal video, inclusi J/L cut. Effetti complessi vengono dopo il motore.

## 8. Progetti, cache e distribuzione

Formato di lavoro proposto `.ayravideo`: directory/package con manifest JSON
versionato e transazioni recuperabili. Contiene asset registry, sequenze, routing,
caption, preset e impostazioni; media originali referenziati di default. Comando
**Raccogli media** per una copia portabile. Cache e proxy rigenerabili, mai
necessari a interpretare il montaggio; originali mai sovrascritti.

Salva/Salva con nome, autosave debounced, scrittura temporanea e replace atomico,
backup ultimo valido, recovery dopo crash, gestione spazio esaurito, lock tra
istanze. Percorsi relativi quando possibile, relink assistito e fingerprint per
non agganciare un file diverso con lo stesso nome. Migrazioni testate e rifiuto
esplicito di versioni future sconosciute. L'apertura offline deve mantenere il
montaggio anche quando manca un media, con placeholder e avviso.

## 9. Render, fotogrammi e social

RenderJob utilizza una revisione immutabile del progetto: modifiche successive
non alterano un export in corso. Il preflight controlla media, font, routing,
intervalli e spazio libero. Progress reale, cancellazione, cleanup dei temporanei,
file finale pubblicato solo dopo completamento e verifica con probe.

Preset iniziali proposti: verticale 1080x1920, quadrato 1080x1080 e orizzontale
1920x1080; fps razionale e qualità configurabili, profili H.264/AAC da validare.
Sono impostazioni di prodotto, non una dichiarazione universale sui limiti dei
social. Profili platform-specific versionati e verificati prima di ogni release;
nessuna promessa di pubblicazione automatica. Safe area e preview del crop.
Sottotitoli impressi nel video oppure SRT/VTT; questi ultimi non conservano tutte
le animazioni. Esportazione WAV opzionale del mix.

Fotogramma al playhead dalla composizione finale, non screenshot della GUI:
PNG/JPEG, con/senza sottotitoli e grafica, dimensione finale e profilo colore.
Nella preview si possono usare proxy; l'export riparte dagli originali.

## 10. Roadmap con criteri di uscita

Stime orientative in settimane-persona di sviluppo esperto, non scadenze promesse.
Non sommare tempi di attesa a tempi di lavoro; riesaminare dopo lo spike media.

| Fase | Deliverable | Criterio di uscita | Stima |
|---|---|---|---|
| 0 — fondazioni | Audit, decisioni, contratti e test source-bound | Primo nucleo di questa consegna; integrazione ancora esclusa | Avviata |
| 1 — progetto e import | ProjectCore completo, store, migrazioni, import/proxy e adapter della timeline | Salvare, chiudere e riaprire un montaggio reale senza perdere identità, link o modifiche | 3–5 |
| 2 — editing e playback | Riproduzione AV, trim/split/ripple, link, gruppi, compound, Undo e export base | Montaggio reale con sync verificato, seek ripetuti e export confrontabile | 4–7 |
| 3 — audio | Pulizia, loudness, bus, sidechain, registrazione e silenzi | Ducking continuo tra gruppi, tagli annullabili, assenza di click e clipping nel set di prova | 3–5 |
| 4 — sottotitoli | ASR locale, correzione, binding nel documento, renderer e primi preset | Move/trim/split/delete/undo/compound e riapertura senza cue orfani | 3–5 |
| 5 — grafica | Colore, preset/keyframe, transizioni, fotogrammi e profili export | Confronti preview/render, import preset e test media handles | 3–5 |
| 6 — consolidamento | Recovery, performance, installer, firme e collaudo sul Mac target | Tutta la checklist sotto su un corpus rappresentativo | 2–4 |

Ordine dipendenze: 1 prima di 2; 2 abilita 3/4; 5 riusa il rendering; 6 integra.
Spike Remotion e collaudo ASR anticipabili durante 1. Totale iniziale **18–31
settimane-persona** per il perimetro completo, da ricalibrare; sviluppo assistito
non elimina test hardware, codec, UI o manutenzione. Mac target iniziale proposto,
configurazione Intel/Apple Silicon da confermare senza bloccare il core portabile.

Prima milestone utile: import -> montaggio semplice -> salva/riapri -> sottotitoli
legati alla voce -> un preset -> export verticale. Anticipare un sottoinsieme di 4
prima di completare gli effetti avanzati. Non aspettare venti preset per validare
l'intero percorso con un vostro video.

### Definition of done del prodotto

Salva/riapri identico; recovery con salvataggio interrotto; file mancanti/relink;
nessun orfano dopo editing e Undo; ASR obsoleto scartato dopo modifica sorgente;
annidamento e link coerenti; sidechain aciclico e senza doppia voce; riproduzione
continuativa e seek senza deriva oltre un frame nel corpus concordato; audio
verificato a sample boundary dove applicabile; export/fotogrammi con gli stessi
effetti e font; job cancellabile e cache limitata; contenuti VFR/rotati/mono/stereo,
44.1/48 kHz e 23.976/29.97 fps; progetti lunghi e moltissime clip con misure reali.
Obiettivi di fps/UI da fissare dopo benchmark sul computer target, non inventati.

## 11. Cosa contiene davvero questa consegna

`CaptionDocument.h`: documento ristretto alle istanze audio/caption; convalida
identità e riferimenti, proiezione a velocità unitaria, move/trim/split/delete,
transazioni con sessione/revisione e Undo/Redo limitato per numero di snapshot.
`AudioRouting.h`: validazione di bus, send e sidechain, parametri e assenza di cicli.
`tests/MediaEditTests.cpp`: 29 casi con controlli attivi anche in Release.

Questi componenti non elaborano media, non trascrivono, non salvano progetti e
non sono ancora collegati alla timeline. Il routing è un contratto, non un
compressore funzionante. Non sono implementati compound, link AV, retiming,
registrazione, import, GUI, preset grafici o export. La durata fisica degli asset
sarà verificata dal catalogo/probe di Fase 1; il nucleo valida solo i propri range.
La storia a snapshot e le ricerche lineari sono una scelta di prototipo, non una
promessa di prestazioni su grandi progetti. Il modello completo dovrà mantenere
queste invarianti migrando a store indicizzato e journal di comandi.

La build standalone evita deliberatamente dipendenze JUCE/provider private;
non sostituisce i test nativi del Lab. Vedere `VERIFICATION.md`.

## 12. Dipendenze e costi da verificare prima della distribuzione

Remotion non è il riconoscitore vocale: integra provider quali Whisper.cpp e
consuma timestamp. L'Editor Starter è un prodotto/template distinto e React-based;
non serve comprarlo per riutilizzare la timeline Ayra. Isolare il renderer evita
che un cambiamento di licenza imponga di riscrivere il documento.

Alla verifica del 22 ottobre 2026 il sito Remotion indica uso gratuito per individui
e aziende fino a tre persone; per organizzazioni più grandi, automazioni/editor
possono ricadere nella Company License, con minimo pubblicato di 100 USD/mese per
Automators. Applicabilità da verificare sulla vostra organizzazione e modalità
commerciale; non promettere costo zero né dedurla dal numero di utenti dell'app.
Verificare inoltre licenza JUCE già posseduta, build/licenze codec FFmpeg,
distribuzione modelli e font/LUT. Nessuna nuova licenza viene attribuita ai moduli
Ayra esistenti e nessun asset proprietario CapCut viene importato.

Fonti tecniche primarie consultate; versioni da fissare dopo lo spike:

- Remotion captions: https://www.remotion.dev/docs/captions/
- Trascrizione/provider: https://www.remotion.dev/docs/captions/transcribing
- Wrapper Whisper.cpp: https://www.remotion.dev/docs/install-whisper-cpp/transcribe
- Stili disponibili: https://www.remotion.dev/elements/captions
- Overlay alpha: https://www.remotion.dev/docs/transparent-videos
- Starter: https://www.remotion.dev/docs/editor-starter/before-you-buy
- Licenza/prezzi: https://www.remotion.dev/docs/license/pricing
- JUCE 8 EULA: https://juce.com/legal/juce-8-licence/
- FFmpeg filtri: https://ffmpeg.org/ffmpeg-filters.html

Il costo principale da considerare è sviluppo e manutenzione. Come semplice
sostituto economico di un abbonamento l'investimento può non convenire; come
strumento Ayra riutilizzabile e automatizzabile ha un obiettivo diverso.
