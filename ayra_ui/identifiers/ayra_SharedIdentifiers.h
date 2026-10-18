/*
  ==============================================================================

   This file is part of the Ayra ecosystem.
   Copyright (c) Ayra Productions.

   ayra_common / shared / identifiers / SharedIdentifiers
   ------------------------------------------------------
   Identificatori `juce::Identifier` del Document Model CONDIVISO tra Ayra Tensor
   (Prodotto A) e Ayra Multiverse (Prodotto B).

   ## Perche' esiste
   Il documento di un'istanza Tensor e' UN `juce::ValueTree` (vedi CLAUDE.md
   "Document Model"). Multiverse lo wrappa e aggiunge i propri nodi (Song,
   Performer, Setlist). Perche' i due prodotti leggano/scrivano lo STESSO albero
   senza divergere, gli `Identifier` devono vivere in un posto solo: qui.

   ## Convenzione di naming dei nodi/proprieta'
   - Tipi di nodo (tag XML) in UPPER_CASE: `GRAPH`, `NODE`, `CONNECTION`.
   - Proprieta'/attributi in camelCase: `posX`, `editorOpened`, `uuid`.
   - Namespace: `ayra::ids` (minuscolo: distinto da eventuali `ayra::IDs` legacy).

   ## Linkage
   `inline const juce::Identifier` (C++17 inline variable): UNA sola definizione
   anche se l'header viene incluso in piu' translation unit (Tensor.cpp, harness
   di test, Multiverse.cpp). Niente duplicazione, niente ODR issue. La string
   pool globale di JUCE garantisce comunque uguaglianza per puntatore.

  ==============================================================================
*/

#pragma once

namespace ayra
{

//==============================================================================
// MARK: - UUID nullo canonico (anti-footgun)
//==============================================================================
/** Ritorna un `juce::Uuid` GENUINAMENTE NULLO (`isNull() == true`).

    ## Perche' esiste (FOOTGUN documentato)
    `juce::Uuid` di default-construisce un uuid **CASUALE**, non nullo:
    @code
      juce::Uuid{}.isNull()                    // == FALSE (e' casuale!)
      juce::Uuid().isNull()                    // == FALSE
      return {};   // in una funzione che ritorna juce::Uuid -> CASUALE, non null!
    @endcode
    Una funzione che usa `return {};` / `return juce::Uuid();` come sentinella di
    "non trovato / fallimento" inganna SILENZIOSAMENTE ogni chiamante che fa
    `if (result.isNull())`: la guardia non scatta mai e si prosegue con un uuid
    spazzatura. Bug storico: `DefaultNodeProfile::findExistingDefaultNodeUuid`
    ritornava `{}` -> `applyTo` prendeva SEMPRE il ramo "reuse" e non creava MAI
    i nodi di default (channel-strip vuote).

    ## La forma corretta
    Solo il costruttore da stringa vuota (o `juce::Uuid::null()`) produce un uuid
    nullo: `juce::Uuid (juce::String()).isNull() == true`. Questo helper centralizza
    l'idioma in UN posto greppabile cosi' il footgun non puo' ripresentarsi: ogni
    funzione che ritorna `juce::Uuid` come "null/not-found/failure" deve usare
    `ayra::nullUuid()`, MAI `{}` ne' `juce::Uuid()`.

    @see DefaultNodeProfile::findExistingDefaultNodeUuid, GraphProcessor::getUuidForNode */
[[nodiscard]] inline juce::Uuid nullUuid() noexcept { return juce::Uuid (juce::String()); }

} // namespace ayra

