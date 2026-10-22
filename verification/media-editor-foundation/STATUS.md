# Verifica: media editor foundation

Data: 22 ottobre 2026. Ambiente esecutore Linux x86_64 della sessione; non il Mac dell'utente. Base del repository: `48502a95c92ce10a8a974dc792bae45027ab0ab0`.

## Test realmente eseguiti

| Configurazione | Toolchain | Esito |
|---|---|---|
| Debug | GCC 14.2.0, CMake 3.31.6 | 1/1 CTest, 20/20 casi PASS |
| Release | GCC 14.2.0, CMake 3.31.6 | 1/1 CTest, 20/20 casi PASS |
| Debug + AddressSanitizer/UndefinedBehaviorSanitizer | Clang 17.0.0, CMake 3.31.6 | 1/1 CTest, 20/20 casi PASS; nessuna diagnostica sanitizer osservata |

Warning trattati come errori. I 20 casi sono scenari del prototipo, non 20 test end-to-end dell'app. Il caso esaustivo verifica 99 posizioni di split e 103 istanti per posizione nella piccola fixture. I check restano attivi in Release. I log completi delle tre esecuzioni sono inclusi in questa cartella.

Copertura: mapping source-relative, move, trim con parole parziali, split ai confini e dentro le parole, cancellazione dei binding, duplicazione indipendente, transcript condiviso, rollback del batch, revisione obsoleta, riferimenti/ID invalidi, overflow, range negativi, caption nascoste, clip senza caption e revisione esaurita.

## Limiti

Non eseguiti: build/test dell'intero Lab, GUI nativa, regressioni visive, ascolto audio, import/save/open progetto, trascrizione Whisper, render Remotion, export FFmpeg, test macOS/Windows, CI remota o benchmark con video reali. Il prototipo è a velocità 1x, senza ripple/nesting o serializzazione. La riuscita dei test non dimostra qualità ASR o sincronizzazione audiovisiva in riproduzione.

La clone pubblica via terminale è fallita per DNS. Lettura e pubblicazione su GitHub usano il connettore; il test locale riguarda soltanto i nuovi file autonomi, non una checkout completa con provider privati. Nessun software è stato installato sul dispositivo dell'utente.

## Impronta dei sorgenti testati

SHA-256 dei file, per confrontare il codice pubblicato con quello eseguito:

```text
46bac3fd473d9a8fe836c1661022c164ed3252c90e66d23a55981876f3491de5  prototypes/media_document/CaptionDocument.h
fa29011ef0fefed82505bd801105549182df3fe0c9340af1c6121fba1fedad3f  prototypes/media_document/CaptionDocumentTests.cpp
a8b9085117b831829cdedb9c9dd0c008c90bd821e627ee1d6e8e8829ea06c8fd  prototypes/media_document/CMakeLists.txt
```

Per ripetere le prove vedere il README del prototipo. Non serve JUCE né accesso di rete.
