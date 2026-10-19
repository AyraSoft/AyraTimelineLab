# Ayra Timeline Lab

Banco separato per progettare e verificare gli header della Timeline Multiverse.

**Consegna:** mockup HTML interattivo + sorgenti di una applicazione desktop C++20/JUCE + nucleo C++ testato. Non e' un installer, non contiene un motore audio e non modifica `ayra_multiverse`.

**Stato verificato, 16 ottobre 2026:** 47 controlli browser; 184 asserzioni del nucleo C++ in Debug, Release e Clang ASan/UBSan. La GUI nativa, il nuovo layer clip e l'adattatore ai token Ayra sono stati compilati e linkati in Debug e Release su macOS x86_64; quattro target CTest risultano verdi, compreso il test strutturale delle clip. L'app non e' stata avviata: la parita' visiva e delle gesture resta quindi una verifica manuale, non una certificazione pixel-perfect della build Multiverse.

## Aprire subito il mockup

Aprire `web/ayra-header-lab.html` in un browser desktop. E' un file autonomo: CSS, JavaScript e dati sono inclusi; non richiede server, installazione, font aggiuntivi o connessione.

**Confronta header** mostra i quattro ruoli affiancati. Disattivarlo riporta alla Timeline. La barra **Raggruppata / Libera** cambia soltanto la disposizione. In Libera, trascinare il grip a sinistra del nome per riordinare; Song Main deve essere prima sbloccata con **PIN**. Il mini-mixer segue identita', selezione, ordine e valori della stessa sessione.

Click sul corpo dell'header seleziona il Tensor; Ctrl/Cmd aggiunge o toglie dalla selezione e Shift seleziona un intervallo. Il click sulla clip seleziona invece l'evento audio. Disclosure e bordo inferiore hanno compiti distinti: la prima apre le lane, il secondo regola l'altezza. Escape annulla il resize non confermato. I pulsanti non avviano il riordino.

Il doppio click su una waveform apre l'editor della clip. **Channels / Mapping...** mostra la lista del suo ordine condiviso: il trascinamento o le frecce preparano una bozza; Applica aggiorna tutte le clip della traccia, con un solo Undo. Nessun cavo o colonna delle destinazioni. L'esempio della voce ha L in posizione 1 e R in posizione 4, mantenendo vuote le due righe intermedie.

**Salva** permette di esportare la sessione JSON oppure una nuova copia autonoma dell'HTML con i valori correnti. Si puo' reimportare esclusivamente il formato del Lab, non un progetto `.aym` o `.ayt`.

## I quattro ruoli

| Ruolo | Identita' e comportamento |
| --- | --- |
| Song Track | Traccia appartenente alla Song; nome, colore, M/S, controlli e destinazione Song Main. |
| Performer Track | Appartenenza performer esplicita; ingressi multistream e destinazione MIX riconoscibili. |
| Performer MIX | Entita' di convergenza reale, non una cartella. Conserva controlli e visibilita' quando il gruppo viene ripiegato. |
| Song Main | Finale della Song, Mute Program senza Solo. Posizione pinnata di default, ma sbloccabile in Libera. |

La fixture usa colori espliciti: backing verde, performer e MIX turchese, Song Main oro. Il colore e' un'identita', non un effetto della posizione. Non vengono inferiti nuovi ruoli da nomi o numero di connessioni.

### Controlli esposti

M/S quando previsti, enable, bypass, gain e pan, monitoring del bus audio, Graph, Routing, automazioni, impostazioni, rinomina, colore, buffer, oversampling 2x, espansione, resize e pin. Buffer e oversampling sono disponibili nei controlli estesi e nelle impostazioni.

I controlli agiscono **sui dati dimostrativi**. Graph e Routing aprono descrizioni fixture; non caricano plugin e non modificano un grafo. Le automazioni sono un inventario dimostrativo. Non vengono mostrati Record/Write o meter finti come se fosse presente un motore audio. Gain e pan nel Lab sono valori assegnati della fixture: nel prodotto dovranno essere risolti dai parametri canonici della Channel Strip, non trasformarsi in un secondo gain nascosto.

La regola di audibilita' del banco serve soltanto a mostrare stati diretti/ereditati e Program/Cue. Non e' un porting del resolver runtime Multiverse, non modella tutte le Route, il solo globale o ogni configurazione del grafo.

## Perche' un progetto separato

L'HTML e' la specifica visuale rapidamente modificabile. L'app JUCE serve a collaudare i problemi che il browser non puo' certificare: focus, cattura del mouse, font, scaling, menu, lifetime dei componenti, resize e trascinamento nativi.

