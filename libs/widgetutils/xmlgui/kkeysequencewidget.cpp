/* This file is part of the KDE libraries
    SPDX-FileCopyrightText: 1998 Mark Donohoe <donohoe@kde.org>
    SPDX-FileCopyrightText: 2001 Ellis Whitehead <ellis@kde.org>
    SPDX-FileCopyrightText: 2007 Andreas Hartmetz <ahartmetz@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "kkeysequencewidget.h"
#include "kkeysequencewidget_p.h"
#include "config-xmlgui.h"
#include <QAction>
#include <QKeyEvent>
#include <QTimer>
#include <QHBoxLayout>
#include <QToolButton>
#include <QApplication>
#include <QDebug>

#include <klocalizedstring.h>
#include <kmessagebox.h>
#include "kactioncollection.h"

#include <kis_icon_utils.h>

#include <QtGui/private/qkeymapper_p.h>


uint qHash(const QKeySequence &seq)
{
    return qHash(seq.toString());
}

class KisKKeySequenceWidgetPrivate
{
public:
    KisKKeySequenceWidgetPrivate(KisKKeySequenceWidget *q);

    void init();

    static QKeySequence appendToSequence(const QKeySequence &seq, int keyQt);
    static bool isOkWhenModifierless(int keyQt);

    void updateShortcutDisplay();
    void startRecording();

    /**
     * Conflicts the key sequence @a seq with a current standard
     * shortcut?
     *
     * Pops up a dialog asking overriding the conflict is OK.
     */
    bool conflictWithStandardShortcuts(const QKeySequence &seq);

    /**
     * Conflicts the key sequence @a seq with a current local
     * shortcut?
     */
    bool conflictWithLocalShortcuts(const QKeySequence &seq);

    /**
     * Conflicts the key sequence @a seq conflict with Windows shortcut keys?
     */
    bool conflictWithGlobalShortcuts(const QKeySequence &seq);

    /**
     * Get permission to steal the shortcut @seq from the standard shortcut @a std.
     */
    bool stealStandardShortcut(KStandardShortcut::StandardShortcut std, const QKeySequence &seq);

    bool checkAgainstStandardShortcuts() const
    {
        return checkAgainstShortcutTypes & KisKKeySequenceWidget::StandardShortcuts;
    }

    bool checkAgainstGlobalShortcuts() const
    {
        return checkAgainstShortcutTypes & KisKKeySequenceWidget::GlobalShortcuts;
    }

    bool checkAgainstLocalShortcuts() const
    {
        return checkAgainstShortcutTypes & KisKKeySequenceWidget::LocalShortcuts;
    }

    void controlModifierlessTimeout()
    {
        if (nKey != 0 && !modifierKeys) {
            // No modifier key pressed currently. Start the timeout
            modifierlessTimeout.start(600);
        } else {
            // A modifier is pressed. Stop the timeout
            modifierlessTimeout.stop();
        }

    }

    void cancelRecording()
    {
        keySequence = oldKeySequence;
        doneRecording();
    }

//private slot
    void doneRecording(bool validate = true);

//members
    KisKKeySequenceWidget *const q;
    QHBoxLayout *layout;
    KKeySequenceButton *keyButton;
    QToolButton *clearButton;

    QKeySequence keySequence;
    QKeySequence oldKeySequence;
    QTimer modifierlessTimeout;
    bool allowModifierless;
    uint nKey;
    uint modifierKeys;
    bool isRecording;
    bool multiKeyShortcutsAllowed;
    QString componentName;

    //! Check the key sequence against KStandardShortcut::find()
    KisKKeySequenceWidget::ShortcutTypes checkAgainstShortcutTypes;

    /**
     * The list of action to check against for conflict shortcut
     */
    QList<QAction *> checkList; // deprecated

    /**
     * The list of action collections to check against for conflict shortcut
     */
    QList<KisKActionCollection *> checkActionCollections;

    /**
     * The action to steal the shortcut from.
     */
    QList<QAction *> stealActions;

    bool stealShortcuts(const QList<QAction *> &actions, const QKeySequence &seq);
    void wontStealShortcut(QAction *item, const QKeySequence &seq);

};

