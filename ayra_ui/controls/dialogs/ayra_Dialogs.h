/*
  ==============================================================================

   This file is part of the Ayra ecosystem.
   Copyright (c) Ayra Productions.

   ayra_common / widgets / controls / Dialogs (FACADE)
   ------------------------------------------------------------------------------
   Dialog modali (alert / conferma / text-entry) STILATI con la LookAndFeel
   d'app (Frosted Lab), drop-in sincroni per le statiche JUCE.

   ## Perche' esistono
   In contesto PLUGIN la `MultiverseLookAndFeel` e' SCOPED sull'editor (niente
   `setDefaultLookAndFeel`, per non contaminare l'host). Conseguenza: ogni
   finestra desktop TOP-LEVEL creata "al volo" -- `juce::AlertWindow::showOkCancelBox`,
   `EnterTextDialogWindow`, ecc. -- ricade sulla LookAndFeel di DEFAULT di JUCE
   (grigia, fuori tema), perche' non eredita la LAF dell'editor.

   La cura, gia' usata per i CallOutBox (vedi MacrosPanel::openMacroMapCallout) e
   per le DocumentWindow (PluginEditor::applyAppLaf), e' la stessa qui: si crea il
   dialog e gli si IMPONE la LAF dell'anchor con `setLookAndFeel
   (&anchor.getLookAndFeel())`. L'anchor e' un qualsiasi component vivo dentro
   l'editor: il suo `getLookAndFeel()` risolve la `MultiverseLookAndFeel` scoped.

   Cosi' l'alert usa `drawAlertBox` (card + accent rail + dual-rim), i bottoni
   usano `drawButtonBackground`/`drawButtonText` e il text editor usa le ricette
   form del LAF: dialog coerente coi pannelli, senza toccare la LAF di processo.

   ## Uso (drop-in)
   @code
   // prima:  if (juce::AlertWindow::showOkCancelBox (icon, title, msg)) {...}
   // dopo:   if (ayra::showOkCancelDialog (*this, title, msg)) {...}

   // prima:  auto [text, ok] = ayra::EnterTextDialogWindow::showEnterTextDialogWindow (title, def);
   // dopo:   auto [text, ok] = ayra::showTextEntryDialog (*this, title, def);
   @endcode

   ## Nota modal-loop
   Sincroni (`runModalLoop`): richiedono `JUCE_MODAL_LOOPS_PERMITTED=1`, gia'
   attivo nel progetto (lo usano sia `showOkCancelBox` sia `EnterTextDialogWindow`).
   `AlertWindow` e' un `TopLevelWindow`: si auto-aggiunge al desktop quando diventa
   visibile (`enterModalState` -> `setVisible`), quindi non serve `addToDesktop`.

   Doctrine: header-only (inline), ZERO #include (juce gia' visibile via
   ayra_common.h), namespace ayra, ASCII-only nei letterali stringa.

  ==============================================================================
*/

#pragma once