Il progetto separato non e' una seconda DAW. Non contiene decoder, AudioDeviceManager, trasporto, plugin host, rete MIDI/OSC/DMX o salvataggi musicali. Usa un piccolo Store fixture e un solo `TrackHeader`, configurato per ruolo e capacita'. Timeline e mini-mixer sono due consumer dello stesso Store.

La matematica generica delle curve vive nel modulo headless `ayra_curves` ed e'
consumata dal Lab, dai moduli Ayra e da Multiverse. Il Lab conserva soltanto i
view-model specifici delle clip e l'ownership delle gesture nelle canvas. Paint,
hit-test, valutazione e split usano le stesse primitive canoniche: nessuno di
questi consumer mantiene una formula parallela. Il browser resta un riferimento
di design; non e' l'implementazione runtime C++.

Nelle clip il singolo handle di segmento e' un vero controllo 2D normalizzato
nel rettangolo formato dai due endpoint: non puo' uscire da quel rettangolo e
puo' quindi arrivare agli angoli che descrivono una curva ad angolo retto. La
stessa geometria governa paint, hit-test e drag. Lo split usa De Casteljau e
conserva esattamente entrambi i sottosegmenti.

Ogni sequenza conserva sempre il primo e l'ultimo punto. Con
`pinnedEndpoints` questi restano ancorati a X=0 e X=1; con `fadeEndpoints` sono
essi stessi i punti di fine fade-in e inizio fade-out e possono muoversi in X.
Gli anchor sinistro e destro al bordo sono soltanto estremi matematici, non
punti editabili aggiuntivi, e possono avere valori Y indipendenti (per esempio
`0.5` per un Pitch Bend neutro). Audio Gain e Image Alpha usano questo stesso
contratto: fade e automazione sono un'unica linea, con un unico stato e gli
stessi controlli di curvatura. Nelle linee continue entrambi gli endpoint sono
permanenti e pinnati; in particolare il primo resta sempre a X=0, all'inizio
della timeline.

## Build del solo nucleo, senza JUCE e senza rete

Servono CMake 3.22 o successivo e un compilatore C++20. Python 3 aggiunge il controllo di integrita' delle fixture generate.

```sh
cmake --preset core
cmake --build --preset core
ctest --preset core
```

Alternativa senza preset:

```sh
cmake -S . -B build/core -DAYRA_LAB_BUILD_GUI=OFF -DCMAKE_BUILD_TYPE=Debug
cmake --build build/core --config Debug
ctest --test-dir build/core -C Debug --output-on-failure
```

Release e sanitizer sono build separate:

```sh
cmake -S . -B build/release -DAYRA_LAB_BUILD_GUI=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build/release --config Release
ctest --test-dir build/release -C Release --output-on-failure

cmake -S . -B build/asan -DAYRA_LAB_BUILD_GUI=OFF -DAYRA_LAB_SANITIZE=ON \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=clang++
cmake --build build/asan
ctest --test-dir build/asan --output-on-failure
```

## Build dell'app JUCE

Usare un checkout **ufficiale e immutato** di JUCE che esponga le API JUCE 8 (`FontOptions` incluso). Il progetto dichiara JUCE 8.0.12 per il download opzionale; non e' una dichiarazione che sia la release piu' recente. Il checkout locale permette di usare la versione gia' adottata nel workspace, da registrare e verificare nel collaudo.

```sh
cmake -S . -B build/native \
  -DJUCE_SOURCE_DIR="/percorso/JUCE" \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build build/native --config Debug --target AyraTimelineLab
```

Su Windows, usare percorsi come `C:/dev/JUCE`; con Visual Studio il parametro `--config Debug` seleziona la configurazione. Su macOS si puo' aggiungere `-G Xcode` in una directory di build nuova. Occorrono SDK e compilatore della piattaforma. Su Linux occorrono anche i prerequisiti GUI indicati dalla documentazione JUCE della versione scelta.

L'artefatto e' sotto `build/native/AyraTimelineLab_artefacts/`, nella sottocartella della configurazione quando prevista dal generatore. Nessuna firma o notarizzazione e' predisposta; non alterare le credenziali o il signing del progetto principale.

Per caricare una sessione del mockup usare il bottone **Importa JSON** dell'app, oppure l'opzione `--session "/percorso/sessione.json"` all'avvio del binario. Il file e' validato prima di sostituire lo stato.

Solo con consenso esplicito al download si puo' evitare il checkout locale:

```sh
cmake -S . -B build/native-fetch -DAYRA_LAB_FETCH_JUCE=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build/native-fetch --config Debug --target AyraTimelineLab
```

Questa opzione richiede rete e Git. JUCE non e' incluso nel pacchetto; la relativa licenza resta applicabile. Nessun download avviene nel programma in esecuzione.

## Look and feel: due modalita', un'interfaccia

### Modalita' predefinita, tema isolato