KisKKeySequenceWidgetPrivate::KisKKeySequenceWidgetPrivate(KisKKeySequenceWidget *q)
    : q(q)
    , layout(0)
    , keyButton(0)
    , clearButton(0)
    , allowModifierless(false)
    , nKey(0)
    , modifierKeys(0)
    , isRecording(false)
    , multiKeyShortcutsAllowed(true)
    , componentName()
    , checkAgainstShortcutTypes(KisKKeySequenceWidget::LocalShortcuts | KisKKeySequenceWidget::GlobalShortcuts)
    , stealActions()
{}

bool KisKKeySequenceWidgetPrivate::stealShortcuts(
    const QList<QAction *> &actions,
    const QKeySequence &seq)
{

    const int listSize = actions.size();

    QString title = i18ncp("%1 is the number of conflicts", "Shortcut Conflict", "Shortcut Conflicts", listSize);

    QString conflictingShortcuts;
    Q_FOREACH (const QAction *action, actions) {
        conflictingShortcuts += i18n("Shortcut '%1' for action '%2'\n",
                                     action->shortcut().toString(QKeySequence::NativeText),
                                     KLocalizedString::removeAcceleratorMarker(action->text()));
    }
    QString message = i18ncp("%1 is the number of ambiguous shortcut clashes (hidden)",
                             "The \"%2\" shortcut is ambiguous with the following shortcut.\n"
                             "Do you want to assign an empty shortcut to this action?\n"
                             "%3",
                             "The \"%2\" shortcut is ambiguous with the following shortcuts.\n"
                             "Do you want to assign an empty shortcut to these actions?\n"
                             "%3",
                             listSize,
                             seq.toString(QKeySequence::NativeText),
                             conflictingShortcuts);

    if (KMessageBox::warningContinueCancel(q, message, title, KGuiItem(i18n("Reassign"))) != KMessageBox::Continue) {
        return false;
    }

    return true;
}

void KisKKeySequenceWidgetPrivate::wontStealShortcut(QAction *item, const QKeySequence &seq)
{
    QString title(i18n("Shortcut conflict"));
    QString msg(i18n("<qt>The '%1' key combination is already used by the <b>%2</b> action.<br>"
                     "Please select a different one.</qt>", seq.toString(QKeySequence::NativeText),
                     KLocalizedString::removeAcceleratorMarker(item->text())));
    KMessageBox::error(q, msg, title);
}

KisKKeySequenceWidget::KisKKeySequenceWidget(QWidget *parent)
    : QWidget(parent),
      d(new KisKKeySequenceWidgetPrivate(this))
{
    d->init();
    setFocusProxy(d->keyButton);
    connect(d->keyButton, SIGNAL(clicked()), this, SLOT(captureKeySequence()));
    connect(d->clearButton, SIGNAL(clicked()), this, SLOT(clearKeySequence()));
    connect(&d->modifierlessTimeout, SIGNAL(timeout()), this, SLOT(doneRecording()));
    //TODO: how to adopt style changes at runtime?
    /*QFont modFont = d->clearButton->font();
    modFont.setStyleHint(QFont::TypeWriter);
    d->clearButton->setFont(modFont);*/
    d->updateShortcutDisplay();
}

void KisKKeySequenceWidgetPrivate::init()
{
    layout = new QHBoxLayout(q);
    layout->setContentsMargins(0, 0, 0, 0);

    keyButton = new KKeySequenceButton(this, q);
    keyButton->setFocusPolicy(Qt::StrongFocus);
    keyButton->setIcon(KisIconUtils::loadIcon(QStringLiteral("configure")));
    keyButton->setToolTip(i18n("Click on the button, then enter the shortcut like you would in the program.\nExample for Ctrl+A: hold the Ctrl key and press A."));
    layout->addWidget(keyButton);

    clearButton = new QToolButton(q);
    layout->addWidget(clearButton);

    if (qApp->isLeftToRight()) {
        clearButton->setIcon(KisIconUtils::loadIcon(QStringLiteral("edit-clear-locationbar-rtl")));
    } else {
        clearButton->setIcon(KisIconUtils::loadIcon(QStringLiteral("edit-clear-locationbar-ltr")));
    }
}

KisKKeySequenceWidget::~KisKKeySequenceWidget()
{
    delete d;
}

