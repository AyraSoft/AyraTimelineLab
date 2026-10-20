# Identità flottante e sorgenti visuali

## Il problema corretto

Il primo nome occupava una zona di 240 px a tutta altezza, con un gradiente tinto
che arrivava al 97% di opacità. Un'unica stringa a 14 px mescolava traccia e lane.
Il fondale risultava più evidente del nome, soprattutto sopra waveform o note
sature, e sembrava una colonna laterale. Essere un overlay non rendeva questa
composizione automaticamente buona o coerente con Multiverse.

Ora il titolo è l'owner, il sottotitolo la lane. Il titolo usa il ruolo tipografico
Ayra a 20 px, il dettaglio caption a 11.5 px; le corsie globali restano su una sola
riga compatta. Dove Avenir Next non è installato, il fallback dichiara un peso
Bold reale anziché lasciare che Demi Bold ricada silenziosamente sul Regular.
Nessun font viene distribuito dalla sandbox.

Il fondale misura il testo: un plateau neutro dietro le lettere, una sfumatura
laterale morbida e dissolvenza verticale. Non cresce con tutta l'altezza della
lane. Un filo del colore owner e un piccolo indicatore del tipo distinguono le
due identità senza colorare il testo principale. Nomi lunghi usano ellissi,
mai compressione orizzontale del font. Nessuno spazio viene sottratto al canvas.

## Confine del nuovo painter

`modules/ayra_timeline_panel/TimelineIdentity.h` calcola la sola geometria.
`TimelinePanelLookAndFeel::prepareIdentity/drawIdentity` sono i punti di delega.
`demo/TimelineIdentityArtwork.h` è la ricetta dell'host di prova: legge i token
Ayra selezionati. È una proposta visuale del Lab, NON un nuovo widget già presente
nel prodotto. Nel port, questa ricetta può entrare nel relativo LAF del pannello.

Il backdrop è preparato soltanto quando cambiano nome, layout, stile o stato
visuale. Lo possiede la riga residente; il normale scroll orizzontale non lo
rigenera. Nessun blur per-frame, animazione continua o cache globale viene aggiunto.
Il testo è disegnato vettorialmente alla scala corrente.

Click, drag e wheel sui rettangoli dei nomi vengono inoltrati al canvas. Soltanto
il doppio click viene intercettato. Il titolo rinomina l'owner condiviso da tutte
le lane; il sottotitolo rinomina solo quella lane. La rinomina di una corsia globale
indirizza la lane. I comandi restano soggetti a session/revision guard e Undo host.

## La risposta precisa alla domanda sul riuso

Prima veniva compilato `ayra_ui`, un sottoinsieme locale di primitive Ayra. Le
classi non erano tutte inventate, ma uno snapshot non segue gli aggiornamenti del
checkout Multiverse. Inoltre il Lab aveva painter propri per i bottoni JUCE e per
il nome. La variabile `AYRA_SOURCE_DIR` non instradava realmente questi renderer.
Non era corretto chiamare questa configurazione il LookAndFeel completo del prodotto.

### Profilo sorgenti originali (default richiesto dall'app nativa)

Passare `AYRA_SOURCE_DIR` al checkout di `AyraSoft/ayra_multiverse`.
`cmake/VisualSource.cmake` verifica e include in place i file selezionati:

| Elemento | Fonte effettiva |
| --- | --- |
| Colori, tipografia, raggi | `modules/look_and_feel/tokens/ayra_Tokens*.h` |
| Glyph e geometrie bottoni nodo | `modules/look_and_feel/glyphs/*`, `ayra_NodeButtons.h` |
| Superfici recessed/raised e shadow cache | `ayra_ChromePrimitives.h` |
| Meter/cap/fader painter comuni | `ayra_CommonDefaultLookAndFeel_Mixer.cpp` |
| Pulsanti a icona | `ShapeIconButton` e `AddButton` originali |
| Tab e campi trascinabili | `TabStrip` e `DragValueField` originali |
| Bottoni, toggle, tickbox e tab JUCE | Corpi originali di `ApplicationLookAndFeel_Buttons.cpp` |
| Nome flottante | Nuova ricetta host-side che usa quei token |
| Menu e restanti superfici del Lab | Adattatori/compatibilità, NON intero ApplicationLookAndFeel |

I sette metodi standard del file Buttons originale entrano in
`SharedChromeLookAndFeel`, la base del LAF applicativo del Lab. Lo script
`tools/adapt_multiverse_buttons.py` cambia soltanto l'include della classe e il
qualificatore dei metodi. I corpi di paint e gli helper restano invariati.
L'output vive sotto il BUILD, mai nei sorgenti o nella repo pubblica. Un inventario
metodi diverso viene rifiutato, non sostituito automaticamente da una copia vecchia.

Questo NON importa tutto `ayra_common`, `ayra_gui`, Plugin Manager o il runtime.
Il vero `ApplicationLookAndFeel` ha interfacce e dipendenze ulteriori. Il vantaggio
qui è consumare davvero le ricette visuali necessarie senza avviare motore/audio,
scanner o servizi non pertinenti al banco. Nel prodotto, il normale LAF applicativo
rimane il proprietario di questi metodi.

### Profilo snapshot (opt-out esplicito)

Senza checkout si può passare `AYRA_LAB_REQUIRE_MULTIVERSE_VISUALS=OFF`. Il banner
nella UI indica `ayra_ui: snapshot locale / non Multiverse live`. Non si presenta
un confronto di quello snapshot come prova del renderer originale.

Il profilo originale è indicato come `Multiverse: sorgenti visuali diretti /
adattatore Lab`. La build genera `visual-source-manifest.txt` con origine,
percorsi relativi e SHA-256. Il cambio di file selezionati aggiorna il manifesto.
Un checkout incompleto o un contratto incompatibile è un errore esplicito.

## Verifica e trasferimento

Reference ispezionata tramite connettore: Multiverse
`459d8c5cfc4e06bd849db12799e447ec90be3feb`.
Non sono state scritte modifiche al prodotto, né pubblicati suoi sorgenti privati.
I risultati correnti sono in `verification/identity/STATUS.md`; il precedente
report della demo completa è marcato storico. Le immagini correnti sono fixture
native JUCE del painter, non screenshot dell'intera applicazione o audio decodificato.

L'approvazione estetica resta distinta dalla riuscita dei test. Restano fuori
perimetro in questa revisione: persistenza della demo, Glue PCM, runtime monitoring,
parità dei menu completi di Multiverse e collaudo macOS/Windows.
