# Ayra Timeline Lab

## Timeline a lane separate, senza track header

La demo JUCE usa le primitive di presentazione **reali di `ayra_timeline`** e i
canvas clip del Lab. Nessuna fascia header viene aggiunta alle lane della vista
libera. Il contenuto scorre nel tempo; nome, accenti e controlli restano nel viewport.

**Identità e coerenza visuale:** i nomi ora hanno gerarchia traccia/lane e un
fondale compatto, misurato sul testo, non un gradiente a tutta altezza. La build
nativa richiede per default i sorgenti visuali originali di Multiverse in
`AYRA_SOURCE_DIR`. Lo snapshot locale è un opt-out esplicito e viene dichiarato
nella UI: non è il LookAndFeel completo di Multiverse.

La verifica corrente è descritta in
[verification/identity/STATUS.md](verification/identity/STATUS.md): build native
del painter e del pannello/input nei profili originale e snapshot, più test core.
La build completa dell'app a 19 lane e i sette target del report
[no-header precedente](verification/no-header/STATUS.md) appartengono al checkpoint
`278060c`; non vengono attribuiti automaticamente alla nuova revisione grafica.

### Utilizzo

L'app apre il progetto fittizio a **19 lane** in vista libera. L'accento laterale è
l'unico accesso alla selezione/riordino della lane (Cmd/Ctrl additivo, Shift intervallo).
M/S e Monitoring sono condivisi dall'owner. Open apre l'inspector fixture, non un
processore Tensor. I pulsanti `<` e `>` navigano la lane, compresi gli eventi locali
di Marker/Metrica/Counter e i punti delle linee continue.

Il menu **Visibilità** nasconde singoli controlli, meter e indicatore di resize.
Nome/gradiente e accento non possono sparire: nome sinistra/destra, accento
sinistra/destra/entrambi. Click e drag sotto il nome vanno al canvas; doppio click
sul titolo rinomina la traccia condivisa; sul sottotitolo rinomina solo la lane. Il bordo inferiore ridimensiona soltanto
le lane manuali. Marker, Metrica e Counter restano compatti.

I meter sono **sintetici, opzionali e spenti al primo avvio**, anche sulle lane
non audio; il loro painter è quello Ayra già presente nel Lab. I colori di tipo
sono modificabili separatamente dal colore della traccia. Le corsie globali
usano colori neutri del tema. La demo non riproduce audio.

### Build

Servono un checkout ufficiale JUCE 8, `ayra_curves`, `ayra_timeline` a
`caa00902884214bf808befae294efe59c1b088fa` o revisione compatibile, e il submodule
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

`--legacy` e `--session "file.json"` preservano il vecchio banco e il codec v1;
l'HTML incluso resta la sua specifica storica, non la nuova GUI. La demo del nuovo
pannello è in memoria: non esporta un JSON v1 che perderebbe owner e relazioni.

Gruppo temporale, link cross-lane e gruppi di disposizione sono distinti. Il gruppo
temporale è ancora relazione reversibile, **non Glue PCM né un contenitore visuale
unificato**. I canvas Point/Span/Continuous conservano l'editing locale esistente;
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