KisKKeySequenceWidget::ShortcutTypes KisKKeySequenceWidget::checkForConflictsAgainst() const
{
    return d->checkAgainstShortcutTypes;
}

void KisKKeySequenceWidget::setComponentName(const QString &componentName)
{
    d->componentName = componentName;
}

bool KisKKeySequenceWidget::multiKeyShortcutsAllowed() const
{
    return d->multiKeyShortcutsAllowed;
}

void KisKKeySequenceWidget::setMultiKeyShortcutsAllowed(bool allowed)
{
    d->multiKeyShortcutsAllowed = allowed;
}

void KisKKeySequenceWidget::setCheckForConflictsAgainst(ShortcutTypes types)
{
    d->checkAgainstShortcutTypes = types;
}

void KisKKeySequenceWidget::setModifierlessAllowed(bool allow)
{
    d->allowModifierless = allow;
}

bool KisKKeySequenceWidget::isKeySequenceAvailable(const QKeySequence &keySequence) const
{
    if (keySequence.isEmpty()) {
        // qDebug() << "Key sequence" << keySequence.toString() << "is empty and available.";
        return true;
    }

    bool hasConflict = (d->conflictWithLocalShortcuts(keySequence)
                        || d->conflictWithGlobalShortcuts(keySequence)
                        || d->conflictWithStandardShortcuts(keySequence));

    if (hasConflict) {
        /* qInfo() << "Key sequence" << keySequence.toString() << "has an unresolvable conflict." <<
            QString("Local conflict: %1. Windows conflict: %2.  Standard Shortcut conflict: %3") \
            .arg(d->conflictWithLocalShortcuts(keySequence))            \
            .arg(d->conflictWithGlobalShortcuts(keySequence))           \
            .arg(d->conflictWithStandardShortcuts(keySequence)); */
    }
    return !(hasConflict);

}

bool KisKKeySequenceWidget::isModifierlessAllowed()
{
    return d->allowModifierless;
}

void KisKKeySequenceWidget::setClearButtonShown(bool show)
{
    d->clearButton->setVisible(show);
}

void KisKKeySequenceWidget::setCheckActionCollections(const QList<KisKActionCollection *> &actionCollections)
{
    d->checkActionCollections = actionCollections;
}

//slot
void KisKKeySequenceWidget::captureKeySequence()
{
    d->startRecording();
}

QKeySequence KisKKeySequenceWidget::keySequence() const
{
    return d->keySequence;
}

//slot
void KisKKeySequenceWidget::setKeySequence(const QKeySequence &seq, Validation validate)
{
    // oldKeySequence holds the key sequence before recording started, if setKeySequence()
    // is called while not recording then set oldKeySequence to the existing sequence so
    // that the keySequenceChanged() signal is emitted if the new and previous key
    // sequences are different
    if (!d->isRecording) {
        d->oldKeySequence = d->keySequence;
    }

    d->keySequence = seq;
    d->doneRecording(validate == Validate);
}

//slot
void KisKKeySequenceWidget::clearKeySequence()
{
    setKeySequence(QKeySequence());
}

//slot
void KisKKeySequenceWidget::applyStealShortcut()
{
    QSet<KisKActionCollection *> changedCollections;

    Q_FOREACH (QAction *stealAction, d->stealActions) {

        // Stealing a shortcut means setting it to an empty one.
        stealAction->setShortcuts(QList<QKeySequence>());

        // The following code will find the action we are about to
        // steal from and save its action collection.
        KisKActionCollection *parentCollection = 0;
        foreach (KisKActionCollection *collection, d->checkActionCollections) {
            if (collection->actions().contains(stealAction)) {
                parentCollection = collection;
                break;
            }
        }

        // Remember the changed collection
        if (parentCollection) {
            changedCollections.insert(parentCollection);
        }
    }

    Q_FOREACH (KisKActionCollection *col, changedCollections) {
        col->writeSettings();
    }

    d->stealActions.clear();
}

void KisKKeySequenceWidgetPrivate::startRecording()
{
    nKey = 0;
    modifierKeys = 0;
    oldKeySequence = keySequence;
    keySequence = QKeySequence();
    isRecording = true;
    keyButton->grabKeyboard();

    if (!QWidget::keyboardGrabber()) {
        qWarning() << "Failed to grab the keyboard! Most likely qt's nograb option is active";
    }

    keyButton->setDown(true);
    updateShortcutDisplay();
}

