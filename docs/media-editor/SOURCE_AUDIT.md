# Audit della base e fonti

Audit del 22 ottobre 2026, sul commit `48502a95c92ce10a8a974dc792bae45027ab0ab0` di **AyraSoft/AyraTimelineLab**. Letture effettuate con il connettore GitHub: metadati repository, branch main, tree ricorsivo, README, CMakeLists e contratti TimelineModel/TimelinePanel. Non è stata eseguita una nuova build dell'intera applicazione.

## Evidenze e conseguenze

| Fonte nel repository | Evidenza | Conseguenza per il prodotto |
|---|---|---|
| README.md | Demo nativa JUCE; canvas e presentazione Ayra; nuova vista in memoria; la demo non riproduce audio | Riusare la presentazione, implementare/adattare un vero host media |
| modules/ayra_timeline_panel/README.md | Il modulo non possiede trasporto, media, DSP o ammissione Undo documentale | Non caricare queste responsabilità nel pannello |
| modules/ayra_timeline_panel/TimelineModel.h | Snapshot Owner/Lane/Event, CommandSink, clipLink e temporalGroup; tempi di presentazione double | Adapter dal documento canonico; non persistere lo stato UI come unico progetto |
| README.md e contratto modulo | Gruppo temporale reversibile distinto dal link e dal gruppo di disposizione; non è una compound né Glue PCM | Introdurre Sequence/CompoundInstance e AudioBus come concetti distinti |
| README.md | Codec legacy v1 e demo nuova con limiti di persistenza dichiarati | Nuovo formato versionato e migrazione esplicita, senza falsa compatibilità |
| CMakeLists.txt | C++20; JUCE 8.0.12 opzionalmente scaricabile; provider canonici esterni; JUCE_WEB_BROWSER=0 | Tenere l'app nativa; il renderer React richiede un adapter/worker |
| Contratto modulo | 10.000 righe logiche testate, non 10.000 canvas audio reali in benchmark | Non attribuire al prodotto capacità realtime non misurate |

La ricerca ha individuato anche `AyraSoft/ayra_timeline` e `AyraSoft/ayra_multiverse`, private. Non ne sono stati copiati sorgenti o font. L'architettura privata completa non è stata riesaminata in questo intervento: l'audit del riuso riguarda i contratti pubblici del Lab e i suoi riferimenti ai provider esterni. L'inventario dei motori audio/media realmente riutilizzabili è un deliverable della fase 0.

## Operazioni e perimetro della modifica

Il catalogo GitHub ha esposto create_file, update_file, create_blob, create_tree, create_commit, create_branch, update_ref e create_pull_request. I metadati della repository riportavano push abilitato; main risultava non protetto nella lettura eseguita. È stato creato un branch separato da quella revisione, senza aggiornare main. La clone via terminale ha fallito per risoluzione DNS di github.com: ciò non limita le operazioni del connettore, usato per letture e pubblicazione.

Nessuna modifica alla GUI approvata, al CMake root, al codec legacy o ai provider privati. Il prototipo aggiunto è autonomo e non entra automaticamente nel build/CI esistente. Non sono stati installati motori o modelli sul Mac dell'utente, né prodotti video con il nuovo editor.

## Fonti ufficiali consultate per le scelte esterne

- [Remotion Caption](https://www.remotion.dev/docs/captions/caption): formato dei dati con testo e timestamp; non un documento editoriale completo.
- [Remotion transcribe / Whisper.cpp](https://www.remotion.dev/docs/install-whisper-cpp/transcribe): integrazione del motore ASR separato, timestamp e requisiti audio.
- [Pagine di caption](https://www.remotion.dev/docs/captions/create-tiktok-style-captions): raggruppamento delle parole per la presentazione.
- [Overlay trasparenti](https://www.remotion.dev/docs/transparent-videos): possibilità di output con alpha; l'integrazione performante con JUCE resta da provare.
- [Licenza e prezzi Remotion](https://www.remotion.dev/docs/license/pricing) e [FAQ](https://www.remotion.dev/docs/license/faq): gratuità condizionata, non garanzia per qualsiasi organizzazione/distribuzione.
- [Filtri FFmpeg](https://www.ffmpeg.org/ffmpeg-filters.html): componenti per denoise, loudness, sidechain, silenzi e filtri audiovisivi. La loro presenza non fornisce da sola un editor interattivo.
- [JUCE 8 EULA](https://juce.com/legal/juce-8-licence/): verificare le condizioni applicabili al prodotto.

Le fonti esterne descrivono componenti; non attestano che questa applicazione sia implementata. Compatibilità, prestazioni, licenze della specifica distribuzione e qualità sul corpus Ayra restano gate del piano. La pagina legale FFmpeg non è stata recuperata dal browser in questa sessione per timeout: il relativo inventario di licenze/build/codec non è concluso.
