# Ayra Media Editor — architettura proposta

Data: 22 ottobre 2026. Nome di lavoro, non nome definitivo del prodotto.
Base esaminata: AyraTimelineLab `48502a95c92ce10a8a974dc792bae45027ab0ab0`.

**Stato:** specifica di prodotto e architettura, con un piccolo prototipo C++20 eseguibile dei contratti audio/sottotitoli. Non è un editor funzionante. Il prototipo non è collegato alla GUI e non sostituisce il documento canonico Ayra. Vedere [audit](SOURCE_AUDIT.md), [piano](ROADMAP.md) e [verifica](../../verification/media-editor-foundation/STATUS.md).

## 1. Decisione principale

Costruire un'app desktop local-first C++20/JUCE che riutilizzi il pannello timeline del Lab e i provider canonici esterni. Non riscrivere la timeline in React e non promuovere le fixture dimostrative a modello di progetto. Il Lab rimane il banco di prova: un futuro target applicativo distinto ospiterà il prodotto, dopo la prima integrazione verificata. Nessun sorgente privato Multiverse deve essere copiato nella repository pubblica.

Remotion è un adapter opzionale per sottotitoli e motion graphics, non il proprietario della timeline, del trasporto o dei progetti. La trascrizione avviene attraverso un motore ASR, inizialmente Whisper.cpp; il pacchetto Remotion relativo lo integra, non lo sostituisce. Verificare le licenze prima di rendere Remotion o altri componenti obbligatori.

Confini previsti:

```text
EditorShell JUCE: media browser | viewer | inspector a schede | timeline Ayra
                              |
                 CommandService / ProjectDocument
                    |                     |
        TimelineProjection          ProjectStore + History
                    |
           PlaybackPlan / RenderPlan immutabili
             |             |                  |
       AudioEngine    VideoCompositor      JobScheduler
                                          |     |      |
                                        ASR   FFmpeg  CaptionRenderer
                                                      (Remotion opzionale)
```

Il documento è l'unica autorità. UI, preview, export e automazioni leggono revisioni dello stesso documento. Il pannello genera richieste semantiche attraverso il suo `CommandSink`; non modifica direttamente media o trascrizioni. Un adapter converte `Owner`, `Lane`, `Event` e relazioni in snapshot di presentazione. I secondi `double` del pannello restano coordinate di visualizzazione, non la rappresentazione temporale persistita.

## 2. Layout e interazioni

Browser media/preset a sinistra, viewer centrale, inspector a destra, timeline in basso. Pannelli ridimensionabili; layout salvabile. Conservare il linguaggio visivo del Lab, le lane compatte, lo zoom, la selezione e la gestione del viewport. Non creare un secondo sistema di colori o renderer canonici.

Inspector contestuale con pagine **Video, Audio, Sottotitoli, Animazioni, Transizioni, Gruppo e Progetto**. I campi non disponibili mostrano una motivazione, non controlli finti. Multiselezione con valori misti; modifica multipla come una sola transazione. Scorciatoie configurabili: Alt/Option+G può essere assegnato a Crea compound, senza assumere che il significato sia identico su ogni versione di CapCut.

Timeline: import, drag/drop, selezione multipla, move, trim con maniglie, split, delete normale/ripple, duplicate, snapping, zoom e scroll, lock/mute/solo, link/unlink e navigazione dentro una compound. Anteprima durante il gesto, commit unico al rilascio; Escape annulla. Una modifica incompatibile con una clip bloccata viene rifiutata o richiede una scelta esplicita, mai un disallineamento silenzioso.

## 3. Modello del progetto

| Entità | Responsabilità |
|---|---|
| ProjectDocument | ID, versione schema, revisione, impostazioni, sequenza principale, riferimenti a tutti gli oggetti |
| MediaAsset | URI relativo/esterno, fingerprint, stream, durata, timebase, orientamento e metadati colore; file originale immutato |
| ClipInstance | ID stabile, asset/stream, intervallo sorgente, posizione, SourceMap, effetti e routing |
| Sequence / CompoundInstance | Sequenza con figli e istanze che la collocano nella timeline; gerarchia aciclica |
| EditGroup / LinkSet | Selezione e operazioni coordinate / vincoli di sincronizzazione con offset espliciti |
| AudioBus / SidechainRoute | Somma audio e collegamenti di controllo indipendenti dai gruppi visuali |
| Transcript / Word | Testo revisionato, ID delle parole, tempi nella sorgente e provenienza ASR |
| CaptionBinding | Proprietà di una specifica istanza audio, transcript, stile e override editoriali |
| EffectStack / Transition | Parametri ordinati, keyframe e legami tra estremi delle clip |
| Preset / ExportProfile | Versione schema, valori, dipendenze e compatibilità del renderer |