void KisKKeySequenceWidgetPrivate::doneRecording(bool validate)
{
    modifierlessTimeout.stop();
    isRecording = false;
    keyButton->releaseKeyboard();
    keyButton->setDown(false);
    stealActions.clear();

    if (keySequence == oldKeySequence) {
        // The sequence hasn't changed
        updateShortcutDisplay();
        return;
    }

    if (validate && !q->isKeySequenceAvailable(keySequence)) {
        // The sequence had conflicts and the user said no to stealing it
        keySequence = oldKeySequence;
    } else {
        Q_EMIT q->keySequenceChanged(keySequence);
    }

    updateShortcutDisplay();
}

bool KisKKeySequenceWidgetPrivate::conflictWithGlobalShortcuts(const QKeySequence &keySequence)
{
    // This could hold some OS-specific stuff, or it could be linked back with
    // the KDE global shortcut code at some point in the future.

#ifdef Q_OS_WIN
#else
#endif
    Q_UNUSED(keySequence);

    return false;
}

bool shortcutsConflictWith(const QList<QKeySequence> &shortcuts, const QKeySequence &needle)
{
    if (needle.isEmpty() || needle.toString(QKeySequence::NativeText).isEmpty()) {
        return false;
    }

    foreach (const QKeySequence &sequence, shortcuts) {
        if (sequence.isEmpty()) {
            continue;
        }

        if (sequence.matches(needle) != QKeySequence::NoMatch
                || needle.matches(sequence) != QKeySequence::NoMatch) {
            return true;
        }
    }

    return false;
}

bool KisKKeySequenceWidgetPrivate::conflictWithLocalShortcuts(const QKeySequence &keySequence)
{
    if (!(checkAgainstShortcutTypes & KisKKeySequenceWidget::LocalShortcuts)) {
        return false;
    }

    // We have actions both in the deprecated checkList and the
    // checkActionCollections list. Add all the actions to a single list to
    // be able to process them in a single loop below.
    // Note that this can't be done in setCheckActionCollections(), because we
    // keep pointers to the action collections, and between the call to
    // setCheckActionCollections() and this function some actions might already be
    // removed from the collection again.
    QList<QAction *> allActions;
    allActions += checkList;
    foreach (KisKActionCollection *collection, checkActionCollections) {
        allActions += collection->actions();
    }

    // Because of multikey shortcuts we can have clashes with many shortcuts.
    //
    // Example 1:
    //
    // Application currently uses 'CTRL-X,a', 'CTRL-X,f' and 'CTRL-X,CTRL-F'
    // and the user wants to use 'CTRL-X'. 'CTRL-X' will only trigger as
    // 'activatedAmbiguously()' for obvious reasons.
    //
    // Example 2:
    //
    // Application currently uses 'CTRL-X'. User wants to use 'CTRL-X,CTRL-F'.
    // This will shadow 'CTRL-X' for the same reason as above.
    //
    // Example 3:
    //
    // Some weird combination of Example 1 and 2 with three shortcuts using
    // 1/2/3 key shortcuts. I think you can imagine.
    QList<QAction *> conflictingActions;

    //find conflicting shortcuts with existing actions
    foreach (QAction *qaction, allActions) {
        if (shortcutsConflictWith(qaction->shortcuts(), keySequence)) {
            // A conflict with a KAction. If that action is configurable
            // ask the user what to do. If not reject this keySequence.
            if (checkActionCollections.first()->isShortcutsConfigurable(qaction)) {
                conflictingActions.append(qaction);
            } else {
                wontStealShortcut(qaction, keySequence);
                return true;
            }
        }
    }

    if (conflictingActions.isEmpty()) {
        // No conflicting shortcuts found.
        return false;
    }

    if (stealShortcuts(conflictingActions, keySequence)) {
        stealActions = conflictingActions;

        // Announce that the user agreed to override the other shortcut
        Q_FOREACH (QAction *stealAction, stealActions) {
            Q_EMIT q->stealShortcut(
                keySequence,
                stealAction);
        }
        return false;
    } else {
        return true;
    }
}

