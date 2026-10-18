# Collaudo JUCE da eseguire

**Aggiornamento 16 ottobre 2026:** build e link della GUI completati in Debug e Release su macOS x86_64 con AppleClang 17, checkout JUCE locale e bridge verso il checkout locale Ayra. Quattro target CTest passano in entrambe le configurazioni. L'app non e' stata avviata: tutte le caselle interattive, visive, DPI e di lifetime runtime restano intenzionalmente aperte.

## Build e avvio

- [x] GUI Debug con checkout JUCE locale; zero modifiche al provider JUCE.
- [x] GUI Release su macOS x86_64.
- [x] Variante `AYRA_SOURCE_DIR` con token locali; compilazione e link completati.
- [ ] Avvio, chiusura, seconda finestra/istanza e import da `--session` senza crash.
- [ ] Sanitizer della GUI, distinti dai sanitizer del solo core gia' eseguiti.

## Geometria e input

- [ ] Quattro header riconoscibili in Dark/Light; nessun Solo in Song Main.
- [ ] A 100/125/150/200%, nome, bottoni, slider e resize non si sovrappongono.
- [ ] Header largo 320, 352 e 480 px; nomi lunghi/unicode; finestra minima 1200x720.
- [ ] Un click seleziona il Tensor senza selezionare tutte le clip o cambiare tempo.
- [ ] Ctrl/Cmd e Shift rispettano selezione multipla e intervallo visivo.
- [ ] M/S, enable, bypass, Graph, Routing e settings non avviano drag/resize.
- [ ] Click destro non cambia M/S; apre impostazioni sul corpo dell'header.
- [ ] Resize con schermo in movimento, rilascio, Escape, perdita del focus, cambio sessione.
- [ ] Riordino dal grip e Alt+Su/Giu'; globali mai spostabili; Main pinnata protetta.
- [ ] Blocchi multiselezionati mantengono ordine relativo; drop su se stessi e senza movimento non crea Undo.
- [ ] Durante il drag fuori viewport, scroll corretto; fine gesto fuori componente e annullamento.

## Identita', controlli e ciclo di vita

- [ ] Timeline e mini-mixer seguono gli stessi UUID senza divergere dopo rename/colore/mute.
- [ ] Riordinare e cambiare modalita' non modifica clip, ordine dei canali, group o route.
- [ ] MIX resta raggiungibile ripiegando i gruppi; ordini personali ripristinati tornando in Libera.
- [ ] Gain/pan via drag e campo numerico: una sola transazione significativa a gesto concluso.
- [ ] Cambio tema/resize non perde un campo testo attivo; Undo non intercetta editing del testo.
- [ ] Menu e file chooser aperti, poi chiusura finestra: callback obsolete innocue.
- [ ] Import invalido rifiutato senza modifiche parziali; import valido annullabile.
- [ ] JSON esportato dal browser aperto nella GUI e viceversa: stessi ID, controlli, ordine e mapping.

## Clip nel contesto del banco

- [ ] Stereo nella lane a quattro posizioni: L1, vuoto2, vuoto3, R4 visibili dentro un unico evento.
- [ ] Channels/Mapping dal doppio click: bozza, frecce, annulla, applica tutte, Undo.
- [ ] Fade numerici validati, triangoli dal basso, clip piccole senza geometrie invalide.
- [ ] Il passaggio a `AudioClipComponent` non cambia pixel, z-order, hover, cursori o cattura mouse rispetto al canvas precedente.
- [ ] Rail warp superiore e rail cut inferiore non vengono tagliati dai bounds del child component.
- [ ] Nessuna prova della GUI del Lab viene riusata come certificazione del runtime Multiverse.

## Prestazioni e limiti

- [ ] Profilare layout/paint con cardinalita' reali, senza trattare i limiti difensivi come benchmark.
- [ ] Misurare creazione/reparenting dei componenti e invalidazione cache.
- [ ] Prima di dataset grandi introdurre la virtualizzazione nel provider corretto, non in un fast-path app-only.
- [ ] Screenshot nativi confrontati con HTML; differenze deliberate e metriche dei font documentate.

Il Lab non esercita il motore, il dispositivo audio, plugin esterni, il trasporto, DSP o routing reale. La loro validazione deve avvenire nel progetto di produzione e con i suoi owner, non con le formule dimostrative del banco.