`fixtures/theme.json` contiene uno **snapshot dichiarato** dei valori verificati in `ayra_multiverse@5db35b8`. Il generatore produce `generated/ThemeData.h` e `generated/theme.css`. Questo permette di compilare il Lab senza l'ecosistema Ayra e di cambiare rapidamente il design.

Il fallback e' `LabLookAndFeel`, non una copia dell'intero `MultiverseLookAndFeel`. Non rivendica le stesse ombre, metriche dei font o tutti i widget di produzione.

### Modalita' con sorgenti canonici locali

```sh
cmake -S . -B build/native-ayra \
  -DJUCE_SOURCE_DIR="/percorso/JUCE" \
  -DAYRA_SOURCE_DIR="/percorso/ayra_multiverse" \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build build/native-ayra --config Debug --target AyraTimelineLab
```

`app/CanonicalTheme.cpp` include **in sola lettura** i token colori, tipografia e il facade delle primitive del checkout. La palette viene letta tramite `ayra::tokens`, le superfici raised chiamano il vero `fillRaisedSurface`. Il bridge non include `ayra_common` e disabilita localmente soltanto le funzioni di identita' che richiedono il modello Multiverse.

Non viene importato il `MultiverseLookAndFeel` completo e non vengono copiati i suoi file. L'header usa ancora la composizione del Lab; importare i token **non significa** aver integrato tutti i controlli, AppWindowHeader, icone e layout del prodotto. Il bridge e' stato compilato contro il checkout locale scelto, ma non e' stato collaudato visivamente nell'app in esecuzione.

La sessione JSON passa colori di identita' espliciti. La futura integrazione deve iniettare il resolver cromatico canonico, comprese le corrette regole di fallback per Song Track/performer/Song Main.

Non sono distribuiti file di font. L'HTML preferisce Avenir Next e ripiega sui font di sistema; i controlli del Lab nativo usano il fallback JUCE di sistema. Le metriche tipografiche native vanno confrontate sulla piattaforma reale.

### Architettura dei widget clip

La UI delle clip vive in `app/clips/` e ha un solo ingresso aggregato,
`ayra_clips.h`. `ClipComponent` e' la radice visuale generica e le sei primitive
sono `audio`, `midiNote`, `lineClip`, `pointKeyframe`, `spanKeyframe` e
`continuousLine`. Questa gerarchia riguarda esclusivamente i widget: nel
documento reale `ayra::timeline::Clip` resta il container non polimorfico e il
tipo di contenuto continua a dipendere dalla `Source` della take attiva.

I componenti non possiedono file, Undo o stato musicale. Durante il paint
chiedono una proiezione visuale non autorevole al rispettivo provider; il Lab la
materializza dalla fixture, mentre Multiverse dovra' materializzarla dagli owner
e dalle cache canoniche. I canvas mantengono le gesture e riconciliano i child
per ID, quindi un refresh non distrugge il componente che visualizza lo stesso
elemento.

La clip MIDI ha tre LOD verticali canonici: preview a 96 px, editing note a
256 px e piano roll con Velocity Lane a 400 px. Il doppio click sul titolo cicla
i tre livelli. Nei livelli editabili una scrollbar del pitch compare solo in
hover, sul lato piu' vicino al mouse; il doppio click sulla scrollbar centra C3.
Le altezze sono volutamente maggiori dei primi valori proposti (72/132/220), che
avrebbero ridotto le righe a pochi pixel. Il pulsante **Test tutte le Clip** apre
un banco scrollabile con le canvas reali: Audio, MIDI Note, Line Clip,
Point Keyframe, Span Keyframe e Continuous Line. Audio Clip e Line Clip usano
per default la zona sotto la curva piu' scura; il controllo nell'harness permette
di passare in tempo reale alla zona sopra piu' scura, lettura originale di
AyraTimelineLab. Il gradiente Tempo di Multiverse resta esclusivamente nelle
Continuous Line, mostrate con stroke bianco, teal e ambra. Point e span adottano
il layout Counter/Marker di Multiverse. Non sono anteprime
statiche: hit-test, selezione, drag, doppio click, scrollbar MIDI e rail sono
quelli delle implementazioni effettive. Il wiring di prodotto resta comunque
responsabilita' dell'host: l'harness usa la fixture condivisa esclusivamente
come sorgente dimostrativa.

Il Clip LookAndFeel e' separato dal LookAndFeel comune: l'interfaccia non eredita
da `LookAndFeel_V4`, ha fallback stateless e riceve il widget per reference. La
ricetta di disegno e' quella gia' approvata nel Lab; colori, geometrie, hit-zone
e priorita' dei gesti non sono stati ridisegnati in questa estrazione.

## File e responsabilita'

