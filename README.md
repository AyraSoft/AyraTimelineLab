# Ayra Timeline Lab

## Timeline a lane separate, senza track header

La demo JUCE usa le primitive di presentazione **reali di `ayra_timeline`** e i
canvas clip del Lab. Nessuna fascia header viene aggiunta alle lane della vista
libera. Il contenuto scorre nel tempo; nome, accenti e controlli restano nel viewport.

**Identità flottante:** la lente segue l'HTML approvato in
`fixtures/frosted-lens.html`: Performer e tipo/pallino sopra, traccia sotto.
La card resta centrata verticalmente anche durante il resize. Il Performer è a
sinistra; tipo e nome traccia sono allineati a destra. Il bordo sinistro del
fondale (blur e riflesso compresi) scompare prima degli angoli.
Il nome della traccia e le frecce proteggono la card dall'hover-hide. Il doppio
click sul nome apre l'editor inline: Enter/perdita focus confermano una transazione
owner condivisa, Escape annulla. I nomi troncati si espandono orizzontalmente al
passaggio del mouse, senza spostare le clip. Performer, tipo e stati restano passivi.
Sopra il restante contenuto editabile la card scompare dopo 500 ms, con un solo
coordinatore; resta nascosta fino all'uscita dalla propria area.
M/S seguono il nome prima delle frecce; le piccole icone attive si impaginano sotto
le frecce. Le lane di eventi misurano i widget: 24 px per una banda di punti,
32 px per uno span con barra di taglio, crescita solo per sovrapposizioni reali.
La card passa automaticamente a una riga nelle lane basse, senza determinarne l'altezza.

La build nativa richiede per default i sorgenti visuali originali di Multiverse
in `AYRA_SOURCE_DIR`. Lo snapshot locale è un opt-out dichiarato nella UI.
Vedere [contratto della lente](docs/FROSTED_LENS.md) e
[contratto di navigazione/stato](docs/CARD_STATUS_AND_NAVIGATION.md) e
[card adattiva e rinomina](docs/ADAPTIVE_CARD_AND_TITLE_EDIT.md) e
[verifica corrente](verification/adaptive-title/STATUS.md).

### Utilizzo

L'app apre il progetto fittizio a **19 lane** in vista libera. L'accento laterale è
l'unico accesso alla selezione/riordino della lane (Cmd/Ctrl additivo, Shift intervallo).
M/S e Monitoring sono condivisi dall'owner. Open apre l'inspector fixture, non un
processore Tensor. I pulsanti `<` e `>` navigano la lane, compresi gli eventi locali
di Marker/Metrica/Counter e i punti delle linee continue.

I controlli laterali e i meter sono tutti **nascosti per default**, senza eliminarne
le API. Il menu **Visibilità** permette di riattivarli singolarmente e separa la
navigazione nella card dalla navigazione laterale legacy.
Nome/gradiente e accento non si possono disattivare dal menu: nome sinistra/destra,
accento sinistra/destra/entrambi. La lente ha la sola scomparsa temporanea
content-aware descritta sopra. Fuori dal nome e dalle frecce anche il doppio click raggiunge il canvas. Il bordo inferiore ridimensiona soltanto
le lane manuali. Marker, Metrica, Counter, point e span sono dimensionati automaticamente dal contenuto.
Riattivando i controlli laterali, ogni lato conserva il proprio unico well recessed. Le frecce sono
i chevron singoli `<`/`>`, non icone di trasporto. Gli accenti laterali usano
soltanto il colore della traccia: il colore tipo resta nel pallino della card.
Solo i controlli laterali realmente visibili impongono il proprio floor: 56 px per due righe di bottoni,
32 px per la sola navigazione, senza imporre quel minimo alle lane prive di bottoni.

I meter sono **sintetici, opzionali e spenti al primo avvio**, anche sulle lane
non audio; il loro painter è quello Ayra già presente nel Lab. I colori di tipo
sono modificabili separatamente dal colore della traccia. Le corsie globali
usano colori neutri del tema. La demo non riproduce audio. Anche gli stati DSP/canvas/monitoring sono readback
dimostrativi espliciti, non un motore attivo o valori dedotti dal tipo della lane.