bool KisKKeySequenceWidgetPrivate::conflictWithStandardShortcuts(const QKeySequence &keySequence)
{
    if (!(checkAgainstShortcutTypes & KisKKeySequenceWidget::StandardShortcuts)) {
        return false;
    }
    KStandardShortcut::StandardShortcut ssc = KStandardShortcut::find(keySequence);
    if (ssc != KStandardShortcut::AccelNone && !stealStandardShortcut(ssc, keySequence)) {
        return true;
    }
    return false;
}

bool KisKKeySequenceWidgetPrivate::stealStandardShortcut(KStandardShortcut::StandardShortcut std, const QKeySequence &seq)
{
    QString title = i18n("Conflict with Standard Application Shortcut");
    QString message = i18n("The '%1' key combination is also used for the standard action "
                           "\"%2\" that some applications use.\n"
                           "Do you really want to use it as a global shortcut as well?",
                           seq.toString(QKeySequence::NativeText), KStandardShortcut::label(std));

    if (KMessageBox::warningContinueCancel(q, message, title, KGuiItem(i18n("Reassign"))) != KMessageBox::Continue) {
        return false;
    }
    return true;
}

void KisKKeySequenceWidgetPrivate::updateShortcutDisplay()
{
    //empty string if no non-modifier was pressed
    QString s = keySequence.toString(QKeySequence::NativeText);
    s.replace(QLatin1Char('&'), QStringLiteral("&&"));

    if (isRecording) {
        if (modifierKeys) {
            if (!s.isEmpty()) {
                s.append(QLatin1Char(','));
            }
            if (modifierKeys & Qt::MetaModifier) {
                s += QKeySequence(Qt::MetaModifier).toString(QKeySequence::NativeText);
            }
#if defined(Q_OS_MAC)
            if (modifierKeys & Qt::AltModifier) {
                s += QKeySequence(Qt::AltModifier).toString(QKeySequence::NativeText);
            }
            if (modifierKeys & Qt::ControlModifier) {
                s += QKeySequence(Qt::ControlModifier).toString(QKeySequence::NativeText);
            }
#else
            if (modifierKeys & Qt::ControlModifier) {
                s += QKeySequence(Qt::ControlModifier).toString(QKeySequence::NativeText);
            }
            if (modifierKeys & Qt::AltModifier) {
                s += QKeySequence(Qt::AltModifier).toString(QKeySequence::NativeText);
            }
#endif
            if (modifierKeys & Qt::ShiftModifier) {
                s += QKeySequence(Qt::ShiftModifier).toString(QKeySequence::NativeText);
            }
            if (modifierKeys & Qt::KeypadModifier) {
                s += QKeySequence(Qt::KeypadModifier).toString(QKeySequence::NativeText);
            }

        } else if (nKey == 0) {
            s = i18nc("What the user inputs now will be taken as the new shortcut", "Input");
        }
        //make it clear that input is still going on
        s.append(QStringLiteral(" ..."));
    }

    if (s.isEmpty()) {
        s = i18nc("No shortcut defined", "None");
    }

    s.prepend(QLatin1Char(' '));
    s.append(QLatin1Char(' '));
    keyButton->setText(s);

}

KKeySequenceButton::~KKeySequenceButton()
{
}

//prevent Qt from special casing Tab and Backtab
bool KKeySequenceButton::event(QEvent *e)
{
    if (d->isRecording && e->type() == QEvent::KeyPress) {
        keyPressEvent(static_cast<QKeyEvent *>(e));
        return true;
    }

    // The shortcut 'alt+c' ( or any other dialog local action shortcut )
    // ended the recording and triggered the action associated with the
    // action. In case of 'alt+c' ending the dialog.  It seems that those
    // ShortcutOverride events get sent even if grabKeyboard() is active.
    if (d->isRecording && e->type() == QEvent::ShortcutOverride) {
        e->accept();
        return true;
    }

    if (d->isRecording && e->type() == QEvent::ContextMenu) {
        // is caused by Qt::Key_Menu
        e->accept();
        return true;
    }

    return QPushButton::event(e);
}