Il video importato con audio produce componenti audiovisive collegate. **Detach audio** rende indipendenti le operazioni di montaggio; **unlink** elimina il vincolo, non la sorgente. I sottotitoli appartengono al componente audio identificato stabilmente: non al rettangolo video né all'indice della lane.

Distinguere quattro concetti: raggruppare per editare insieme; linkare per mantenere sincronismo; creare una compound per avere una timeline interna; sommare su un bus per elaborare più clip audio. Una compound non è un mixdown distruttivo. Due gruppi grafici non creano automaticamente due bus. Limite iniziale proposto di annidamento: 8 livelli, con errore esplicito oltre il limite. Duplicare un'istanza di compound condivide la sequenza interna; il comando Rendi unico crea una copia indipendente.

### Tempo e transazioni

Produzione: tempi razionali tipizzati per sorgente, campioni, sequenza e frame; frame rate espresso come numeratore/denominatore. Il SourceMap mappa il tempo sorgente in quello di sequenza e compone le trasformazioni delle compound. Non accumulare durate arrotondate per ogni taglio. L'ingest conserva i PTS dei video a frame rate variabile; i proxy devono avere una mappa verificabile verso l'originale.

Velocità costante positiva nella prima versione; curve di velocità, reverse e freeze richiedono contratti e test dedicati prima di essere abilitati. Campionamento/quantizzazione avvengono ai confini di riproduzione/render, con regole dichiarate. Nel ponte JSON i grandi interi vanno serializzati senza perdere precisione JavaScript.

Ogni comando porta ID documento, revisione attesa e scope. Preparazione, controllo riferimenti/lock/cicli, applicazione atomica e pubblicazione della nuova revisione. Undo/redo opera sul documento intero, non separatamente su testo, clip e routing. L'Undo di layout già presente nel Lab resta distinto dall'Undo documentale.

## 4. Salvataggio, riapertura e recupero

Formato proposto: bundle `.ayraedit` con manifest versionato, snapshot documentali immutabili, journal transazionale e cartelle media/cache. Il salvataggio scrive un nuovo snapshot completo, lo verifica e aggiorna atomicamente il manifest; mantiene almeno un ultimo snapshot valido. Autosave e recupero dopo crash riusano questo protocollo. Lock del documento per impedire scritture concorrenti non coordinate; migrazioni esplicite e rifiuto non distruttivo delle versioni future.

Due modalità: progetto leggero con riferimenti ai media, oppure Raccolta progetto che copia i media nel bundle con consenso. Media mancanti: placeholder, ricerca e relink controllato tramite metadati/fingerprint. Cache di waveform, proxy e thumbnail ricostruibili, mai unica copia del contenuto. Eliminare una clip non cancella il file originale.

Il codec legacy v1 del Lab non può conservare le nuove relazioni: prevedere un importatore con report delle informazioni mancanti, non salvarvi silenziosamente il nuovo documento. Test obbligatori di round-trip, interruzione in ogni punto del save, migrazione, media offline e percorsi Unicode.

## 5. Sottotitoli realmente agganciati all'audio

Trascrivere la sorgente audio e conservare parole e timestamp nel suo dominio temporale. Eventuale audio pulito per l'ASR deve mantenere una mappa verso l'originale. Il file di riconoscimento ricampionato non diventa il clock della timeline. Lingua italiana, modello multilingue, revisione manuale del testo e del timing sono requisiti.

La vista sottotitoli si ricava dall'intersezione delle parole con i segmenti audio ancora presenti, trasformata dal SourceMap. Non è una traccia SRT indipendente con tempi assoluti da inseguire dopo ogni taglio.

