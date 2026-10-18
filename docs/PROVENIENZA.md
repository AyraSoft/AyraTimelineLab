# Provenienza del riferimento visuale

## Repository verificata

- Repository: `AyraSoft/ayra_multiverse`.
- Commit letto: `5db35b89ddb73da6da2c5b6d3184f5a3878bda1b`.
- Documento di design: `_doc/DESIGN_LANGUAGE.md`.
- Colori: `modules/look_and_feel/tokens/ayra_Tokens_Colours.h`, blob `55d6c4a2277c24b01b75d4d047aa2deb0c9096d8`.
- Primitive: `modules/look_and_feel/tokens/ayra_Tokens.h`, blob `63dd66ec58cb086675ee876a316582532cc73299`.
- Tipografia: `modules/look_and_feel/tokens/ayra_Tokens_Typography.h`, blob `77d4127bd37acfd11cb1d5c521e62b68796af47a`.

Letture effettuate tramite il connettore GitHub; non si assumono modifiche locali non pubblicate. Nessuna scrittura alla repository in questa consegna.

## Clip approvata come base

File della precedente iterazione: `ayra-audio-regole-canali.html`.
SHA-256: `ec48b62af66190b393c264a082ba11b89f2c5650e0cb7026027f0ebd34114719`.

La parte visuale dell'HTML usa le funzioni derivate dalla base precedente: `trackVars`, `wavePath`, `waveform`, `fadeGeometry`, `fadePaths`, `fadeMarkup`. Il nuovo host sostituisce le vecchie regole per-clip con l'ordine condiviso della traccia. Il disegno dell'header, la modalita' libera e la galleria sono la nuova proposta, non una fotografia della GUI di produzione.

Restano il gradiente in rilievo del colore traccia, selezione scura, quattro waveform in un solo evento, nome sovrapposto, linee fade marcate e triangoli semitrasparenti ancorati in basso. Non vengono aggiunti colori individuali delle clip.

## Due rese e due livelli di certezza

L'HTML e' stato eseguito in Chromium; gli screenshot in `verification` provengono da quel DOM. Il renderer JUCE e' una nuova trasposizione di riferimento per il banco: usa gli stessi principi visuali ma non e' stato compilato ne' confrontato pixel per pixel con il browser. I path waveform sintetici nativi non sono gli stessi campioni della fixture JavaScript.

Il tema statico e' una fixture versionata, non una nuova palette canonica di produzione. La modalita' `AYRA_SOURCE_DIR` accede ai token e alla primitiva raised del checkout locale, non al LookAndFeel completo. Non distribuiamo font, file media o librerie terze.

`verification/source-manifest.json` identifica i sorgenti consegnati. I log del core attestano esclusivamente i file compilati nel target `ayra_lab_core` e nel suo test, non i file `app/`.