namespace {

// Copied here from KKeyServer
static bool isShiftAsModifierAllowed(int keyQt)
{
    // remove any modifiers
    keyQt &= ~Qt::KeyboardModifierMask;

    // Shift only works as a modifier with certain keys. It's not possible
    // to enter the SHIFT+5 key sequence for me because this is handled as
    // '%' by qt on my keyboard.
    // The working keys are all hardcoded here :-(
    if (keyQt >= Qt::Key_F1 && keyQt <= Qt::Key_F35) {
        return true;
    }

    if (QChar::isLetter(keyQt)) {
        return true;
    }

    switch (keyQt) {
    case Qt::Key_Return:
    case Qt::Key_Space:
    case Qt::Key_Backspace:
    case Qt::Key_Tab:
    case Qt::Key_Backtab:
    case Qt::Key_Escape:
    case Qt::Key_Print:
    case Qt::Key_ScrollLock:
    case Qt::Key_Pause:
    case Qt::Key_PageUp:
    case Qt::Key_PageDown:
    case Qt::Key_Insert:
    case Qt::Key_Delete:
    case Qt::Key_Home:
    case Qt::Key_End:
    case Qt::Key_Up:
    case Qt::Key_Down:
    case Qt::Key_Left:
    case Qt::Key_Right:
    case Qt::Key_Enter:
    case Qt::Key_SysReq:
    case Qt::Key_CapsLock:
    case Qt::Key_NumLock:
    case Qt::Key_Help:
    case Qt::Key_Back:
    case Qt::Key_Forward:
    case Qt::Key_Stop:
    case Qt::Key_Refresh:
    case Qt::Key_Favorites:
    case Qt::Key_LaunchMedia:
    case Qt::Key_OpenUrl:
    case Qt::Key_HomePage:
    case Qt::Key_Search:
    case Qt::Key_VolumeDown:
    case Qt::Key_VolumeMute:
    case Qt::Key_VolumeUp:
    case Qt::Key_BassBoost:
    case Qt::Key_BassUp:
    case Qt::Key_BassDown:
    case Qt::Key_TrebleUp:
    case Qt::Key_TrebleDown:
    case Qt::Key_MediaPlay:
    case Qt::Key_MediaStop:
    case Qt::Key_MediaPrevious:
    case Qt::Key_MediaNext:
    case Qt::Key_MediaRecord:
    case Qt::Key_MediaPause:
    case Qt::Key_MediaTogglePlayPause:
    case Qt::Key_LaunchMail:
    case Qt::Key_Calculator:
    case Qt::Key_Memo:
    case Qt::Key_ToDoList:
    case Qt::Key_Calendar:
    case Qt::Key_PowerDown:
    case Qt::Key_ContrastAdjust:
    case Qt::Key_Standby:
    case Qt::Key_MonBrightnessUp:
    case Qt::Key_MonBrightnessDown:
    case Qt::Key_KeyboardLightOnOff:
    case Qt::Key_KeyboardBrightnessUp:
    case Qt::Key_KeyboardBrightnessDown:
    case Qt::Key_PowerOff:
    case Qt::Key_WakeUp:
    case Qt::Key_Eject:
    case Qt::Key_ScreenSaver:
    case Qt::Key_WWW:
    case Qt::Key_Sleep:
    case Qt::Key_LightBulb:
    case Qt::Key_Shop:
    case Qt::Key_History:
    case Qt::Key_AddFavorite:
    case Qt::Key_HotLinks:
    case Qt::Key_BrightnessAdjust:
    case Qt::Key_Finance:
    case Qt::Key_Community:
    case Qt::Key_AudioRewind:
    case Qt::Key_BackForward:
    case Qt::Key_ApplicationLeft:
    case Qt::Key_ApplicationRight:
    case Qt::Key_Book:
    case Qt::Key_CD:
    case Qt::Key_Clear:
    case Qt::Key_ClearGrab:
    case Qt::Key_Close:
    case Qt::Key_Copy:
    case Qt::Key_Cut:
    case Qt::Key_Display:
    case Qt::Key_DOS:
    case Qt::Key_Documents:
    case Qt::Key_Excel:
    case Qt::Key_Explorer:
    case Qt::Key_Game:
    case Qt::Key_Go:
    case Qt::Key_iTouch:
    case Qt::Key_LogOff:
    case Qt::Key_Market:
    case Qt::Key_Meeting:
    case Qt::Key_MenuKB:
    case Qt::Key_MenuPB:
    case Qt::Key_MySites:
    case Qt::Key_News:
    case Qt::Key_OfficeHome:
    case Qt::Key_Option:
    case Qt::Key_Paste:
    case Qt::Key_Phone:
    case Qt::Key_Reply:
    case Qt::Key_Reload:
    case Qt::Key_RotateWindows:
    case Qt::Key_RotationPB:
    case Qt::Key_RotationKB:
    case Qt::Key_Save:
    case Qt::Key_Send:
    case Qt::Key_Spell:
    case Qt::Key_SplitScreen:
    case Qt::Key_Support:
    case Qt::Key_TaskPane:
    case Qt::Key_Terminal:
    case Qt::Key_Tools:
    case Qt::Key_Travel:
    case Qt::Key_Video:
    case Qt::Key_Word:
    case Qt::Key_Xfer:
    case Qt::Key_ZoomIn:
    case Qt::Key_ZoomOut:
    case Qt::Key_Away:
    case Qt::Key_Messenger:
    case Qt::Key_WebCam:
    case Qt::Key_MailForward:
    case Qt::Key_Pictures:
    case Qt::Key_Music:
    case Qt::Key_Battery:
    case Qt::Key_Bluetooth:
    case Qt::Key_WLAN:
    case Qt::Key_UWB:
    case Qt::Key_AudioForward:
    case Qt::Key_AudioRepeat:
    case Qt::Key_AudioRandomPlay:
    case Qt::Key_Subtitle:
    case Qt::Key_AudioCycleTrack:
    case Qt::Key_Time:
    case Qt::Key_Select:
    case Qt::Key_View:
    case Qt::Key_TopMenu:
    case Qt::Key_Suspend:
    case Qt::Key_Hibernate:
    case Qt::Key_Launch0:
    case Qt::Key_Launch1:
    case Qt::Key_Launch2:
    case Qt::Key_Launch3:
    case Qt::Key_Launch4:
    case Qt::Key_Launch5:
    case Qt::Key_Launch6:
    case Qt::Key_Launch7:
    case Qt::Key_Launch8:
    case Qt::Key_Launch9:
    case Qt::Key_LaunchA:
    case Qt::Key_LaunchB:
    case Qt::Key_LaunchC:
    case Qt::Key_LaunchD:
    case Qt::Key_LaunchE:
    case Qt::Key_LaunchF:
    case Qt::Key_Shift:
    case Qt::Key_Control:
    case Qt::Key_Meta:
    case Qt::Key_Alt:
    case Qt::Key_Super_L:
    case Qt::Key_Super_R:
        return true;

    default:
        return false;
    }
}

} // namespace

