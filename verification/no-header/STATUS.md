> REPORT STORICO del checkpoint locale `278060c1029400e9f111a7eea5d4306b51586050`.
> La verifica corrente dei nomi e dei sorgenti grafici è in `../identity/STATUS.md`.
> Le prove seguenti non costituiscono una nuova esecuzione della GUI corrente.

# Verifica nativa no-header — 20 ottobre 2026

Base documento: `AyraSoft/AyraTimelineLab@b214bcc7f960bab44d5e66f4827d8615a813c739`.
Il report descrive **sorgenti locali testati**; non prova da solo la pubblicazione
su GitHub. Il checkpoint di consegna distingue il commit locale dal ref remoto.
Nessun nuovo test è attribuito a una revisione differente senza ricompilazione.

## Ambiente

Linux x86_64, Clang 17.0.0 per C e C++, GNU 14.2.0 per i profili headless,
CMake 3.31.6. Test grafici su Xvfb 1600x1120 e Openbox; screenshot della finestra
JUCE reale (client 1480x1040), non immagini generate. Questo non è un certificato
pixel-perfect della build macOS originale.

Dipendenze lette, non pubblicate né ricopiate nella repository pubblica:

| Provider | Identità |
|---|---|
| JUCE ufficiale 8.0.12 | `29396c22c93392d6738e021b83196283d6e4d850` |
| ayra_timeline | `caa00902884214bf808befae294efe59c1b088fa` |
| ayra_curves, tree letto | `86da4a5f4bb3c9dade434be64f5791b38f4383df` |
| ayra_curves, CurveSegment blob | `0a2b3c6ac6822d2fdc4c1ff4c9d5b3ce95e634f7` |
| melatonin_blur, gitlink originale Lab | `fcc18578d9db3f9400c74ee1bb2eb1070ee959f9` |

## Eseguito

| Verifica | Esito e ambito |
|---|---|
| Native Debug Clang: configure/build/link | PASS, app e target di test |
| Native CTest | 7/7 PASS: core, presentazione, transazioni, fixture, codec, componenti clip, interazioni pannello |
| Nucleo GNU Debug | 4/4 PASS |
| Nucleo GNU Release | 4/4 PASS |
| Nucleo Clang ASan/UBSan, leak detection | 4/4 PASS, nessuna diagnostica nei run registrati |
| Audit confini Python | PASS; sola analisi statica, non benchmark |
| Avvio demo con canvas reali | PASS, screenshot Dark/Light, menu, rename e resize |
| Launcher e chiusura della finestra | PASS, exit 0; cache bitmap rilasciate prima dello shutdown JUCE |
| Interazioni OS XTest | Smoke reale di menu, toggle meter, tema, doppio click nome, Escape, drag bordo e Undo vista |
| macOS/Windows, DSP/Multiverse, frame-time reale | NOT_RUN |

Conteggi osservati: **208** asserzioni core, **939** presentazione, **106** fixture,
**349** interazioni native. Sono asserzioni, non altrettanti test case indipendenti.
Il test nativo usa il pannello effettivo con canvas strumentati: 10.000 lane logiche,
300 posizioni di scroll e massimo **15 righe residenti** nella geometria scelta.
Non è un benchmark di 10.000 editor Audio/MIDI né misura il frame presentato dal GPU.
I canvas reali sono esercitati separatamente nell'avvio della demo a 19 lane.

Il vecchio linking fallito su `FloatVectorOperations` è risolto collegando il modulo
JUCE audio_basics. Il primo tentativo GUI senza window manager incontrava un errore
X11 di ambiente; i run finali usano Openbox. Il collaudo di chiusura ha inoltre
rilevato bitmap nella cache chrome statica dopo la distruzione dei peer: lo shutdown
dell’app ora rilascia la cache Ayra esistente e le immagini JUCE non più usate,
prima dei detector statici. Il run finale termina con exit 0 senza quella diagnostica.
Non sono stati sostituiti JUCE o Melatonin
con stub per far passare la verifica.

## Riproduzione

```sh
cmake -S . -B build/native \
  -DJUCE_SOURCE_DIR="/percorso/JUCE" \
  -DAYRA_MODULES_DIR="/percorso/provider-parent" \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build build/native --config Debug --parallel 2
ctest --test-dir build/native -C Debug --output-on-failure
python3 tools/check_panel_boundary.py
python3 tools/generate.py --check
```

Su Linux predisporre display e window manager prima di CTest. Per ASan/UBSan è stato
usato il profilo GUI=OFF, `AYRA_LAB_SANITIZE=ON`, Clang e `ASAN_OPTIONS=detect_leaks=1`.
La build nativa completa non è stata eseguita sotto sanitizer.

## Evidenze

[Build](native-build.log), [CTest](native-ctest.log),
[interazioni](native-interactions.log), [avvio e chiusura](native-launch-and-shutdown.log),
[conteggi](assertion-counts.log), [Debug](headless-debug.log),
[Release e sanitizer](headless-release-sanitizers.log), [audit](source-boundaries.log).

Le cinque immagini storiche sono conservate nell'archivio di consegna
`AyraTimelineLab-no-header-278060c.zip`; non sono ripubblicate qui come screenshot
della revisione corrente. Il manifesto storico conserva i relativi hash.

Gli screenshot attestano la composizione eseguita, non l'assenza di ogni regressione
nelle gesture. I test nativi verificano i contratti di forwarding con contatori di
input e gli ID delle righe. I limiti funzionali rimanenti sono elencati in
`docs/TIMELINE_PANEL_IMPLEMENTATION.md`: persistenza nuova demo, Glue reale,
source-aware editing, stato locale degli eventi e collaudo di piattaforma.
