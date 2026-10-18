# Stato delle verifiche

## Aggiornamento clip - 16 ottobre 2026

Ambiente: macOS x86_64, AppleClang 17, checkout JUCE locale (i test riportano JUCE 9.0.3) e `AYRA_SOURCE_DIR` collegato al checkout locale di `ayra_multiverse`.

| Prova | Esito | Ambito reale |
| --- | --- | --- |
| GUI Debug, compilazione e link | PASS | App JUCE, bridge Ayra e layer `app/clips/`. |
| GUI Release, compilazione e link | PASS | Stesso percorso nativo in configurazione ottimizzata. |
| CTest Debug e Release | 4/4 PASS | Core, fixture, codec JUCE e 10 asserzioni strutturali delle clip. |
| Completezza dei sorgenti | PASS | `tools/verify_sources.py`, inclusi i nuovi file clip. |
| Avvio GUI e confronto visuale/gesture | NOT_RUN | Nessuna certificazione pixel-perfect, input, focus o DPI. |

Le righe seguenti conservano l'evidenza storica della sessione iniziale del 15 ottobre; non sostituiscono l'aggiornamento sopra.

Data della sessione: **15 ottobre 2026**. Ambiente: Linux x86_64, GCC 14.2.0, Clang 17.0.0, Python 3.13.5. Il vettore sorgenti e' identificato da `source-manifest.json`.

| Prova | Esito | Ambito reale |
| --- | --- | --- |
| CMake/GCC Debug, 2 casi CTest | PASS | Nucleo C++ del Lab + integrita' delle fixture generate. |
| Asserzioni nel test core | 184 PASS | Ruoli, selezione, ordine, mapping, validazione, Undo e fixture audibilita'. |
| CMake/GCC Release, 2 casi CTest | PASS | Stesso nucleo C++ reale, non una replica nei test. |
| Clang ASan + UBSan, 2 casi CTest | PASS | Core e test; **non GUI, JUCE o engine**. |
| Browser Chromium/Playwright | 47 controlli PASS | DOM, alcuni gesti reali, alcune API di stato, mapping, export e layout. |
| Completezza dei sorgenti | PASS | File attesi presenti, fixture coerenti, HTML senza dipendenze esterne. |
| Configurazione GUI nativa | BLOCKED | Nessun checkout JUCE disponibile; download disabilitato. |
| Compilazione `app/` e link JUCE | NOT_RUN | Nessun risultato implicito dai test del core. |
| Test codec JUCE aggiunti in sorgente | NOT_RUN | Richiedono JUCE e il target `ayra_lab_codec_tests`. |
| Bridge dei token canonici Ayra | NOT_RUN | Non compilato contro il checkout locale. |
| Mouse/focus/accessibilita'/DPI JUCE | NOT_RUN | Checklist in `docs/VERIFICHE_NATIVE.md`. |
| Audio, plugin, hardware, RT e frame-time | NON ESERCITATI | Il Lab non contiene quei sottosistemi. |

## Riproduzione

```sh
cmake --preset core
cmake --build --preset core
ctest --preset core
python3 tests/browser_test.py --chromium /percorso/chromium
python3 tools/verify_sources.py
```

Per il test codec, dopo una configurazione che abbia trovato JUCE:

```sh
cmake --build build/native --config Debug --target ayra_lab_codec_tests
ctest --test-dir build/native -C Debug -R codec_regressions --output-on-failure
```

Il runner browser carica l'HTML autonomo in una pagina Chromium tramite `set_content`; il criterio e' DOM eseguibile senza risorse di rete. Non attesta le policy enterprise di apertura `file://` del computer dell'utente. Gli screenshot a 1620x1200 sono del browser e non costituiscono una verifica del DPI nativo.

La voce "184" e' il numero di asserzioni nella suite core, non 184 test GUI. La voce "47" comprende interazioni browser e verifiche di stato, non 47 percorsi end-to-end della produzione.

Non sono state eseguite misure di prestazione. Nessun dato di questo banco puo' essere riusato come certificazione di Multiverse o del suo codice audio.
