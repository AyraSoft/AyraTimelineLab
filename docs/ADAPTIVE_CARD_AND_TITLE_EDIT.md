# Card adattiva, nome protetto ed editing inline

Revisione successiva a `8eceba72`. Sostituisce le prescrizioni precedenti che
rendevano anche il nome sempre passivo; materiale, blur e colori restano invariati.

## Altezza dal contenuto, non dalla card

Le lane `point` e `span` della demo sono `HeightPolicy::automatic`. Il canvas
pubblica la propria misura con `preferredPanelHeight` e una callback di invalidazione;
il pannello riceve `LaneCanvas::preferredHeight` / `setContentChangeListener`.
Nessuna scrittura al documento musicale, nessun timer per lane. L'host scollega la
callback prima di distruggere un canvas. Il resize manuale delle curve continue
resta attivo e non e' esteso alle corsie evento.

`ayra_KeyframeRowLayout.h` e' il packer effettivamente usato dal canvas: intervalli
visivi proiettati nella finestra corrente, sort stabile e code di priorita',
O(N log N). Gli span adiacenti sono half-open e restano sulla stessa banda.
Nessun limite artificiale di sei bande sovrapposte. I dati degli eventi non cambiano.
Le misure orizzontali includono le etichette dei punti, senza dipendere dall'altezza
corrente della lane o dalla geometria della card.

| Contenuto | Altezza naturale |
| --- | --- |
| Una banda di punti | 24 px: 3 + pill 18 + 3 |
| Due bande di punti | 46 px; ogni banda successiva aggiunge 22 px |
| Una banda span | 32 px, inclusi gli 8 px necessari alla barra di taglio/hit |
| Due bande span | 62 px; ogni banda successiva aggiunge 30 px |

Queste sono misure logiche. Restano validi limiti/capability dell'host e il floor
richiesto da eventuali controlli laterali riattivati esplicitamente. Durante il drag
si mantengono le bande visuali catturate; il rilascio ricalcola misura e packing.
Le variazioni di altezza sono coalescenti e mantengono l'ancora dello scroll verticale.
Una misura identica non genera un nuovo bind o un ciclo di layout.

## Una o due righe automaticamente

Sotto 40 px la card usa una sola riga da 22 px, con la tipografia del tipo. A partire
da 40 px usa le due righe del materiale approvato. Le corsie globali di servizio
rimangono a una riga: non aggiungiamo un Performer fittizio o il titolo `Globals`.
La card e' sempre centrata verticalmente e non contribuisce alla misura della lane.
Il limite normale e' ancora 229 px; testi lunghi hanno ellissi, non font schiacciati.

M gialla e S rossa seguono il nome della traccia, PRIMA della colonna delle frecce.
Le sette indicazioni di stato attive stanno sotto le frecce: 6x6 px, due righe
massime da quattro colonne nella card alta; una sola fila (massimo 54 px) nella
card compatta. In quest'ultima le frecce usano bersagli 16x12 quando devono convivere
con gli stati, 16x18 senza stati; non si aumenta la lane per ospitarle. Non ci sono
slot vuoti per gli stati spenti, ne' sovrapposizioni con nome/M/S.

## Nome della traccia: protezione ed espansione

Solo l'area effettivamente disegnata del nome della traccia e la coppia di frecce
proteggono la card dalla scomparsa. Performer, tipo, pallino, M/S e stati di processing
rimangono passivi. L'attesa content-aware di 500 ms resta centralizzata nel pannello.
Il movimento fra aree passive non fa ricominciare il conteggio ad ogni pixel.

Sopra un nome troncato la card puo' allargarsi verso l'interno della vista fino a
mostrare il testo, entro i limiti disponibili. Nessuna variazione di altezza,
origine temporale o rettangolo del canvas. Non viene cambiato il materiale approvato.
Per nomi ancora piu' lunghi del viewport, il tooltip conserva il testo completo e
l'editor ha il proprio scorrimento orizzontale. Una card a sinistra conserva il
bersaglio originale del nome durante l'espansione: il secondo click non puo'
accidentalmente finire nella clip appena scoperta. Non viene protetto il rettangolo
vuoto fra vecchio e nuovo bersaglio.

Doppio click sul titolo apre un solo TextEditor lazy della riga residente.
Enter conferma, Escape annulla, perdita del focus conferma una volta. Il testo
vuoto viene rifiutato. Le rinomine indirizzano l'owner (Track/Tensor, convergence,
Song Main), non la lane ne' il Performer. Le corsie globali di servizio non diventano
editor del loro owner aggregato. I callback esistenti di renameLane restano API
separate, senza riutilizzarle per il nome di una traccia.

Il commit e' differito fuori dalla callback del TextEditor e verifica sessione,
owner/lane, revisione e nome originale. Un cambio di stato non correlato puo'
convivere con l'edit; una rinomina concorrente non viene sovrascritta. Un cambio
sessione/owner o eliminazione annulla il tentativo. La riga con editor attivo
conserva la propria lease di residenza anche se esce dal viewport. L'host ammette
la transazione e possiede Undo: la UI non introduce una seconda autorita'.

## File e confini

- `TimelineIdentity.h`: layout, titleHit, stato transitorio di espansione.
- `TimelinePanel.h`: editor, input, lease, callback di misura/scroll anchor.
- `ayra_KeyframeRowLayout.h`: misura/packing senza JUCE.
- `ayra_KeyframeLaneCanvas.cpp`: geometria vera del canvas ed invalidazione.
- `demo/PanelFixture.h`: policy automatiche per le lane di eventi della demo.

I sei pulsanti laterali e i meter restano API opt-in, nascosti per default.
Nessun provider, sorgente privato o font e' copiato nella repository.
I risultati eseguiti sono in `verification/adaptive-title/STATUS.md`.