namespace ayra::ids
{

#ifndef AYRA_DECLARE_ID
 /** Dichiara un `juce::Identifier` inline (singola definizione cross-TU). */
 #define AYRA_DECLARE_ID(name) inline const juce::Identifier name { #name };
#endif

//==============================================================================
// MARK: - Gerarchia documento (tipi di nodo, UPPER_CASE)
//==============================================================================

AYRA_DECLARE_ID (PROJECT)          // radice progetto Multiverse
AYRA_DECLARE_ID (CONCERT)          // contenitore live (Multiverse)
AYRA_DECLARE_ID (PERFORMER)        // un performer (Multiverse) = un Tensor esterno
AYRA_DECLARE_ID (SONG)             // una song (Multiverse)
AYRA_DECLARE_ID (SONG_MAIN)        // graph finale specifico della song
AYRA_DECLARE_ID (PROJECT_MAIN)     // graph finale globale del progetto
AYRA_DECLARE_ID (INTERLUDES)       // master project-owned dei Tensor Interlude
AYRA_DECLARE_ID (SETLIST)          // ordine delle song (Multiverse)
AYRA_DECLARE_ID (TRACK)            // una traccia = un Tensor (foglia)
AYRA_DECLARE_ID (GRAPH)            // radice del documento di un grafo Tensor
AYRA_DECLARE_ID (NODE)             // un nodo del grafo (plugin o sub-Tensor)
AYRA_DECLARE_ID (SUBGRAPH)         // nodo-sottografo: porta inline un GRAPH annidato
AYRA_DECLARE_ID (CONNECTION)       // una connessione tra due pin
AYRA_DECLARE_ID (CONNECTIONS)      // contenitore connessioni
AYRA_DECLARE_ID (PLUGIN)           // descrizione plugin (opaca) del nodo
AYRA_DECLARE_ID (STATE)            // blob di stato (base64) del nodo
AYRA_DECLARE_ID (HOST_STATE)       // stato host AGGIUNTIVO di un nodo sub-Tensor (oltre al <GRAPH>): APVTS/macro a livello-Tensor. Opaco al documento
AYRA_DECLARE_ID (descriptionXml)   // XML opaco della juce::PluginDescription (dentro <PLUGIN>)
AYRA_DECLARE_ID (format)           // nome formato plugin: "Internal"/"VST3"/"AU"
AYRA_DECLARE_ID (SNAPSHOT)         // uno snapshot di parametri
AYRA_DECLARE_ID (SNAPSHOTS)        // contenitore snapshot
AYRA_DECLARE_ID (SCENE)            // una scena (riga della matrice snapshot)
AYRA_DECLARE_ID (ACTION)           // azione/evento generico

//==============================================================================
// MARK: - Multiverse Document Model (Project > Setlist > Song > Performer > Track)
//  Nodi e proprieta' del layer orchestratore Multiverse (Prodotto B). Tensor
//  (Prodotto A) NON li istanzia: sono additivi e inerti nel suo binario.
//  Schema gerarchico corrente in _doc/ARCHITECTURE.md.
//  Performer = roster GLOBALE al Project (UUID = identita stabile); la
//  partecipazione a una Song e' la presenza di un <NODE type="performer"
//  performerRef=uuid> nel <GRAPH> master della Song. Import = copia lossless del
//  ValueTree + manifest <REQUIRED_PERFORMERS>; riferimenti non risolti =
//  dormant-preserved (dottrina orfani, ARCHITECTURE.md sez. 9).
//==============================================================================

AYRA_DECLARE_ID (PERFORMERS)          // contenitore roster globale dei performer
AYRA_DECLARE_ID (AUDIO_SLOTS)         // layout/mapping audio nominali del Performer
AYRA_DECLARE_ID (AUDIO_SLOT)          // singolo slot Input/Main/Cue referenziabile
AYRA_DECLARE_ID (layoutRevision)      // revisione positiva del solo layout nominale
AYRA_DECLARE_ID (audioRoutingRevision) // revisione positiva del dominio routing audio Performer
AYRA_DECLARE_ID (DEVICE_BINDINGS)     // contenitore binding I/O di un'entita' del documento
AYRA_DECLARE_ID (DEVICE_BINDING)      // binding route/stream/verso -> endpoint
AYRA_DECLARE_ID (SONGS)               // contenitore delle song del progetto
AYRA_DECLARE_ID (SONG_ENTRY)          // voce di setlist: riferimento song + stato lifecycle
AYRA_DECLARE_ID (SONG_BUNDLE)         // wrapper d'export: <SONG> + <REQUIRED_PERFORMERS>
AYRA_DECLARE_ID (REQUIRED_PERFORMERS) // manifest import: copie delle entry performer referenziate
AYRA_DECLARE_ID (TRANSPORT)           // dati tempo/timesig della song (TempoMap reale: M1)
AYRA_DECLARE_ID (MARKERS)             // contenitore marker della song
AYRA_DECLARE_ID (MARKER)              // un marker (anche sceneCue)
AYRA_DECLARE_ID (COUNTER_EVENTS)      // contenitore eventi della traccia Counter (count-in "1,2,3,GO")
AYRA_DECLARE_ID (COUNTER_EVENT)       // un evento counter: beat + valore (0=GO, 1..32 = numero)
AYRA_DECLARE_ID (counterValue)        // int: 0 = "GO", 1..32 = numero pronunciato all'evento counter
AYRA_DECLARE_ID (counterVoice)        // int: 0 = female (Achernar), 1 = male (Puck)
AYRA_DECLARE_ID (SCENE_CUES)          // contenitore scene cue (T3) della song
AYRA_DECLARE_ID (SCENE_CUE)           // un cue: marker -> (performer, scena)
AYRA_DECLARE_ID (performerRef)        // riferimento stabile a un <PERFORMER> del roster (uuid)
AYRA_DECLARE_ID (songRef)             // riferimento stabile a una <SONG> (uuid)
AYRA_DECLARE_ID (markerRef)           // riferimento a un <MARKER> (uuid)
AYRA_DECLARE_ID (cachedName)          // nome cache del referente (display orfani)
AYRA_DECLARE_ID (deviceId)            // id device audio del performer (roster); rimappato via EndpointRemapper
AYRA_DECLARE_ID (routeId)             // route logica generica del binding (Multiverse: uuid performer)
AYRA_DECLARE_ID (streamKind)           // "audio" / "midi" / "dmx" / "osc" / "image"
AYRA_DECLARE_ID (direction)            // "input" / "output"
AYRA_DECLARE_ID (deviceType)           // backend/protocollo dell'endpoint
AYRA_DECLARE_ID (endpointId)           // identificatore persistente dell'endpoint
AYRA_DECLARE_ID (channelMask)          // BigInteger in esadecimale; solo audio
AYRA_DECLARE_ID (reconnectAutomatically) // policy hotplug del binding
AYRA_DECLARE_ID (displayName)          // etichetta endpoint scoped alla route
AYRA_DECLARE_ID (settingsJson)         // configurazione protocol-specific JSON
AYRA_DECLARE_ID (DEVICE_CHANNEL)       // etichetta di un canale fisico del binding
AYRA_DECLARE_ID (channelIndex)         // indice fisico 0-based dentro DEVICE_CHANNEL
AYRA_DECLARE_ID (deviceBindingRef)        // connessione audio derivata dal binding (uuid)
AYRA_DECLARE_ID (mainPerformer)       // true sul <PERFORMER> base "Main": auto-creato, non eliminabile,
                                      // primo del roster e normale owner di participation/Track-Tensor.
                                      // Non coincide con Song Main o Project Main Output.
AYRA_DECLARE_ID (lifecycleState)      // 0 disabled / 1 preloaded / 2 active
AYRA_DECLARE_ID (bpm)                 // BPM: <TRANSPORT bpm> = mirror del cambio a beat 0; <TEMPO_CHANGE bpm>
AYRA_DECLARE_ID (timeSigNumerator)    // <TRANSPORT> mirror + <TIMESIG_CHANGE numeratore>
AYRA_DECLARE_ID (timeSigDenominator)  // idem denominatore
AYRA_DECLARE_ID (markerKind)          // "marker" / "sceneCue" / ...
AYRA_DECLARE_ID (sceneRow)            // riga della matrice snapshot da lanciare (scene)

// TempoMap reale (M1): liste di cambi tempo/metrica + posizione musicale in beat.
// Modello Document (VT children con uuid stabile -> undo/serializzazione JUCE-native).
// Un cambio a `beat==0` esiste sempre (iniziale, non rimovibile) e specchia il flat
// <TRANSPORT bpm/timeSig...>. `beat` (double) e' l'unita' di posizione condivisa da
// tempo-change, timesig-change E marker.
AYRA_DECLARE_ID (TEMPO_CHANGES)       // contenitore dei <TEMPO_CHANGE> (figlio di <TRANSPORT>)
AYRA_DECLARE_ID (TEMPO_CHANGE)        // un cambio di BPM: <TEMPO_CHANGE uuid beat bpm/>
AYRA_DECLARE_ID (TIMESIG_CHANGES)     // contenitore dei <TIMESIG_CHANGE>
AYRA_DECLARE_ID (TIMESIG_CHANGE)      // un cambio di metrica: <TIMESIG_CHANGE uuid beat timeSigNumerator timeSigDenominator/>
AYRA_DECLARE_ID (beat)                // posizione musicale in beat (double) di change/marker
AYRA_DECLARE_ID (curveType)           // <TEMPO_CHANGE curveType> forma della rampa VERSO il prossimo cambio
                                      // (int = ayra::TempoCurveType: 0 Step / 1 Linear / 2 Bezier / 3 Exp / 4 Log)
AYRA_DECLARE_ID (tension)             // <TEMPO_CHANGE tension> [0..1] curvatura per Bezier/Exp/Log (default 0)
AYRA_DECLARE_ID (controlX)            // <TEMPO_CHANGE controlX> [0..1] punto di controllo 2D X (handle unico); assente = forma parametrica
AYRA_DECLARE_ID (controlY)            // <TEMPO_CHANGE controlY> [0..1] punto di controllo 2D Y; assente = forma parametrica

// Content-source model (Sessione 04). Ogni Tensor possiede una TIMELINE inline.
// Tipi e proprieta' del sotto-documento appartengono esclusivamente a
// `ayra::timeline::id`; qui restano soltanto le estensioni Multiverse.
AYRA_DECLARE_ID (SOURCE_SLOTS)        // slot live/destinazione di un Tensor
AYRA_DECLARE_ID (SOURCE_SLOT)         // uno slot tipizzato e identificato da uuid
AYRA_DECLARE_ID (SOURCE_REGIONS)      // regioni semantiche possedute da Tensor/Performer
AYRA_DECLARE_ID (SOURCE_REGION)       // range+mode+TargetSet, mai Track/Lane
AYRA_DECLARE_ID (TARGET_SET)          // insieme ordinato di riferimenti SourceSlot
AYRA_DECLARE_ID (SOURCE_SLOT_REF)     // riga di TargetSet
AYRA_DECLARE_ID (outputPortRef)       // ContentBus indipendente -> porta graph di uscita
AYRA_DECLARE_ID (targetSourceSlotRef) // ContentBus replacement/overlay -> SourceSlot
AYRA_DECLARE_ID (playbackRole)        // "replacement" / "overlay" / "playbackOnly"
AYRA_DECLARE_ID (liveSourcePortRef)   // SourceSlot -> porta live opzionale
AYRA_DECLARE_ID (destinationGraphPortRef) // SourceSlot -> porta graph richiesta
AYRA_DECLARE_ID (takeoverPolicy)      // policy live/playback persistita sullo slot
AYRA_DECLARE_ID (sourceRegionMode)    // risoluzione semantica del range
AYRA_DECLARE_ID (sourceSlotRef)       // riferimento stabile in <SOURCE_SLOT_REF>

// Timeline Actions: controllo di flusso del transport POSIZIONALE (stop, stop&play,
// loop-ritornello, range di esclusione). Lista PIATTA di <ACTION> (tag generico gia'
// esistente) figli di <TIMELINE_ACTIONS> (figlio di <SONG>). Il NESTING dei loop e'
// inferito dal motore per contenimento di range (beat..beatEnd), non dalla struttura
// dell'albero -> editing semplice. Il motore campione-esatto vive in TransportPlayHead.
AYRA_DECLARE_ID (TIMELINE_ACTIONS)    // contenitore dei <ACTION> di flow-control
AYRA_DECLARE_ID (actionKind)          // "stop" / "stopPlay" / "loop" / "exclude"
AYRA_DECLARE_ID (beatEnd)             // fine del range (loop/exclude); ignorato per stop/stopPlay
AYRA_DECLARE_ID (repeatCount)         // loop: esecuzioni totali 1..32 (0 = infinito)
AYRA_DECLARE_ID (exitBeat)            // loop: uscita anticipata della percorrenza finale (beat); -1 = nessuna
AYRA_DECLARE_ID (actionCountInBars)   // stopPlay: battute di count-in stazionario al resume (0 = off)
AYRA_DECLARE_ID (loopCounterEnabled)  // loop: scandisce automaticamente l'ultima battuta di uscita
AYRA_DECLARE_ID (actionEnabled)       // loop/exclude abilitato (bool); permette il disable senza cancellare

// Playback range (FASE 1): nodo singleton <PLAYBACK_RANGE> figlio di <SONG> = range
// di esecuzione della song con fade-in/fade-out. Stato di documento (undoable,
// serializzato, NON APVTS), come le Timeline Actions. Le due curve di fade mirrorano
// il modo in cui un <TEMPO_CHANGE> memorizza la sua forma (curveType/tension + punto
// di controllo 2D opzionale), qui su set di attributi distinti per fade-in / fade-out
// (due curve sullo STESSO nodo non possono condividere lo stesso nome-attributo).
// Invarianti: startBeat <= fadeInEndBeat < fadeOutStartBeat <= endBeat.
// fadeInEndBeat==startBeat => nessun fade-in; fadeOutStartBeat==endBeat => nessun fade-out.
AYRA_DECLARE_ID (PLAYBACK_RANGE)      // nodo singleton figlio di <SONG>: range di esecuzione + fade
AYRA_DECLARE_ID (startEnabled)        // legacy load-only: start e' ora obbligatorio
AYRA_DECLARE_ID (endEnabled)          // legacy load-only: end e' ora obbligatorio
AYRA_DECLARE_ID (startBeat)           // double: inizio del range (beat)
AYRA_DECLARE_ID (fadeInEndBeat)       // double: fine della rampa di fade-in (== startBeat => nessun fade-in)
AYRA_DECLARE_ID (endBeat)             // double: fine del range = stop duro (beat)
AYRA_DECLARE_ID (fadeOutStartBeat)    // double: inizio della rampa di fade-out (== endBeat => nessun fade-out)
AYRA_DECLARE_ID (fadeInCurveType)     // String: preset di forma del fade-in ("linear"/"bezier"/"exponential"/...)
AYRA_DECLARE_ID (fadeInTension)       // double [0..1]: curvatura del preset di fade-in
AYRA_DECLARE_ID (fadeInControlX)      // double [0..1]: handle 2D X del fade-in; < 0 => usa curveType+tension
AYRA_DECLARE_ID (fadeInControlY)      // double [0..1]: handle 2D Y del fade-in; < 0 => usa curveType+tension
AYRA_DECLARE_ID (fadeOutCurveType)    // String: preset di forma del fade-out
AYRA_DECLARE_ID (fadeOutTension)      // double [0..1]: curvatura del preset di fade-out
AYRA_DECLARE_ID (fadeOutControlX)     // double [0..1]: handle 2D X del fade-out; < 0 => usa curveType+tension
AYRA_DECLARE_ID (fadeOutControlY)     // double [0..1]: handle 2D Y del fade-out; < 0 => usa curveType+tension

// Multiverse — Setlist unica del Project + gruppi medley.
AYRA_DECLARE_ID (MEDLEY)             // gruppo ordinato di <SONG_ENTRY> dentro la <SETLIST>
// Medley overlap (FASE 3): overlap beat-based fra due song consecutive di un medley.
// Proprieta' della <SONG_ENTRY> di PARTENZA (il passaggio S->S+1), come la vecchia
// transition. La testa (startBeat) di S+1 si allinea a [endBeat(S) - medleyOverlapBeats].
// 0 = butt-join (sequenziale). I fade restano della song (Fase 1/2): l'overlap governa
// solo l'ALLINEAMENTO temporale; il crossfade emerge se i fade delle due song si sovrappongono.
AYRA_DECLARE_ID (medleyOverlapBeats) // double >= 0 su <SONG_ENTRY>: overlap in beat verso la song successiva del medley

//==============================================================================
// MARK: - Parametri & bus
//==============================================================================

AYRA_DECLARE_ID (PARAMETER)
AYRA_DECLARE_ID (PARAMETERS)
AYRA_DECLARE_ID (paramId)
AYRA_DECLARE_ID (paramIndex)
AYRA_DECLARE_ID (INPUTS)
AYRA_DECLARE_ID (OUTPUTS)
AYRA_DECLARE_ID (BUS)
AYRA_DECLARE_ID (LAYOUT)           // tag layout bus
AYRA_DECLARE_ID (layout)           // valore layout (stringa)
AYRA_DECLARE_ID (index)

//==============================================================================
// MARK: - Bus descriptor audio dei nodi di confine (cue routing, M6)
//==============================================================================
// <AUDIO_BUSES> e' figlio del <NODE> di confine (performer nel master graph,
// Track associato nel master graph). Contiene <AUDIO_BUS_IN> e <AUDIO_BUS_OUT> con
// Ogni bus porta identita', ruolo/target e riferimento opzionale al layout
// Project; i child ordinati AUDIO_BUS_CHANNEL portano l'identita' dei canali.
// Gli indici flat sono una proiezione runtime e non vengono persistiti.
// Vedi _doc/multiverse_build/MIX_ROUTING_TIMELINE_V1.md sez. 3-4.
// NB: gli ID vestigiali INPUTS/OUTPUTS/BUS non vengono riusati.

AYRA_DECLARE_ID (AUDIO_BUSES)
AYRA_DECLARE_ID (AUDIO_BUS_IN)
AYRA_DECLARE_ID (AUDIO_BUS_OUT)
AYRA_DECLARE_ID (AUDIO_BUS_CHANNEL)
AYRA_DECLARE_ID (busId)
AYRA_DECLARE_ID (busRole)          // ruolo semantico del bus (vedi ayra::busRoles)
AYRA_DECLARE_ID (busTarget)        // uuid del destinatario (send) / sorgente (return)
AYRA_DECLARE_ID (layoutRef)        // AUDIO_SLOT.uuid, oppure stringa vuota graph-local
AYRA_DECLARE_ID (channelId)        // identita' logica del canale nel bus

//==============================================================================
// MARK: - Universal Route document (Sessione 07)
//  Una Route vive UNA sola volta nel GraphDocument piu' basso che contiene
//  entrambi i Tensor. Send, return e cue sono proiezioni/ruoli, non altri schemi.
//==============================================================================

AYRA_DECLARE_ID (ROUTES)                    // contenitore route del GraphDocument owner
AYRA_DECLARE_ID (ROUTE)                     // singola route universale
AYRA_DECLARE_ID (ownerGraphUuid)            // Graph UUID che possiede l'unica copia
AYRA_DECLARE_ID (sourceTensorUuid)          // Tensor UUID sorgente
AYRA_DECLARE_ID (sourcePort)                // porta sorgente risolta off RT
AYRA_DECLARE_ID (sourcePortDomain)          // tensorPort
AYRA_DECLARE_ID (sourcePortName)            // label cached user-facing della porta sorgente
AYRA_DECLARE_ID (routeSourceChannelId)      // vuoto = bus intero; altrimenti channelId
AYRA_DECLARE_ID (destinationTensorUuid)     // Tensor UUID destinazione
AYRA_DECLARE_ID (destinationPort)           // porta destinazione risolta off RT
AYRA_DECLARE_ID (streamMask)                // bitmask audio/MIDI/DMX/OSC/image/video/string/param
AYRA_DECLARE_ID (routeGainDb)               // gain persistito in dB, compilato lineare
AYRA_DECLARE_ID (routeBalance)              // balance -1..+1
AYRA_DECLARE_ID (routeEnabled)              // enable live (non e' il tombstone)
AYRA_DECLARE_ID (mergePolicy)               // auto/sum/midiMerge/dmx*/oscTimestamp/exclusive
AYRA_DECLARE_ID (compensationPolicy)        // automatic/disabled/fixed
AYRA_DECLARE_ID (fixedCompensationSamples)  // latenza fissa se policy=fixed
AYRA_DECLARE_ID (feedbackDelaySamples)      // FeedbackDelay esplicito; 0 = arco normale
AYRA_DECLARE_ID (routeEventFilterMask)      // event key mask per MIDI/OSC/DMX/string/param
AYRA_DECLARE_ID (routeEventFilterValue)     // valore richiesto dopo applicazione mask

//==============================================================================
// MARK: - Porte logiche dei Tensor
//  I main input/output sono impliciti e canonici. Qui persistono soltanto le
//  porte custom create dall'utente dentro il GRAPH del Tensor proprietario.
//==============================================================================

AYRA_DECLARE_ID (TENSOR_INPUT_PORTS)
AYRA_DECLARE_ID (TENSOR_INPUT_PORT)
AYRA_DECLARE_ID (TENSOR_OUTPUT_PORTS)
AYRA_DECLARE_ID (TENSOR_OUTPUT_PORT)
AYRA_DECLARE_ID (portRole)
AYRA_DECLARE_ID (portChannels)
AYRA_DECLARE_ID (routeEventKeyXor)          // trasformazione key precompilata
AYRA_DECLARE_ID (routeEventValueScale)      // trasformazione scalare del valore evento
AYRA_DECLARE_ID (routeDmxPriority)           // priorita' esplicita 0..255 per merge dmxPriority
AYRA_DECLARE_ID (routeDestinationChannelId) // vuoto = bus intero; altrimenti channelId
AYRA_DECLARE_ID (routeRole)                 // ruolo UI: generic/cue/return/monitor
AYRA_DECLARE_ID (cueGainMode)               // Cue: "independentDb" | "followMain"
AYRA_DECLARE_ID (cueFollowMainPercent)      // Cue: float 0..400, 100 = unity sul source post-fader
AYRA_DECLARE_ID (sourceRef)                 // riferimento UI alla sorgente canonica
AYRA_DECLARE_ID (sourceKind)                // tipo cached della sorgente
AYRA_DECLARE_ID (removed)                   // tombstone Route preservato per undo/resurrezione

//==============================================================================
// MARK: - Scope mix Project / Song e guidance inheritance
//  Autorita' canoniche del documento Multiverse. Le proprieta' `*Override*` e
//  cue/master sono OPZIONALI: assente = Inherited, presente = Detached.
//==============================================================================

// Project Main: le basi Project sono l'autorita' runtime. Gli ID override
// per-Song restano soltanto per compatibilita' dello schema corrente e verranno
// rimossi con la prossima migrazione documentale.
AYRA_DECLARE_ID (projectMainGainDb)          // float dB: base Project Main
AYRA_DECLARE_ID (projectMainPan)             // float -1..+1: base Project Main
AYRA_DECLARE_ID (projectMainMuted)           // bool: mute Project Main
AYRA_DECLARE_ID (projectMainWidth)           // float 0..2: width Project Main
AYRA_DECLARE_ID (projectMainPolarityMask)    // int64: polarity per canale
AYRA_DECLARE_ID (projectMainSafetyEnabled)   // bool: safety finale Project Main
AYRA_DECLARE_ID (projectClickRouted)         // bool: click instradato al Program Main
AYRA_DECLARE_ID (projectCounterRouted)       // bool: counter instradato al Program Main

// Controllo intrinseco delle tracce Interlude. Non e' un plugin del Tensor:
// il runtime applica questo stadio fra il Tensor performer e il master Interlude.
AYRA_DECLARE_ID (interludeGainDb)             // float dB
AYRA_DECLARE_ID (interludePan)                // float -1..+1
AYRA_DECLARE_ID (interludeMuted)              // bool

AYRA_DECLARE_ID (projectMainGainOverrideDb)  // float dB: override song detached
AYRA_DECLARE_ID (projectMainPanOverride)     // float -1..+1: override song detached
AYRA_DECLARE_ID (projectMainMuteOverride)    // bool: override song detached
AYRA_DECLARE_ID (projectMainWidthOverride)   // float 0..2: override song detached
AYRA_DECLARE_ID (projectMainPolarityOverride)// int64: override song detached
AYRA_DECLARE_ID (projectMainSafetyOverride)  // bool: override song detached

// Output monitor + routing/default guidance: proprieta' del roster Performer.
AYRA_DECLARE_ID (monitorOutputGainDb)         // float dB: livello finale monitor performer
AYRA_DECLARE_ID (monitorOutputPan)            // float -1..+1: pan finale monitor performer
AYRA_DECLARE_ID (monitorOutputMuted)          // bool: mute finale monitor performer
AYRA_DECLARE_ID (monitorOutputWidth)          // float 0..2: width finale monitor
AYRA_DECLARE_ID (monitorOutputPolarityMask)   // int64: polarity finale monitor
AYRA_DECLARE_ID (guidanceClickRouted)         // bool: click instradato all'output finale
AYRA_DECLARE_ID (guidanceCounterRouted)       // bool: counter instradato all'output finale
AYRA_DECLARE_ID (defaultClickGainDb)          // float dB: default Project del performer
AYRA_DECLARE_ID (defaultClickCueGainMode)     // "independentDb" | "followMain"
AYRA_DECLARE_ID (defaultClickCueFollowMainPercent) // float 0..400
AYRA_DECLARE_ID (defaultClickPan)             // float -1..+1: default Project del performer
AYRA_DECLARE_ID (defaultCounterGainDb)        // float dB: default Project del performer
AYRA_DECLARE_ID (defaultCounterCueGainMode)   // "independentDb" | "followMain"
AYRA_DECLARE_ID (defaultCounterCueFollowMainPercent) // float 0..400
AYRA_DECLARE_ID (defaultCounterPan)           // float -1..+1: default Project del performer
AYRA_DECLARE_ID (songMainCueRouted)           // bool: Song Main instradata alla Cue performer
AYRA_DECLARE_ID (defaultSongMainCueGainDb)    // float dB: default Song Main nella Cue
AYRA_DECLARE_ID (defaultSongMainCueGainMode)   // "independentDb" | "followMain"
AYRA_DECLARE_ID (defaultSongMainCueFollowMainPercent) // float 0..400
AYRA_DECLARE_ID (defaultSongMainCuePan)       // float -1..+1: default Song Main nella Cue

// Gate mute gerarchico della Song Main. Gain/pan appartengono ai parametri
// persistenti dei nodi della Main Chain nel GraphDocument.
AYRA_DECLARE_ID (songBusMuted)                // bool

// Enable musicale e schema della futura automazione relativa.
AYRA_DECLARE_ID (guidanceClickEnabled)        // bool song-owned
AYRA_DECLARE_ID (guidanceCounterEnabled)      // bool song-owned
AYRA_DECLARE_ID (AUTOMATION_SCHEMA)           // singleton child della Song
AYRA_DECLARE_ID (AUTOMATION_TARGET)           // target persistente canonico della lane timeline
AYRA_DECLARE_ID (gainAutomationMode)          // "relative"
AYRA_DECLARE_ID (panAutomationMode)           // "relative"
AYRA_DECLARE_ID (automationWriteMode)         // "absolute" | "trim" | "relative"

// Override guidance sulla participation.
AYRA_DECLARE_ID (cueClickGain)     // float dB: override click; assente = inherited
AYRA_DECLARE_ID (cueClickGainMode) // string opzionale: assente = inherit default Project
AYRA_DECLARE_ID (cueClickFollowMainPercent) // float 0..400 opzionale: assente = inherit
AYRA_DECLARE_ID (cueClickPan)      // float -1..+1: override click; assente = inherited
AYRA_DECLARE_ID (cueCounterGain)   // float dB: override counter; assente = inherited
AYRA_DECLARE_ID (cueCounterGainMode)
AYRA_DECLARE_ID (cueCounterFollowMainPercent)
AYRA_DECLARE_ID (cueCounterPan)    // float -1..+1: override counter; assente = inherited
AYRA_DECLARE_ID (cueSongMainGain)  // float dB: override Song Main; assente = inherited
AYRA_DECLARE_ID (cueSongMainGainMode)
AYRA_DECLARE_ID (cueSongMainFollowMainPercent)
AYRA_DECLARE_ID (cueSongMainPan)   // float -1..+1: override Song Main; assente = inherited
AYRA_DECLARE_ID (masterClickPan)   // legacy: vecchio override Main per-Song, ignorato dal render corrente
AYRA_DECLARE_ID (masterCounterPan) // legacy: vecchio override Main per-Song, ignorato dal render corrente

//==============================================================================
// MARK: - Identita' & naming
//==============================================================================

AYRA_DECLARE_ID (itemID)
AYRA_DECLARE_ID (uuid)             // identificatore stabile (risoluzione binding)
AYRA_DECLARE_ID (uid)
AYRA_DECLARE_ID (name)
AYRA_DECLARE_ID (type)
AYRA_DECLARE_ID (value)
AYRA_DECLARE_ID (orderIndex)
AYRA_DECLARE_ID (colour)
AYRA_DECLARE_ID (note)
AYRA_DECLARE_ID (ticked)

//==============================================================================
// MARK: - Stati nodo / grafo
//==============================================================================

AYRA_DECLARE_ID (shutdown)
AYRA_DECLARE_ID (bypass)
// Lifecycle current-only: le durate sono stringhe TimeValuePersistence,
// non campioni e unita' parallelamente editabili.
AYRA_DECLARE_ID (shutdownFadeDuration)
AYRA_DECLARE_ID (bypassFadeDuration)
AYRA_DECLARE_ID (sendPanicAtShutdown)
AYRA_DECLARE_ID (sendPanicAtBypass)
AYRA_DECLARE_ID (sendNoteOff)
AYRA_DECLARE_ID (sendSustainOff)
AYRA_DECLARE_ID (sendPitchWheelReset)
AYRA_DECLARE_ID (sendModulationWheelOff)
AYRA_DECLARE_ID (sendControllerOff)
AYRA_DECLARE_ID (sendSoundOff)
AYRA_DECLARE_ID (mute)
AYRA_DECLARE_ID (solo)
AYRA_DECLARE_ID (mixerVisible)     // performer/traccia visibile nel Mixer (Multiverse). Assente = visibile. NB: distinto da ids::visibility (visibilita' nel CANVAS del grafo).
AYRA_DECLARE_ID (mixerCollapsed)   // performer RIPIEGATO nel Mixer: resta solo la strip di convergenza. View-state persistito (um=nullptr, non-undoable)
AYRA_DECLARE_ID (monitor)
AYRA_DECLARE_ID (lock)
AYRA_DECLARE_ID (active)
AYRA_DECLARE_ID (compensated)
AYRA_DECLARE_ID (bufferized)
AYRA_DECLARE_ID (bufferizationTimes)
AYRA_DECLARE_ID (oversampled)
AYRA_DECLARE_ID (oversampleStage)
AYRA_DECLARE_ID (oversampleFilterType)
AYRA_DECLARE_ID (oversampleQuality)
AYRA_DECLARE_ID (preload)
AYRA_DECLARE_ID (defaultNode)
AYRA_DECLARE_ID (useARA)

//==============================================================================
// MARK: - Layout/posizione GUI (resolution-independent 0..1)
//==============================================================================

AYRA_DECLARE_ID (posX)
AYRA_DECLARE_ID (posY)
AYRA_DECLARE_ID (visibility)
AYRA_DECLARE_ID (collapsed)
AYRA_DECLARE_ID (editorOpened)
AYRA_DECLARE_ID (windowBounds)
AYRA_DECLARE_ID (customName)          // nome custom del nodo (override del nome plugin)
AYRA_DECLARE_ID (displayCustomName)   // se mostrare il customName sulla card (default true)

//==============================================================================
// MARK: - Connessioni & routing
//==============================================================================

AYRA_DECLARE_ID (sourceNodeUuid)
AYRA_DECLARE_ID (sourceChannel)
AYRA_DECLARE_ID (sourceChannelId)
AYRA_DECLARE_ID (destinationNodeUuid)
AYRA_DECLARE_ID (destinationChannel)
AYRA_DECLARE_ID (destinationChannelId)
AYRA_DECLARE_ID (defaultConnection) // ownership del reconciler; non implica immutabilita' del cavo
AYRA_DECLARE_ID (DEFAULT_CONNECTION_EXCLUSIONS) // tombstone dei seed di boundary rimossi dall'utente
AYRA_DECLARE_ID (DEFAULT_CONNECTION_EXCLUSION)  // una esclusione diagonale per ordinale, non una matrice
AYRA_DECLARE_ID (channelOrdinal)                // indice 0-based del seed diretto escluso
AYRA_DECLARE_ID (boundarySide)                  // intento semantico "input" oppure "output"
// ID device stabile quando un estremo tocca un pin device-bound (IO node MIDI/
// DMX/OSC/Image): permette di rimappare il channelIndex al reload anche se
// l'ordine dei device e' cambiato. Vuoto se l'estremo non e' device-bound.
AYRA_DECLARE_ID (sourceDeviceId)
AYRA_DECLARE_ID (destinationDeviceId)
// ID stabile di un endpoint derivato da Route. Separato dal deviceId: i pin
// Route sono posseduti dal documento di routing, non dal DeviceManagerHub.
AYRA_DECLARE_ID (sourceEndpointId)
AYRA_DECLARE_ID (destinationEndpointId)

//==============================================================================
// MARK: - Associazioni parametri (CV routing, pin-aware)
//==============================================================================

AYRA_DECLARE_ID (PARAM_ASSOCIATIONS)
AYRA_DECLARE_ID (PARAM_ASSOCIATION)
AYRA_DECLARE_ID (driverNodeUuid)
AYRA_DECLARE_ID (driverPinIndex)    // sourcePinIndex (0-based, pin di output del source)
AYRA_DECLARE_ID (driverParamIndex)  // sourceSlot
AYRA_DECLARE_ID (drivenNodeUuid)
AYRA_DECLARE_ID (drivenParamIndex)  // destParamIndex

//==============================================================================
// MARK: - Settaggi globali condivisi (vedi settings/GlobalSettings)
//==============================================================================

AYRA_DECLARE_ID (GLOBAL_SETTINGS)
AYRA_DECLARE_ID (sampleRate)
AYRA_DECLARE_ID (bufferSize)
AYRA_DECLARE_ID (videoFps)          // frame rate stream immagine, disaccoppiato da audio
AYRA_DECLARE_ID (doublePrecision)

//==============================================================================
// MARK: - Impostazioni d'app machine-local (vedi settings/AppSettings)
//  Radice <APP_SETTINGS> con branch figli; persistite nel PropertiesFile via
//  AppStorage, FUORI dal documento .ayra/.ayramv (sono per-installazione).
//==============================================================================

AYRA_DECLARE_ID (APP_SETTINGS)      // radice dell'aggregato impostazioni d'app
AYRA_DECLARE_ID (APPEARANCE)        // branch aspetto (tema + override colori nodi)
AYRA_DECLARE_ID (MONITORING)        // branch monitoring grafo (stream + cpu)
// NB: il branch "nodi di default" usa il tag prodotto da
// DefaultNodeProfile::toValueTree(), ossia "DEFAULT_NODE_PROFILE" (stesso valore;
// la string pool di JUCE garantisce l'uguaglianza per puntatore).
AYRA_DECLARE_ID (DEFAULT_NODE_PROFILE)
AYRA_DECLARE_ID (NODE_COLOURS)      // contenitore override colore per tipo di stream
AYRA_DECLARE_ID (SWATCHES)          // contenitore palette swatch (ricolora nodo manuale)
AYRA_DECLARE_ID (COLOUR)            // <COLOUR stream=".." argb=N/> riga colore

AYRA_DECLARE_ID (trackColour)       // colore-traccia standalone (string, juce::Colour::toString)
AYRA_DECLARE_ID (stream)            // nome ASCII del tipo di stream (chiave override)
AYRA_DECLARE_ID (argb)             // colore impacchettato (int, juce::Colour::getARGB)
AYRA_DECLARE_ID (graphMonitoring)   // bool: metering stream del grafo attivo
AYRA_DECLARE_ID (cpuMonitoring)     // bool: misura consumo CPU per-nodo attiva

//==============================================================================
// MARK: - Processing del grafo (vedi settings/ProcessingSettings)
//  Branch <PROCESSING> dello stato d'istanza Tensor; <APP_SETTINGS> conserva
//  soltanto il template per i nuovi documenti standalone. L'owner applica:
//    - processingMode            -> AudioProcessorGraph::setProcessingMode
//    - bufferization/oversampling whole-graph -> TensorGraphLifecycle
//  (whole-graph rimosso da ayra_graph nel refactor 2026-06: vive a livello app).
//  Mirror 1:1 del pannello per-nodo (vedi NodeSettingsComponent).
//==============================================================================

AYRA_DECLARE_ID (PROCESSING)                 // branch processing del grafo
AYRA_DECLARE_ID (processingMode)             // int: 0 singleThread / 1 multiThread / 2 automatic (default)
AYRA_DECLARE_ID (graphBufferizeEnabled)      // bool: bufferization whole-graph on/off
AYRA_DECLARE_ID (graphBufferizeTimes)        // int 2..10: cicli di accumulo blocco
AYRA_DECLARE_ID (graphOversampleEnabled)     // bool: oversampling whole-graph on/off
AYRA_DECLARE_ID (graphOversampleStage)       // int: stage entro i limiti canonici DSP
AYRA_DECLARE_ID (graphOversampleFilterType)  // int: 0 = FIR equiripple, 1 = polyphase IIR
AYRA_DECLARE_ID (graphOversampleQuality)     // bool: High Quality

//==============================================================================
// MARK: - General (preferenze d'interfaccia — vedi settings/GeneralSettings)
//  Branch <GENERAL> di <APP_SETTINGS>: preferenze d'interfaccia machine-local
//  (non legate al render). L'app le legge e le applica alla GUI (footer, clock).
//==============================================================================

AYRA_DECLARE_ID (GENERAL)                  // branch general (UI prefs d'app)
AYRA_DECLARE_ID (transportClockFormat)     // int 0..4: formato del clock (bars/ms/smpte/samples/frames)
AYRA_DECLARE_ID (snapshotCaptureTickPolicy)          // int: SnapshotCaptureTickPolicy machine-local
AYRA_DECLARE_ID (snapshotPreserveTicksOnOverwrite)    // bool: preserve parameter ticks on overwrite (default true)
AYRA_DECLARE_ID (externalMapSourceFanOut)             // bool: una sorgente External Map puo' pilotare N target (default true)

// Sezioni visibili delle channel strip del mixer Multiverse (selettore GEAR,
// pattern "Racks" Cubase). Uniformi su TUTTE le strip (allineamento righe);
// machine-local, NON stato del documento .aym. Default: tutte visibili.
AYRA_DECLARE_ID (mixerShowPan)             // bool: knob di pan
AYRA_DECLARE_ID (mixerShowToggles)         // bool: griglia toggle M/S + Byp/Buf/Ovs
AYRA_DECLARE_ID (mixerShowCues)            // bool: mandate cue compatte sopra la strip
AYRA_DECLARE_ID (mixerShowInputMeters)      // bool: bank meter IN
AYRA_DECLARE_ID (mixerShowOutputMeters)     // bool: bank meter OUT

// Preferenze della Timeline (sezione "Timeline" della settings window). Governano
// come la timeline multiverse disegna e segue il playhead; machine-local.
AYRA_DECLARE_ID (timelineDisplayMode)      // int 0=time-linear (velocita'), 1=beat-linear (battute uguali)
AYRA_DECLARE_ID (playheadFollowMode)       // int 0=off, 1=follow (paging), 2=centered (timeline scorre)
AYRA_DECLARE_ID (followSmooth)             // bool: follow morbido (lerp) invece di scatto secco
AYRA_DECLARE_ID (showSnapGrid)             // bool: overlay griglia di snap visibile
AYRA_DECLARE_ID (stopReturnsToStart)       // bool: allo Stop il playhead torna a inizio

// Click / metronomo (sezione "Click" della settings window + footer). Il timbro
// puo' avere un template machine-local; gain/pan/routing Main sono project-owned
// sul Main Performer, mentre i Cue vivono sulla participation della song.
AYRA_DECLARE_ID (clickEnabled)             // bool: click attivo (toggle footer)
AYRA_DECLARE_ID (clickLevelDb)             // float dB: livello click Tensor standalone (knob footer). -100 = -inf
AYRA_DECLARE_ID (clickHighVolume)          // float 0..1: volume relativo tick acuto (downbeat)
AYRA_DECLARE_ID (clickLowVolume)           // float 0..1: volume relativo tick grave (altri beat)
AYRA_DECLARE_ID (clickHighPitchHz)         // float: pitch tick acuto
AYRA_DECLARE_ID (clickLowPitchHz)          // float: pitch tick grave
AYRA_DECLARE_ID (clickDifferentiate)       // bool: differenzia downbeat (false = tick uguale su tutti)

// Campi legacy opzionali conservati per leggere documenti precedenti. Il render
// corrente usa i default project-owned del Main Performer e non li riscrive.
AYRA_DECLARE_ID (masterClickGain)
AYRA_DECLARE_ID (masterCounterGain)

//==============================================================================
// MARK: - Sync (branch sincronizzazione esterna; vedi settings/SyncPrefs)
//==============================================================================
// Tag del branch <SYNC>. Le proprieta' flat (technology, linkEnabled, ...) NON
// hanno id dedicati: vivono dentro ayra::SyncSettings::to/fromValueTree
// (ayra_transport), unica fonte di verita' dello schema.

AYRA_DECLARE_ID (SYNC)                      // branch sincronizzazione (SyncPrefs)

//==============================================================================
// MARK: - Channel Strip (vedi settings/ChannelStripSettings + ChannelStripLibrary)
//  Branch <CHANNEL_STRIP_LIBRARY> di <APP_SETTINGS>: il REGISTRO delle "catene di
//  canale" di default. Ogni catena e' un figlio <CHANNEL_STRIP trackType="..">
//  (un ayra::ChannelStripSettings) applicabile ai grafi Tensor (ex panpot+gain+IO
//  del vecchio NodeGraphProcessor) + 2 slot di controllo mixer (Main Volume /
//  Main Pan). Ogni <CHANNEL_STRIP> contiene come figlio il
//  <DEFAULT_NODE_PROFILE> (la catena vera e propria). `ayra_multiverse` usa piu'
//  catene (una per tipologia di traccia); `ayra_tensor` usa solo la default.
//==============================================================================

AYRA_DECLARE_ID (CHANNEL_STRIP_LIBRARY)    // branch: registro di N catene per tipologia di traccia
AYRA_DECLARE_ID (CHANNEL_STRIP)            // una catena (figlio del registro)
AYRA_DECLARE_ID (trackType)                // id legacy del template/hint visuale; NON limita gli stream del Tensor
AYRA_DECLARE_ID (label)                    // display name (ASCII) della catena / tipologia
AYRA_DECLARE_ID (enabled)                  // bool: catena di default attiva (default true)
AYRA_DECLARE_ID (prePostLinePosition)      // float 0..1: posizione del divisore PRE/POST (canvas)
AYRA_DECLARE_ID (CONTROL)                  // <CONTROL slot=".." nodeRef=".." paramId=".." paramIndex=N paramName=".."/>
AYRA_DECLARE_ID (slot)                     // nome slot di controllo: "volume" / "pan"
AYRA_DECLARE_ID (nodeRef)                  // riferimento nodo della catena: ruolo (ids::type) o uuid
AYRA_DECLARE_ID (paramName)                // display name (ASCII) del parametro assegnato
// NB: ids::paramId/paramIndex sono gia' dichiarati sopra (sezione "Parametri & bus").

//==============================================================================
// MARK: - Marker di versione documento
//==============================================================================

AYRA_DECLARE_ID (documentVersion)

} // namespace ayra::ids

#undef AYRA_DECLARE_ID
