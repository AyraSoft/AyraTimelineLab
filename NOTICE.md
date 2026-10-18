# Provenienza e dipendenze

I sorgenti del Lab sono stati preparati per Ayra come materiale di progettazione e test. Il pacchetto non include JUCE, i moduli Ayra completi, binari di plugin, file audio o file di font.

Le parti del disegno delle clip derivate dal mockup HTML precedente e i valori dello snapshot dei token sono identificati in `docs/PROVENIENZA.md`. I diritti sui sorgenti originali Ayra restano dei rispettivi titolari. Non si attribuisce al Lab una nuova licenza sui sorgenti originali.

L'opzione `AYRA_SOURCE_DIR` legge i file dal checkout dell'utente senza copiarli. JUCE e gli strumenti di test devono essere ottenuti e utilizzati secondo le rispettive licenze. Il tag di download JUCE e' esplicito nel CMake e il download e' disattivato per default.