### Build

Servono un checkout ufficiale JUCE 8, `ayra_curves`, `ayra_timeline` a
`ddf196e94de744f189b61b75277c2f9df3b8598f` o revisione compatibile, e il submodule
Melatonin già dichiarato dalla repository. Nessun provider privato è copiato qui.

```sh
cmake -S . -B build/native \
  -DJUCE_SOURCE_DIR="/percorso/JUCE" \
  -DAYRA_MODULES_DIR="/percorso/moduli-ayra" \
  -DAYRA_SOURCE_DIR="/percorso/ayra_multiverse" \
  -DAYRA_LAB_REQUIRE_MULTIVERSE_VISUALS=ON \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build build/native --config Debug --parallel 2
ctest --test-dir build/native -C Debug --output-on-failure
```

Su Linux i test GUI richiedono un display e un window manager disponibili; per
la verifica registrata sono stati usati Xvfb e Openbox. Su macOS i comandi usano
l'SDK e il compilatore locali. `run.command`/`run.sh` leggono le variabili
`JUCE_SOURCE_DIR`, `AYRA_MODULES_DIR` e `AYRA_SOURCE_DIR`, oppure i percorsi di un build cache esistente.
Non contengono percorsi personali né effettuano download.

Il solo nucleo non richiede JUCE o rete, ma richiede il provider di presentazione:

```sh
cmake -S . -B build/core -DAYRA_LAB_BUILD_GUI=OFF \
  -DAYRA_MODULES_DIR="/percorso/moduli-ayra" -DCMAKE_BUILD_TYPE=Debug
cmake --build build/core
ctest --test-dir build/core --output-on-failure
```

### Confini

`--legacy` e `--session "file.json"` preservano il vecchio banco; il codec legge
anche i formati v1/v2 e scrive il v3, che distingue esplicitamente una EditSource
container anche quando una finestra ottenuta con split non contiene clip. L'HTML incluso
resta la specifica storica, non la nuova GUI del pannello.

Container Glue, link cross-lane e gruppi di disposizione sono distinti. Glue
reparenta clip sorelle in una clip annidabile con Warp/Gain propri; i figli chiusi
sono preview-only e si editano nel canvas temporaneo del CallOutBox. Nella fixture
questa forma corrisponde a `ayra::timeline::EditSource`, che resta l'owner prodotto.
I canvas Point/Span/Continuous conservano l'editing locale esistente;
lo stato sopravvive allo smontaggio ma non acquista per questo Undo documentale,
Split/Stretch source-aware cross-owner o persistenza canonica.

Vedere [contratti del modulo](modules/ayra_timeline_panel/README.md) e
[mappa di implementazione](docs/TIMELINE_PANEL_IMPLEMENTATION.md). Il vecchio
README e i report della prima iterazione restano riferimenti storici.

### Quali renderer vengono realmente usati?

Con `AYRA_SOURCE_DIR`, token, glyph, superfici, meter e quattro controlli sono
inclusi direttamente dal checkout esterno. I sette painter JUCE di
`ApplicationLookAndFeel_Buttons.cpp` usano il corpo originale, adattando nel build
solo include e qualificatore della classe. Non vengono copiati nella repo pubblica.
Menu, altri controlli JUCE e composizione restano adattatori del Lab: non è il
`ApplicationLookAndFeel` completo. Il nome flottante è una nuova ricetta host-side,
non un widget esistente spacciato per canonico.

Vedere [mappa esatta del riuso e revisione dei nomi](docs/TIMELINE_IDENTITY_AND_VISUAL_SOURCES.md).
La build genera `visual-source-manifest.txt` con SHA-256 e percorsi dei file letti.
Per scegliere deliberatamente il profilo compatibilità, lasciare `AYRA_SOURCE_DIR`
vuoto e passare `-DAYRA_LAB_REQUIRE_MULTIVERSE_VISUALS=OFF`.
