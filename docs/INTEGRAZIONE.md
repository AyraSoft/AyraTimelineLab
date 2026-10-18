# Dal banco alla Timeline reale

## Il banco non va collegato al DSP come se fosse il modello del prodotto

`core/LabModel` e' un host fixture per dimostrare interazioni e invarianti. Non deve essere copiato nella serializzazione Multiverse, sostituire `Track`, `Participation`, `ContentBus`, `SourceSlot` o aggiungere un gain/pan di traccia app-only.

La separazione da mantenere e':

```text
Documento/servizi canonici --> proiezione immutabile per la vista
                                             |
                                  widget condiviso + LookAndFeel
                                             |
                                    intenzione dell'utente
                                             |
                       controller canonico --> Undo --> documento
```

Il Lab usa `Store` come host di prova; in produzione un adapter sostituisce lo Store con gli owner gia' presenti. L'aspetto puo' cambiare senza modificare comandi, permutazioni e identita'.

La gerarchia `clips::ClipComponent` / `clips::AudioClipComponent` e' una
gerarchia di **widget**, non un nuovo model layer. Non va trasposta creando
`AudioClip` e `MidiClip` documentali: `ayra::timeline::Clip` resta il wrapper
canonico sul `ValueTree`, mentre la `Source` attiva determina il contenuto.
L'adapter Multiverse deve produrre `AudioClipVisual` dagli snapshot/cache
esistenti e tradurre le intenzioni del canvas in comandi del controller.

## Contratti che il banco rende verificabili

| Fatto | Owner nel Lab | Trasposizione nel prodotto |
| --- | --- | --- |
| Tipo/ruolo della riga | `Role`, `Track`, `capabilities()` | Proiezione app-specifica di ruolo, separata dal singolo stream. |
| Header | `TrackHeader` + `HeaderLayout` | Widget generico nel provider appropriato, con un solo layout per paint/hit-test. |
| Selezione | `Store::select()` | Selezione del Tensor condivisa con Inspector/Mixer, distinta da clip/eventi. |
| Ordine libero | `Session::manualOrder` | Metadata visuali stabili della Song; non ordine dei nodi del grafo per comodita'. |
| Visibilita' | `visibleOrder()` | Proiezione strutturale o personale, separata dal routing. |
| Canali audio | `Track::order` | Permutazione canonica P del bus; composizione R=P*E eseguita una sola volta dagli owner esistenti. |
| Colore | Colore esplicito fixture + `Palette` | Resolver unico dell'identita' del nodo/performer/Song e token correnti. |
| Comandi | `HeaderIntent` | Callback verso comandi/servizi gia' presenti, con UUID e revisione. |
| Undo | Storia bounded del Lab | Undo documentale esistente per i dati; stato di vista distinto dalle revisioni musicali. |

La fixture ha un ingresso audio con N posizioni e una lane visuale per pin MIDI/OSC/DMX. Non genera una nuova track documentale per ogni canale audio. Le lane extra non hanno un altro header a sinistra e i canali rimangono dentro il contorno dell'evento.

## Identita' dei quattro ruoli

Un MIX non e' un folder. Nel percorso attuale esistono `performerRef` per la sezione e l'UUID della participation/convergenza per l'entita' processante. L'adapter deve identificare entrambi esplicitamente, senza usare la posizione o il nome per risolvere i comandi.

Song Main non e' Project Main. Il Mute Program deve mantenere il significato gia' definito dal prodotto; non inventare un Solo per uniformare la toolbar. Le indicazioni di audibilita' ereditata devono venire dal resolver reale e distinguere gli output considerati.

Una Song Track e' song-owned; non acquisisce un performerRef perche' viene trascinata vicino alle tracce di un performer. Una Performer Track conserva appartenenza, colore ereditato e routing in modalita' libera.

## Punti d'integrazione da ricontrollare contro HEAD

Revisione letta per il tema e le note: `5db35b89ddb73da6da2c5b6d3184f5a3878bda1b`.

- `TimelinePanel::makeTrackRow()` e `rebuildSnapshot()`: pubblicare capacita' della riga e identita' controllata, non dedurle da un solo `outputStream`.
- `TimelineTrackHeaderWidget` nel provider `ayra_timeline`: aggiungere selezione e capability di interazione; discriminare mouse primario/popup; layout unico; annullamento dei gesti.
- `TimelineComponent`: applicare altezze effettive e riconciliare righe per UUID; mantenere lo scroll ancorato alla riga quando cambiano ordine o altezza.
- `MixerPanel`: sostituire la ricostruzione distruttiva per i soli cambi d'ordine con una riconciliazione per identita'. Non distruggere una strip che ospita una gesture o un editor.
- `MultiverseTimelineLookAndFeel`: usare ruolo/capacita' espliciti e token canonici. Non considerare ogni stream vuoto una corsia globale.
- `ClipContextEditor` e mapping del contenuto: riusare il percorso esistente `Channels / Mapping...`; non materializzare un ordine diverso su ogni clip.

Sono indicazioni d'integrazione, non modifiche gia' applicate dal pacchetto. La PR precedente sui gesti delle clip e il codice di questo Lab restano consegne separate.

## Cosa condividere, cosa no

Promuovere nel provider generico solo responsabilita' concrete: geometria dell'header, capability, input semantico, stato transiente cancellabile, accessibilita' e hook LookAndFeel. La mappa Song/performer/MIX, il routing e la risoluzione dei parametri sono responsabilita' del prodotto/owner esistente.

Il Lab deve poi linkare il widget canonico, non tenere una sua copia. Non incorporare il motore, non copiare `MultiverseLookAndFeel` insieme a tutte le sue dipendenze per ottenere un colore, non modificare JUCE. Il bridge read-only ai token e' intenzionalmente piccolo.

HTML e C++ non condividono oggi lo stesso motore di input. Le fixture generate sono comuni, ma la parita' delle azioni va mantenuta con test, export/import incrociato e collaudo nativo. Non affermare che il JavaScript sia gia' tradotto esattamente in ogni dettaglio del widget.

## Decisioni successive che restano esplicite

La prima iterazione e' un banco per header, non l'intera app. Prima di estendere il perimetro stabilire:

1. Inventario di entita' e namespace degli UUID per sezioni puramente visuali e Tensor reali.
2. Collasso per singolo gruppo e politiche di filtro su una sessione con piu' performer.
3. Persistenza locale di densita'/larghezza contro metadata visuali condivisi nella Song.
4. Capability reali dei controlli: parametro non assegnato, valore misto, permesso negato o nodo non disponibile.
5. Budget di layout, virtualizzazione e paint con dataset grandi, dopo una misura nativa.

La riunificazione con l'app deve rimuovere i percorsi superati nello stesso cambiamento, non lasciare vecchio e nuovo widget entrambi autorevoli.