namespace ayra
{

//==============================================================================
// PERCHE' ASYNC E' IL DEFAULT (e i sincroni qui sotto sono [[deprecated]])
// ------------------------------------------------------------------------------
// `runModalLoop()` / le statiche `juce::AlertWindow::show*Box` BLOCCANO il
// chiamante in un message loop ANNIDATO. Su macOS standalone, con l'audio device
// vivo, questo loop annidato resta in attesa di eventi nella coda di sistema e
// l'OS puo' ritardare la consegna del click (coalescing eventi, App Nap,
// interplay con l'audio thread): l'utente percepisce la GUI "congelata" per
// secondi dopo aver premuto il bottone -- intermittente, fino a ~10s.
//
// Le varianti `*Async` NON entrano mai in un loop annidato: creano la finestra
// sull'heap, `enterModalState` con callback, ritornano SUBITO. Il message thread
// resta libero -> esperienza fulminea. Stesso identico pattern gia' usato
// nell'app (`showAlertDialogAsync`, `CallOutBox::launchAsynchronously`,
// `PopupMenu::showMenuAsync`). Preferiscile SEMPRE.
//
// NOTE SULLA SAFETY DELLA CALLBACK: con `deleteWhenDismissed=true` JUCE invoca
// la callback PRIMA di distruggere la finestra, quindi leggere il testo della
// editor (`getTextEditorContents`) dentro la callback e' sicuro. Se la callback
// tocca il chiamante (`this`), cattura un `juce::Component::SafePointer`: il
// dialog e' modale ma il chiamante potrebbe essere distrutto da altro codice
// prima che la callback giri.
//==============================================================================

//==============================================================================
/** OK/Cancel NON bloccante, stilato con la LAF dell'anchor. Drop-in async per
    `juce::AlertWindow::showOkCancelBox`: ritorna subito, il dialog vive sul
    desktop e si auto-distrugge alla chiusura.

    @param anchor    component vivo che porta la LAF d'app (tipicamente `*this`).
    @param onResult  invocato sul message thread con `true` se confermato
                     (OK / Return), `false` altrimenti (Cancel / Esc). */
inline void showOkCancelDialogAsync (juce::Component& anchor,
                                     const juce::String& title,
                                     const juce::String& message,
                                     std::function<void (bool)> onResult,
                                     const juce::String& okText     = "OK",
                                     const juce::String& cancelText = "Cancel",
                                     juce::MessageBoxIconType icon = juce::MessageBoxIconType::QuestionIcon)
{
    auto* w = new juce::AlertWindow (title, message, icon, &anchor);
    w->setLookAndFeel (&anchor.getLookAndFeel());
    w->addButton (okText,     1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton (cancelText, 0, juce::KeyPress (juce::KeyPress::escapeKey));
    w->enterModalState (true,
        juce::ModalCallbackFunction::create ([cb = std::move (onResult)] (int r) { if (cb) { cb (r == 1); } }),
        true);   // deleteWhenDismissed
}

//==============================================================================
/** Tre bottoni NON bloccante, stilato con la LAF dell'anchor (es. Salva / Scarta /
    Annulla per un prompt "salvare le modifiche?"). Ritorna subito; il dialog vive sul
    desktop e si auto-distrugge alla chiusura. Stesso pattern di `showOkCancelDialogAsync`.

    @param onResult  invocato sul message thread con l'indice del bottone premuto:
                     0 = `button0` (default, Return), 1 = `button1`, 2 = `button2` (Esc).
                     La chiusura via Esc / X mappa su `button2` (la via d'uscita "annulla"). */
inline void showThreeButtonDialogAsync (juce::Component& anchor,
                                        const juce::String& title,
                                        const juce::String& message,
                                        std::function<void (int)> onResult,
                                        const juce::String& button0,
                                        const juce::String& button1,
                                        const juce::String& button2,
                                        juce::MessageBoxIconType icon = juce::MessageBoxIconType::QuestionIcon)
{
    auto* w = new juce::AlertWindow (title, message, icon, &anchor);
    w->setLookAndFeel (&anchor.getLookAndFeel());
    w->addButton (button0, 0, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton (button1, 1);
    w->addButton (button2, 2, juce::KeyPress (juce::KeyPress::escapeKey));
    w->enterModalState (true,
        juce::ModalCallbackFunction::create ([cb = std::move (onResult)] (int r) { if (cb) { cb (r); } }),
        true);   // deleteWhenDismissed
}

//==============================================================================
/** Text-entry NON bloccante, stilato. Drop-in async per `showTextEntryDialog`:
    ritorna subito, il dialog vive sul desktop e si auto-distrugge alla chiusura.

    @param onResult  invocato sul message thread con { testo digitato, confermato }.
                     Il testo e' valido solo se il `bool` e' `true`. */
inline void showTextEntryDialogAsync (juce::Component& anchor,
                                      const juce::String& title,
                                      const juce::String& defaultText,
                                      std::function<void (juce::String, bool)> onResult,
                                      const juce::String& okText     = "Enter",
                                      const juce::String& cancelText = "Discard")
{
    auto* w = new juce::AlertWindow (title, juce::String(), juce::MessageBoxIconType::NoIcon, &anchor);
    w->setLookAndFeel (&anchor.getLookAndFeel());

    const juce::String field { "text" };
    w->addTextEditor (field, defaultText);
    if (auto* ed = w->getTextEditor (field)) { ed->setJustification (juce::Justification::centred); }

    w->addButton (okText,     1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton (cancelText, 0, juce::KeyPress (juce::KeyPress::escapeKey));

    w->enterModalState (true,
        juce::ModalCallbackFunction::create ([w, field, cb = std::move (onResult)] (int r)
        {
            // deleteWhenDismissed invoca questa callback PRIMA della distruzione:
            // leggere il contenuto dell'editor qui e' sicuro.
            if (cb) { cb (w->getTextEditorContents (field), r == 1); }
        }),
        true);   // deleteWhenDismissed
}

//==============================================================================
/** OK/Cancel modale, stilato con la LAF dell'anchor. Drop-in sincrono per
    `juce::AlertWindow::showOkCancelBox`.

    @warning BLOCCANTE: entra in un message loop annidato (vedi nota sopra).
             Preferisci `showOkCancelDialogAsync`. Lasciato per contesti
             realmente sincroni (script/tool headless), non per la GUI.

    @param anchor      component vivo che porta la LAF d'app (tipicamente `*this`);
                       il dialog viene anche centrato attorno ad esso.
    @returns true se l'utente ha confermato (OK / Return), false altrimenti. */
[[nodiscard]] [[deprecated ("BLOCCANTE: usa showOkCancelDialogAsync")]]
inline bool showOkCancelDialog (juce::Component& anchor,
                                const juce::String& title,
                                const juce::String& message,
                                const juce::String& okText     = "OK",
                                const juce::String& cancelText = "Cancel",
                                juce::MessageBoxIconType icon = juce::MessageBoxIconType::QuestionIcon)
{
    juce::AlertWindow w (title, message, icon, &anchor);
    w.setLookAndFeel (&anchor.getLookAndFeel());
    w.addButton (okText,     1, juce::KeyPress (juce::KeyPress::returnKey));
    w.addButton (cancelText, 0, juce::KeyPress (juce::KeyPress::escapeKey));
    return w.runModalLoop() == 1;
}

//==============================================================================
/** Messaggio modale a un solo bottone (informazione / avviso), stilato. Drop-in
    per `juce::AlertWindow::showMessageBox`. */
inline void showAlertDialog (juce::Component& anchor,
                             const juce::String& title,
                             const juce::String& message,
                             const juce::String& buttonText = "OK",
                             juce::MessageBoxIconType icon = juce::MessageBoxIconType::InfoIcon)
{
    juce::AlertWindow w (title, message, icon, &anchor);
    w.setLookAndFeel (&anchor.getLookAndFeel());
    w.addButton (buttonText, 1, juce::KeyPress (juce::KeyPress::returnKey),
                                juce::KeyPress (juce::KeyPress::escapeKey));
    w.runModalLoop();
}

//==============================================================================
/** Messaggio NON bloccante (un bottone), stilato. Drop-in per
    `juce::AlertWindow::showMessageBoxAsync`: ritorna subito, il dialog vive sul
    desktop e si auto-distrugge alla chiusura.

    Stesso pattern interno di JUCE (AlertWindow su heap + `enterModalState` con
    `deleteWhenDismissed`), ma con la LAF dell'anchor imposta -> Frosted Lab.

    @param onDismiss  callback opzionale invocato alla chiusura (message thread). */
inline void showAlertDialogAsync (juce::Component& anchor,
                                  const juce::String& title,
                                  const juce::String& message,
                                  const juce::String& buttonText = "OK",
                                  juce::MessageBoxIconType icon = juce::MessageBoxIconType::WarningIcon,
                                  std::function<void()> onDismiss = {})
{
    auto* w = new juce::AlertWindow (title, message, icon, &anchor);
    w->setLookAndFeel (&anchor.getLookAndFeel());
    w->addButton (buttonText, 0, juce::KeyPress (juce::KeyPress::returnKey),
                                 juce::KeyPress (juce::KeyPress::escapeKey));
    w->enterModalState (true,
        juce::ModalCallbackFunction::create ([cb = std::move (onDismiss)] (int) { if (cb) { cb(); } }),
        true);   // deleteWhenDismissed: la finestra si auto-distrugge alla chiusura
}

//==============================================================================
/** Text-entry modale, stilato. Drop-in sincrono per
    `ayra::EnterTextDialogWindow::showEnterTextDialogWindow`.

    @warning BLOCCANTE: entra in un message loop annidato (vedi nota sopra).
             Preferisci `showTextEntryDialogAsync`.

    @returns coppia { testo digitato, confermato }. Il testo e' restituito anche
             su annullamento (coerente col comportamento di EnterTextDialogWindow);
             il chiamante guarda il `bool` per sapere se applicare. */
[[nodiscard]] [[deprecated ("BLOCCANTE: usa showTextEntryDialogAsync")]]
inline std::tuple<juce::String, bool>
showTextEntryDialog (juce::Component& anchor,
                     const juce::String& title,
                     const juce::String& defaultText = juce::String(),
                     const juce::String& okText      = "Enter",
                     const juce::String& cancelText  = "Discard")
{
    juce::AlertWindow w (title, juce::String(), juce::MessageBoxIconType::NoIcon, &anchor);
    w.setLookAndFeel (&anchor.getLookAndFeel());

    const juce::String field { "text" };
    w.addTextEditor (field, defaultText);
    if (auto* ed = w.getTextEditor (field)) { ed->setJustification (juce::Justification::centred); }

    w.addButton (okText,     1, juce::KeyPress (juce::KeyPress::returnKey));
    w.addButton (cancelText, 0, juce::KeyPress (juce::KeyPress::escapeKey));

    const bool ok = (w.runModalLoop() == 1);
    return { w.getTextEditorContents (field), ok };
}

} // namespace ayra
