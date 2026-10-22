# Piano di lavoro e criteri di accettazione

Questa è una proposta di sequenziamento, non un rilascio completato. Il dettaglio architetturale è in [ARCHITECTURE.md](ARCHITECTURE.md). Nessuna fase futura risulta eseguita soltanto perché descritta qui.

## Consegna iniziale di questa modifica

Audit della base pubblica, decisioni architetturali, confini di riuso, piano verificabile e prototipo C++20 isolato per l'ownership dei sottotitoli. Il prototipo verifica move, trim, split, delete, duplicate e batch atomici a velocità 1x. Non integra la GUI, non trascrive e non salva progetti. I test eseguiti sono elencati nel [report](../../verification/media-editor-foundation/STATUS.md).

## Fasi

| Fase | Lavoro | Criterio per considerarla terminata |
|---|---|---|
| 0 — Fattibilità e baseline | Inventario provider reali, build nativa riproducibile, hardware target, licenze, corpus autorizzato, spike decoder/audio/overlay Remotion | Aprire un clip reale, cercare un frame, riprodurre audio e mostrare overlay; misurare consumo e tempi sul Mac target; decisione motivata sul renderer |
| 1 — Documento e storage | ID, timebase, asset registry, sequenze, comandi, Undo/redo documentale, save/open, autosave, relink e migrazioni | Round-trip senza perdita di riferimenti; recupero dopo crash simulato; errori non distruttivi su file/schema non valido |
| 2 — Montaggio reale | Adapter del pannello Ayra, playback/preview, import audio/video/immagini, proxy, split/trim/move/delete/ripple, detach/link e selezione | Stesso risultato nel viewer e nell'export base dopo una serie di edit; link A/V e lock rispettati; nessuna dipendenza dallo stato di un canvas smontato |
| 3 — Caption vincolate alla sorgente | Whisper.cpp locale, revisione testo/timing, CaptionBinding, proiezione, gestione risultati asincroni, sei preset iniziali | Move/split/trim/delete/ripple/duplicate e Undo testati su media reali; nessuna caption orfana; riconoscimento italiano valutato separatamente |
| 4 — Gruppi e audio | Edit group, compound annidate, bus, registrazione voiceover, denoise, loudness, sidechain tra clip/gruppi, analisi silenzi | Voce frammentata pilota musica frammentata senza mixdown obbligatorio; seek/export senza salti nel ducking; raggruppa/disgrega senza perdita |
| 5 — Video e preset | Inspector video, catena effetti, keyframe selezionati, profili SDR, import/export preset, transizioni audio/video | Confronto frame preview/export; preset round-trip; comportamento esplicito per HDR, dipendenze mancanti e maniglie insufficienti |
| 6 — Output produttivo | Coda render, cancellazione, profili social, estrazione frame, SRT/VTT, raccolta progetto, diagnostica | Riaprire il progetto su un altro ambiente con media raccolti; output verificati; un job fallito non danneggia progetto e originali |
| 7 — Consolidamento | Stress, memoria, CPU/GPU, regressioni visive del Lab, installer e signing, test su hardware reale, documentazione | Tutta la matrice sotto verde, limiti dichiarati, nessun errore critico; approvazione su progetti reali prima di abbandonare CapCut |

Dipendenze: il documento precede l'editing; l'editing precede gli automatismi; le compound richiedono SourceMap/ownership; gli effetti precedono la verifica di parità definitiva dell'export. Dopo fase 2 si sviluppano in parallelo caption e audio, non due modelli di progetto incompatibili. Anticipare uno spike Remotion evita di scoprire troppo tardi problemi di latenza o distribuzione.

## Prima fetta utilizzabile

Prima di ampliare effetti e preset: importare un video con audio, salvarlo, riaprirlo, tagliarlo, generare caption, spostare/tagliare l'audio con caption coerenti, esportare un verticale. Questo percorso end-to-end è la prima milestone di prodotto. Una demo bella con salvataggio o sincronismo inaffidabili non la supera.

## Matrice di accettazione

| Area | Scenari obbligatori |
|---|---|
| Persistenza | Save/Open/Save deterministico; autosave; crash durante save; media rinominati/spostati; migrazione; schema futuro; lock multi-processo |
| Edit e link | Trim con handles; split al frame e al campione; duplicazione; ripple con lane escluse; elementi bloccati; detach senza cancellazione audio; Undo/redo |
| Gruppi | Gruppo distinto da bus e compound; annidamento; rifiuto cicli; Rendi unico; spostamento e disgregazione con identico contenuto |
| Caption | Confini parola/taglio; pezzi di parola; clip duplicate; cancellazione owner; caption nascoste; edit durante ASR; correzioni manuali non perse |
| Tempo | 25, 30 e 30000/1001 fps; 44.1/48 kHz; VFR; clip lunghe; retime positivo; mappa di nested sequence; assenza di deriva cumulativa |
| Audio | Denoise voce e strumento valutati separatamente; loudness misurato; assenza di clipping; sidechain con gap/crossfade; pre-roll dopo seek; stessa resa offline/preview |
| Silenzi | Parlato basso, respiri e riverberi; proposta vs applicazione; margini; link video; ripple multi-lane; annullamento in una sola operazione |
| Immagine | Orientamento smartphone; range colore; gamma; crop; alpha; preset mancanti; confronto degli effetti preview/export e frame estratto |
| Transizioni | Handles insufficienti; clip eliminata; durata oltre il limite; effetti a cavallo dei tagli; curva audio continua |
| Prestazioni | Corpus target, frame persi, latenza scrub, RAM/cache; tanti clip reali; non usare il test di righe sintetiche come benchmark video |
| Distribuzione | Mac Intel e Apple Silicon come verifiche distinte; dipendenze offline; percorsi Unicode; sicurezza preset/job; licenze e font |

## Stime e gestione del rischio

Ordine di grandezza progettuale, da rivedere dopo fase 0: 6–10 settimane per una prima fetta realmente utilizzabile; circa 4–6 mesi per una V1 che copra l'intero perimetro, ipotizzando due sviluppatori esperti dedicati e una disponibilità regolare per i test. Non è una promessa: riuso effettivo di motori esistenti, prestazioni del Mac, licenze, color pipeline e packaging possono cambiare molto l'effort. Un solo sviluppatore o lavoro discontinuo allunga il calendario.

Separare le stime in giornate effettive durante il backlog tecnico. Prima si finanziano salvataggio, timing, audio e test; poi la quantità di preset. Rinviare collaborazione cloud, marketplace, pubblicazione automatica sui social, plugin di terzi, tracking avanzato, rimozione sfondo e speed ramp complessi: non sono prerequisiti della richiesta attuale.

Rischi prioritari: distanza tra Lab e motore reale; parità preview/export; audio realtime e latenza; frame rate variabile; accuratezza ASR sul materiale musicale; gestione delle licenze. Ogni rischio ha un gate sperimentale prima di entrare nel percorso critico. Conservare CapCut come riferimento sui progetti campione finché il round-trip e l'export non sono verificati: l'eliminazione dell'abbonamento non precede la validazione del sostituto.
