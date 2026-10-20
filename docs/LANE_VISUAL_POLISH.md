# Card centrata e controlli raggruppati

La richiesta del 20 ottobre aggiorna il layout dell’HTML approvato senza cambiare
la sua gerarchia, il carattere passivo della card o l’editing delle clip.

- `TimelineIdentityLayout::make`: card al centro verticale dell’area contenuto,
  sia manuale sia compatta/automatica; rounding al pixel con errore massimo 0.5 px.
  Nome traccia e tipo terminano allo stesso margine destro. Performer a sinistra.
- `TimelineIdentityArtwork`: maschera comune al materiale, al rim superiore e
  al vero blur del canvas. I primi 9 px a sinistra sono trasparenti, prima dei
  due angoli da 7 px; la dissolvenza smoothstep termina a 25 px. Il rettangolo
  logico per l’hover resta stabile. La cache viene preparata solo dopo invalidazione.
- `TimelineLaneControls`: un solo `drawFlagToolbarWell` per gruppo, dalla fonte
  Ayra selezionata. Le icone non disegnano incassi individuali. I chevron singoli
  riprendono geometria e stroke del `TimelineTrackHeaderWidget` canonico, non
  `glyphs::rewind`. I feedback hover/focus non diventano nuove superfici recessed.
- `ayra_timeline` espone `leftGroup`, `rightGroup` e
  `minimumTimelineLaneChromeHeight`: griglia sinistra 2 colonne, destra 1 colonna,
  hit target 22 px non ridotti dal resize. I gruppi vengono centrati verticalmente.
- `Controller` risolve il minimo da capability e visibilità; la geometria e
  l’indice delle righe usano lo stesso valore. Il minimo visuale prevale anche
  su un massimo storico troppo basso. I valori del documento restano immutati.
  Auto-height mantiene la propria misura sotto il floor e non provoca loop.
- L’accento laterale rappresenta solo la traccia. Nessuna strip di tipo vi è
  sovrapposta. Il colore tipo resta nel pallino della card.

Il menu, l’Undo della vista, la selezione via accento e il resize conservano i
contratti esistenti. Nessun editor nomi: tutti i gesti arrivano ai canvas.
La card si nasconde soltanto con una clip sotto il puntatore e resta nascosta
fino all’uscita dal proprio rettangolo, senza lampeggiare sopra i vuoti interni.

Provider richiesto: `AyraSoft/ayra_timeline` main, commit `ddf196e9` o compatibile.
I sorgenti privati non vengono copiati nel Lab. `AYRA_SOURCE_DIR` mantiene il
collegamento ai painter originali; l’opt-out snapshot resta esplicito.

Vedere [esecuzioni della revisione](../verification/lane-polish/STATUS.md).