| Operazione | Invariante |
|---|---|
| Move | Parole e stile seguono l'istanza audio, senza nuova trascrizione |
| Trim / split | Restano solo gli intervalli sorgente udibili; gli ID delle parole vengono conservati |
| Delete | Spariscono i binding posseduti dall'istanza, non le trascrizioni usate da altre istanze |
| Duplicate | Nuovi ID di istanza/binding; stessa trascrizione sorgente, timing indipendente |
| Ripple / rimozione silenzi | Tagli, link, caption e clip coinvolte cambiano in un'unica transazione |
| Compound / retime | Composizione delle mappe temporali; niente trascrizione del mixdown come unico riferimento |
| Undo / redo | Ripristino coerente di media, ownership, testo e stile |

Una parola tagliata a metà produce frammenti temporali segnalati, non un riconoscimento inventato. Gli intervalli sono semiaperti, così lo stesso istante non appartiene a entrambi i lati del taglio. Il renderer può riunire frammenti contigui della stessa parola; un taglio che rimuove fonemi deve restare revisionabile. Nascondere una caption non equivale a staccarla dall'audio. Mute audio e visibilità caption sono controlli distinti; cancellare il proprietario elimina il binding.

Correzioni di trascrizione condivise tra istanze: indicazione esplicita. Override di testo/stile della singola istanza: separati. I job ASR portano fingerprint sorgente e revisione; un risultato in ritardo non deve reintrodurre una clip cancellata o sovrascrivere correzioni più recenti.

ASR corretto e sincronismo del software sono verifiche diverse: un motore di timeline perfetto non rende infallibile il riconoscimento, soprattutto con canto e musica sovrapposta. Prevedere corpus italiano autorizzato, nomi propri, voci diverse e correzioni guidate; non promettere allineamento fonetico perfetto.

Preset iniziali proposti: classico, parola evidenziata, karaoke, pillola mobile, parola pop, due righe editoriali. Varianti di font, palette, bordo, ombra, posizione, enfasi e curva d'animazione sono dati versionati. Import/export del preset senza codice arbitrario. Le animazioni possono agganciarsi alla parola, alla pagina o alla clip. Non copiare template o asset proprietari di CapCut.

## 6. Audio, voiceover e silenzi

Prevedere sia registrazione voiceover con scelta ingresso, count-in e compensazione della latenza, sia ducking tra clip/bus già esistenti. Il gruppo Voce pilota il compressore sul gruppo Musica; entrambi arrivano al master. Nessuna necessità di incollare distruttivamente le clip. Key input pre-fader dopo gli effetti della voce, con mute rispettato, come politica iniziale esplicita.

Grafo audio aciclico; soglia, ratio, attack, release, knee, profondità e bypass salvati nel progetto. Pre-roll di analisi e stato del compressore deterministico nei seek e nei render a blocchi. Mai lanciare un processo o fare I/O nel callback audio. Preparazione del grafo fuori dal thread realtime e scambio sicuro di snapshot. Prima integrazione consentita con stem/cached preview, chiaramente marcata; nessuna promessa di elaborazione realtime finché misurata.

Riduzione rumore con modalità distinte per parlato e materiale musicale, controllo di intensità, confronto A/B e cache non distruttiva. FFmpeg offre `arnndn` per il parlato e altri filtri da valutare; questo non equivale a recuperare qualsiasi registrazione. Normalizzazione loudness, clip gain e compressione sono operazioni diverse. Misurare voce/gruppo, non normalizzare indiscriminatamente ogni frammento; verificare il master dopo il mix. `loudnorm` e `sidechaincompress` sono componenti disponibili, non un motore interattivo completo.

Rimozione silenzi: analizzatore produce una proposta di tagli, con soglia, durata minima, margini, protezione dei respiri e scelta dello scope ripple. Anteprima prima/dopo, salvataggio impostazioni e singolo Undo. Per materiale musicale niente cancellazione automatica delle pause per default. Applicare i tagli anche alle componenti video linkate e derivare nuovamente le caption; non usare la distruzione del file come stato del progetto. `silencedetect` può fornire candidati; VAD e Auto-Editor restano adapter valutabili, non dipendenze obbligatorie.

## 7. Video, effetti e transizioni

Prima versione: crop, scala, posizione, rotazione, opacità, esposizione, contrasto, saturazione, temperatura/tinta, nitidezza, fade e parametri animabili selezionati. Curve/HSL e LUT sono un'estensione pianificata, non una promessa di parità con ogni funzione CapCut. Catena effetti ordinata e bypass per effetto; preset salvabili, richiamabili e importabili con validazione dello schema.