void KKeySequenceButton::keyPressEvent(QKeyEvent *e)
{
    int keyQt = e->key();
    if (keyQt == -1) {
        // Qt sometimes returns garbage keycodes, I observed -1, if it doesn't know a key.
        // We cannot do anything useful with those (several keys have -1, indistinguishable)
        // and QKeySequence.toString() will also yield a garbage string.
        KMessageBox::error(this,
                           i18n("The key you just pressed is not supported by Qt."),
                           i18n("Unsupported Key"));
        return d->cancelRecording();
    }

    uint newModifiers = e->modifiers() & (Qt::SHIFT | Qt::CTRL | Qt::ALT | Qt::META);

    //don't have the return or space key appear as first key of the sequence when they
    //were pressed to start editing - catch and them and imitate their effect
    if (!d->isRecording && ((keyQt == Qt::Key_Return || keyQt == Qt::Key_Space))) {
        d->startRecording();
        d->modifierKeys = newModifiers;
        d->updateShortcutDisplay();
        return;
    }

    // We get events even if recording isn't active.
    if (!d->isRecording) {
        return QPushButton::keyPressEvent(e);
    }

    e->accept();
    d->modifierKeys = newModifiers;

    switch (keyQt) {
    case Qt::Key_AltGr: //or else we get unicode salad
        return;
    case Qt::Key_Shift:
    case Qt::Key_Control:
    case Qt::Key_Alt:
    case Qt::Key_Meta:
    case Qt::Key_Super_L:
    case Qt::Key_Super_R:
        d->controlModifierlessTimeout();
        d->updateShortcutDisplay();
        break;
    default:

        if (d->nKey == 0 && !(d->modifierKeys & ~Qt::SHIFT)) {
            // It's the first key and no modifier pressed. Check if this is
            // allowed
            if (!(KisKKeySequenceWidgetPrivate::isOkWhenModifierless(keyQt)
                    || d->allowModifierless)) {
                // No it's not
                return;
            }
        }

        // We now have a valid key press.
        if (keyQt) {
            /**
             * Here is the trap, which is different on every OS. On Widnows Qt
             * has incomprehensible rules on translating AltGr-related key
             * sequences into shortcuts. On macOS symbols may confuse Qt when
             * using Cmd+Shift-based shortcuts. * So we should just ask Qt
             * itself what it expects to receive as a shortcut :)
             *
             * That is exactly what Qt's QKeySequenceEdit does internally.
             *
             * TODO: in the future replace the whole widget with QKeySequenceEdit,
             *       it uses QKeyMapper directly.
             */
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
            const QList<int> vec = QKeyMapper::possibleKeys(e);
#else
            const QList<QKeyCombination> vec = QKeyMapper::possibleKeys(e);
#endif

            if (!vec.isEmpty() && e->modifiers() != Qt::NoModifier) {
                /**
                 * Take the first element, which is usually the shortcut form the latin
                 * layout of the keyboard
                 */
                keyQt = vec.first();
            } else if ((keyQt == Qt::Key_Backtab) && (d->modifierKeys & Qt::SHIFT)) {
                keyQt = Qt::Key_Tab | d->modifierKeys;
            } else if (isShiftAsModifierAllowed(keyQt)) {
                keyQt |= d->modifierKeys;
            } else {
                keyQt |= (d->modifierKeys & ~Qt::SHIFT);
            }

            if (d->nKey == 0) {
                d->keySequence = QKeySequence(keyQt);
            } else {
                d->keySequence =
                    KisKKeySequenceWidgetPrivate::appendToSequence(d->keySequence, keyQt);
            }

            d->nKey++;
            if ((!d->multiKeyShortcutsAllowed) || (d->nKey >= 4)) {
                d->doneRecording();
                return;
            }
            d->controlModifierlessTimeout();
            d->updateShortcutDisplay();
        }
    }
}

