# Consegna lane compatte e Group — 21 ottobre 2026

Base integrata: `main` **88cc8f16202b1c99a5d5670bb757b8b64d69c1ba**.
La consegna risolve anche la PR #1 (head verificato prima della pubblicazione:
`471ada2288745567e468ee987735f6034561a7ce`), senza riscriverne la cronologia.
La specifica aggiornata e' `docs/COMPACT_LANES_AND_GROUPING.md`.

## Esecuzioni effettive

Linux x86_64; GNU C++ Debug/Release, Clang ASan/UBSan per il modello portabile;
JUCE **8.0.12**, Xvfb + Openbox per l'applicazione e i test nativi.
I risultati seguenti sono di questa revisione, non ereditati dalle verifiche precedenti.

| Verifica | Esito |
| --- | --- |
| Headless Debug | 6/6 PASS |
| Headless Release | 6/6 PASS |
| Headless Release Clang ASan + UBSan | 6/6 PASS |
| Build dell'applicazione e di tutti i target nativi | PASS |
| CTest nativo completo | **11/11 PASS**, nessuna assertion JUCE o segnalazione di leak nel log finale |
| Canvas reali: geometria, input, gruppi, ancoraggi e pixel | 4.221 asserzioni PASS |
| Componenti clip / cursori / geometrie condivise | 1.491 asserzioni PASS |
| Pannello nativo / editor titolo / navigazione | 604 asserzioni PASS |
| Card: geometria, materiale, hover e rendering | 252.671 asserzioni PASS |
| Packer di lane eventi effettivamente usato dall'app | 37.797 asserzioni PASS |
| Controller portabile | 956 asserzioni PASS |
| Store, gruppi, transazioni e Undo/Redo | 154 asserzioni PASS |
| Core / codec JUCE | 208 / 14 asserzioni PASS |
| Avvio app reale, cattura finestra, chiusura WM_DELETE_WINDOW | PASS, exit 0, nessuna assertion/leak |
| Confini provider, adapter visuale, fixture generate, diff e script shell | PASS |

I conteggi comprendono cicli su pixel/geometrie: non sono altrettanti test
end-to-end indipendenti. Il test di residenza esercita 10.000 lane logiche con
un massimo di 15 componenti residenti nella finestra provata; non e' un benchmark DSP.

`CanvasInteractionTests.cpp` include la stessa composition TU dell'applicazione:
AudioClipCanvas, MidiClipCanvas, LineClipCanvas, ImageClipCanvas, linea continua,
bridge, timer e renderer reali. La suite storica del pannello mantiene inoltre
canvas strumentati per isolare virtualizzazione e instradamento degli eventi.

Sono verificati: minimo 32 px con barra di taglio interna; soglia warp a 64 px
di lane e linee verticali conservate; MIDI 32/256/400 con effettivo LOD;
preview read-only e divieto di inserimento invisibile; tratteggio dei soli fade;
strumento Slip/Sizing Moves Contents e ancoraggio sorgente opposto; cursore Glue
su modificatore fermo; singolo/doppio clic senza split e con un solo Undo;
selezione, target fuori viewport, unione di gruppi, blocchi, revisioni obsolete,
annullamento per drag/Escape; card speculare, titolo espandibile e titolo passivo
che scompare anche sopra preview non editabile.

Le prove iniziali hanno trovato e corretto padding dell'espansione del titolo,
contratti della maschera speculare, inserimento di punti nella linea continua
compatta e ordine di distruzione dei cursori custom JUCE. Il test peer del
ritardo hover usa ora attesa condizionale limitata: il tempo esatto di 500 ms
resta verificato nella macchina di stato pura, senza assumere scheduling
hard-real-time del timer nativo sotto carico. Il log finale completo e' allegato.

## Confini e dipendenze

Il profilo visuale eseguito e' **snapshot ayra_ui**, dichiarato anche dall'app.
Non viene dichiarata una build live dell'intero Multiverse, ne' una prova macOS
Intel/Windows, audio/DSP o dei media reali: il Lab usa le sue fixture sintetiche.
Questi limiti di piattaforma/profilo non riguardano i canvas reali effettivamente
compilati ed esercitati. Rimangono warning preesistenti sulla composition TU;
non si dichiara una build priva di warning.

Il provider matematico e' il codice canonico esterno di `AyraSoft/ayra_math`
(commit e hash nel manifest), non uno stub. L'adapter pubblicato contiene solo
include. I cinque header di presentazione `ayra_timeline` sono esterni e
verificati per hash. Nessun corpo di provider privato o font viene pubblicato.

## Riproduzione

```sh
cmake -S . -B build/core -DAYRA_LAB_BUILD_GUI=OFF \
  -DAYRA_MODULES_DIR=/path/to/provider-parent -DCMAKE_BUILD_TYPE=Debug
cmake --build build/core --parallel 2
ctest --test-dir build/core --output-on-failure

cmake -S . -B build/native -DJUCE_SOURCE_DIR=/path/to/JUCE-8.0.12 \
  -DAYRA_MODULES_DIR=/path/to/provider-parent \
  -DAYRA_LAB_REQUIRE_MULTIVERSE_VISUALS=OFF -DCMAKE_BUILD_TYPE=Debug
cmake --build build/native --parallel 2
# Linux: display X11 attivo con window manager (Xvfb + Openbox nel test).
ctest --test-dir build/native --output-on-failure
```

Il parent dei provider contiene `ayra_timeline` e `ayra_math` (oppure il modulo
standalone `ayra_curves`). Su Linux servono i normali header di sviluppo JUCE.
La CI pubblica esegue i contratti indipendenti dai provider privati anche sulle
pull request. Le PNG native sono nel pacchetto di evidenze consegnato; il loro
manifest SHA-256 e' versionato qui.