Preview ed export devono condividere semantica, ordine e spazio colore degli effetti. V1 orientata a SDR Rec.709; file HDR richiedono conversione esplicita con anteprima o un errore informativo, non un export scolorito silenzioso. Conservare orientamento, pixel aspect ratio, gamma e metadati di sorgente. Proxy a risoluzione ridotta per il montaggio, originali nell'export finale.

Transizioni video iniziali: dissolvenza, fade e wipe; audio: fade e crossfade a potenza costante. Oggetti di transizione con ID delle clip agli estremi, durata e curva. Verificare le maniglie di sorgente: ridurre/rifiutare la durata in modo esplicito se insufficienti. Cut/ripple deve aggiornare o rimuovere transizioni non più valide. Tenere distinte transizione audiovisiva e semplice crossfade audio.

## 8. Preview, Remotion ed export

Un solo clock autorevole, guidato dall'audio quando in play. Il compositor segue il tempo di progetto; durante lo scrub richiede frame mirati. Budget iniziali da misurare su hardware concordato: 1080p/30 per il percorso comune e latenza interazione sotto 100 ms con proxy. Sono target di progetto, non risultati ottenuti.

Il renderer caption riceve un contratto JSON versionato con parole già mappate, font, preset, dimensioni, fps e seed. Un worker Node/Remotion isolato produce overlay RGBA/frame in cache; la stessa composizione genera l'export. Cache per hash di transcript, mapping, stile, font, versione renderer e impostazioni colore. Se una preview non è pronta, indicarlo invece di mostrare un effetto diverso. Le convenzioni alpha e colore devono essere dichiarate e testate.

Questo evita di trasformare la timeline JUCE in una pagina web. L'attuale build disabilita `JUCE_WEB_BROWSER`: incorporare un Player non è un'integrazione gratuita. L'efficienza degli overlay nativi va provata con uno spike prima di adottare definitivamente Remotion. Un adapter testuale più semplice può garantire un percorso di base senza Remotion, ma non fingere di riprodurne tutti i preset.

Job locali cancellabili e riprendibili dove possibile; snapshot immutabile del progetto per export. Scrittura su file temporaneo e pubblicazione atomica dell'output; mai sovrascrivere l'originale. Profilo iniziale proposto H.264/AAC SDR, formato verticale 1080x1920, quadrato 1080x1080, 4:5 1080x1350 e orizzontale 1920x1080. Bitrate, fps, qualità e loudness configurabili. Sono preset dell'app, non affermazioni sui limiti correnti delle piattaforme. Aggiornare separatamente le specifiche social e le guide di area sicura.

Esporta fotogramma PNG/JPEG alla risoluzione finale, con effetti e caption come nel viewer; opzione separata per il solo frame sorgente. Export video con caption impresse oppure file SRT/VTT, dichiarando che questi ultimi non conservano le animazioni. Salvare un report di render con versioni e impostazioni riproducibili.

## 9. Worker, REA, privacy e distribuzione

Interfacce tipizzate: ProbeMedia, BuildProxy, AnalyzeSilence, TranscribeSource, RenderCaptionOverlay, RenderProject e CancelJob. REA potrà invocare gli stessi servizi autorizzati, non riscrivere il documento da una seconda pipeline. La disponibilità di tali operazioni in REA è lavoro futuro, non una capacità del connettore qui dimostrata.

Default locale, nessun upload dei media senza consenso. Download dei modelli espliciti con verifica del checksum; niente download durante l'apertura di un progetto. Processi con argomenti strutturati, percorsi confinati e risorse limitate, mai stringhe shell da preset importati. Dipendenze e font con versioni e diritti di distribuzione documentati. Le pipeline GPU/codec vanno provate separatamente su macOS Intel, Apple Silicon e altri sistemi effettivamente supportati.

Prima della distribuzione: inventario licenze JUCE, Remotion, FFmpeg/build/codec, modelli e font; firma/notarizzazione, installer, aggiornamenti e crash recovery. Remotion non è una dipendenza universalmente gratuita. L'obiettivo di risparmio va valutato rispetto a sviluppo e manutenzione, non solo al costo dell'abbonamento sostituito.