void KKeySequenceButton::keyReleaseEvent(QKeyEvent *e)
{
    if (e->key() == -1) {
        // ignore garbage, see keyPressEvent()
        return;
    }

    if (!d->isRecording) {
        return QPushButton::keyReleaseEvent(e);
    }

    e->accept();

    uint newModifiers = e->modifiers() & (Qt::SHIFT | Qt::CTRL | Qt::ALT | Qt::META);

    //if a modifier that belongs to the shortcut was released...
    if ((newModifiers & d->modifierKeys) < d->modifierKeys) {
        d->modifierKeys = newModifiers;
        d->controlModifierlessTimeout();
        d->updateShortcutDisplay();
    }
}

//static
QKeySequence KisKKeySequenceWidgetPrivate::appendToSequence(const QKeySequence &seq, int keyQt)
{
    switch (seq.count()) {
    case 0:
        return QKeySequence(keyQt);
    case 1:
        return QKeySequence(seq[0], keyQt);
    case 2:
        return QKeySequence(seq[0], seq[1], keyQt);
    case 3:
        return QKeySequence(seq[0], seq[1], seq[2], keyQt);
    default:
        return seq;
    }
}

//static
bool KisKKeySequenceWidgetPrivate::isOkWhenModifierless(int keyQt)
{
    //this whole function is a hack, but especially the first line of code
    if (QKeySequence(keyQt).toString().length() == 1) {
        return false;
    }

    switch (keyQt) {
    case Qt::Key_Return:
    case Qt::Key_Space:
    case Qt::Key_Tab:
    case Qt::Key_Backtab: //does this ever happen?
    case Qt::Key_Backspace:
    case Qt::Key_Delete:
        return false;
    default:
        return true;
    }
}

#include "moc_kkeysequencewidget.cpp"
#include "moc_kkeysequencewidget_p.cpp"