```text
core/                 Modello fixture, capacita', ordine, selezione, Undo e validazione.
app/                  Shell JUCE, TrackHeader, Timeline/Mixer, Inspector e codec JSON.
app/clips/            Root clip UI, specializzazione audio, proiezioni e Clip LookAndFeel.
app/CanonicalTheme.cpp Bridge opzionale, sola lettura dei token originali.
fixtures/             Sorgenti editabili dei dati dimostrativi e della palette.
generated/            Dati derivati dal generatore; non editarli manualmente.
web/                  HTML autonomo e sorgenti CSS/JS/template leggibili.
tests/                Test del core reale e interazioni browser Playwright.
tools/                Generatori deterministici e controllo del pacchetto.
docs/                 Integrazione, contratti e checklist nativa.
verification/         Risultati eseguiti, manifest e screenshot del browser.
```

Il Lab ha due implementazioni di interazione, JavaScript e C++, che condividono fixture e schema. Non sono lo stesso binario: la parita' richiede test incrociati e collaudo nativo. Il core C++ testato e' quello linkato dall'app, non una ricopia dei suoi algoritmi nei test.

## Rigenerare e modificare

Modificare `fixtures/session.json` e `fixtures/theme.json`, poi:

```sh
python3 tools/generate.py
python3 tools/build_web.py
python3 tools/verify_sources.py
```

Per cambiare la composizione HTML modificare `web/style.css`, `web/app.js` e `web/index.template.html`, quindi rieseguire `build_web.py`. L'HTML assemblato e' incluso per l'apertura immediata, non e' l'unico sorgente disponibile.

Per ripetere i controlli browser, installare Playwright nell'ambiente di sviluppo e usare un Chromium disponibile:

```sh
python3 -m pip install playwright
python3 tests/browser_test.py --chromium /percorso/chromium
```

In alternativa, senza `--chromium`, il runner usa il Chromium di Playwright se installato. Il test non riproduce audio. Gli screenshot sono acquisizioni del browser, **non immagini generate e non screenshot della GUI JUCE**.

## Funzionalita' e limiti, senza equivalenze implicite

| Superficie | Inclusa | Limite |
| --- | --- | --- |
| HTML | 4 ruoli, confronto affiancato, Dark/Light, controlli fixture, selezione, resize, ordine libero, mini-mixer | Non esegue codice JUCE o DSP. |
| Clip HTML | Layout approvato, N waveform, slot vuoti, drag dimostrativo e fade, editor/mapping | Waveform sintetiche, non decoder reali; non e' il motore di editing Multiverse. |
| Core C++ | 4 ruoli, selezione, ordine/Undo, permutazione, validazione e proiezione fixture | Nessun documento Ayra; audibilita' dimostrativa, non contratto audio. |
| GUI C++ | Sorgenti completi della shell, header condiviso, resize/DnD tracce, controlli, JSON, inspector e mapping con frecce | Non compilata/eseguita qui. Nessuna promessa di parita' visuale pixel-perfect. |
| Clip C++ | Disegno N-channel, selezione, doppio click, move/trim/stretch/slip, warp, gain envelope, fade numerici e mapping condiviso | Usa waveform e stato audio dimostrativi; non decodifica media reali. Il mapping nativo si riordina con Su/Giu', non con DnD delle righe. |
| Contesto globale | Range, Tempo, Time Sig, Marker, Counter, Actions non riordinabili | Fixture informative; non editor completi. |
| Gruppi | Ordine strutturale, gruppo ripiegabile, MIX mantenuto | La fixture ha un solo performer; il flag di collapse e' globale ai gruppi. |
| Performance | Picchi/path sintetici preparati fuori dal paint, componenti riconciliati per ID | Non c'e' virtualizzazione completa, benchmark frame-time o prova zero-allocation della GUI. |

Il nucleo limita la sessione a 256 tracce e 64 clip per traccia; sono limiti difensivi del banco, **non una promessa di performance** e non limiti di prodotto. Per sessioni grandi bisogna collaudare layout e virtualizzazione prima della promozione nei moduli.

Le copie dell'HTML salvate dall'utente persistono sessione, tema e ordine, non Undo/Redo, dialog aperti, scroll e ogni preferenza transiente del browser. Il file JSON e' proprio del Lab, versione 1; non contiene migrazioni per documenti Ayra.

## Gate di accettazione

Leggere `docs/VERIFICHE_NATIVE.md` e `verification/STATUS.md`. Prima di promuovere codice nei moduli, compilare il target GUI, eseguire i gesti reali e acquisire screenshot nativi Light/Dark a scale differenti. Una build del solo core non certifica il renderer, il focus o il ciclo di vita dei componenti.

**Nessuna modifica a main, nessuna PR aggiuntiva e nessuna dipendenza di Multiverse su questo Lab.** Il pacchetto e' una base separata da revisionare, compilare e usare per il collaudo, non una nuova autorita' musicale.
