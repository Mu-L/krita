/*
 *  SPDX-FileCopyrightText: 2012 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_SHORTCUT_MATCHER_H
#define __KIS_SHORTCUT_MATCHER_H


#include "kis_single_action_shortcut.h"
#include "KisInputActionGroup.h"
#include <functional>
#include "kis_shortcut_configuration.h"

class QEvent;
class QWheelEvent;
class QTouchEvent;
class QNativeGestureEvent;
class QString;
class QPointF;

class KisStrokeShortcut;
class KisTouchShortcut;
class KisNativeGestureShortcut;


/**
 * The class that manages connections between shortcuts and actions.
 *
 * It processes input events and generates state transitions for the
 * actions basing on the data, represented by the shortcuts.
 *
 * The class works with two types of actions: long running
 * (represented by KisStrokeShortcuts) and "atomic"
 * (KisSingleActionShortcut). The former one involves some long
 * interaction with the user by means of a mouse cursor or a tablet,
 * the latter one simple action like "Zoom 100%" or "Reset Rotation".
 *
 * The single action shortcuts are handled quite easily. The matcher
 * listens to the events coming, manages two lists of the pressed keys
 * and buttons and when their content corresponds to some single
 * action shortcut it just runs this shortcut once.
 *
 * The strategy for handling the stroke shortcuts is a bit more
 * complex.  Each such action may be in one of the three states:
 *
 * Idle <-> Ready <-> Running
 *
 * In "Idle" state the action is completely inactive and has no access
 * to the user
 *
 * When the action is in "Ready" state, it means that all the
 * modifiers for the action are already pressed and we are only
 * waiting for a user to press the mouse button and start a stroke. In
 * this state the action can show the user its Cursor to notify the user
 * what is going to happen next.
 *
 * In the "Running" state, the action has full access to the user
 * input and is considered to perform all the work it was created for.
 *
 * To implement such state transitions for the actions,
 * KisShortcutMatcher first forms a list of the actions which can be
 * moved to a ready state (m_d->readyShortcuts), then chooses the one
 * with the highest priority to be the only shortcut in the "Ready"
 * state and activates it (m_d->readyShortcut).  Then when the user
 * presses the mouse button, the matcher looks through the list of
 * ready shortcuts, chooses which will be running now, deactivates (if
 * needed) currently activated action and starts the chosen one.
 *
 * \see KisSingleActionShortcut
 * \see KisStrokeShortcut
 */
class KRITAUI_EXPORT KisShortcutMatcher
{
public:
    KisShortcutMatcher();
    ~KisShortcutMatcher();

    bool hasRunningShortcut() const;

    void addShortcut(KisSingleActionShortcut *shortcut);
    void addShortcut(KisStrokeShortcut *shortcut);
    void addShortcut(KisTouchShortcut *shortcut);
    void addShortcut(KisNativeGestureShortcut *shortcut);

    /**
     * Returns true if the currently running shortcut supports
     * processing hi resolution flow of events from the tablet
     * device. In most of the cases (except of the painting itself)
     * too many events make the execution of the action too slow, so
     * the action can decide whether it needs it.
     */
    bool supportsHiResInputEvents();

    /**
     * Handles a key press event.
     * No autorepeat events should be passed to this method.
     *
     * \return whether the event has been handled successfully and
     * should be eaten by the events filter
     */
    bool keyPressed(Qt::Key key);

    /**
     * Handles a key press event that has been generated by the
     * autorepeat.
     *
     * \return whether the event has been handled successfully and
     * should be eaten by the events filter
     */
    bool autoRepeatedKeyPressed(Qt::Key key);

    /**
     * Handles a key release event.
     * No autorepeat events should be passed to this method.
     *
     * \return whether the event has been handled successfully and
     * should be eaten by the events filter
     */
    bool keyReleased(Qt::Key key);

    /**
     * Handles button presses from a tablet or mouse.
     *
     * \param event the event that caused this call.
     * Must be of type QTabletEvent or QMouseEvent.
     *
     * \return whether the event has been handled successfully and
     * should be eaten by the events filter
     */
    bool buttonPressed(Qt::MouseButton button, QEvent *event);

    /**
     * Handles the mouse button release event
     *
     * \param event the event that caused this call.
     * Must be of type QTabletEvent or QMouseEvent.
     *
     * \return whether the event has been handled successfully and
     * should be eaten by the events filter
     */
    bool buttonReleased(Qt::MouseButton button, QEvent *event);

    /**
     * Handles the mouse wheel event
     *
     * \return whether the event has been handled successfully and
     * should be eaten by the events filter
     */
    bool wheelEvent(KisSingleActionShortcut::WheelAction wheelAction, QWheelEvent *event);

    /**
     * Handles tablet and mouse move events.
     *
     * \param event the event that caused this call
     *
     * \return whether the event has been handled successfully and
     * should be eaten by the events filter
     */
    bool pointerMoved(QEvent *event);

    /**
     * Handle cursor's Enter event.
     * We never eat it because it might be used by someone else
     */
    void enterEvent();

    /**
     * Handle cursor's Leave event.
     * We never eat it because it might be used by someone else
     */
    void leaveEvent();

    bool touchBeginEvent(QTouchEvent *event);
    bool touchUpdateEvent(QTouchEvent *event);
    bool touchEndEvent(QTouchEvent *event);

    /**
     * We received TouchCancel event, it means this event sequence has ended
     * right here i.e without a valid TouchEnd, so we should immediately stop
     * all running actions.
     */
    void touchCancelEvent(QTouchEvent *event, const QPointF &localPos);

    bool nativeGestureBeginEvent(QNativeGestureEvent *event);
    bool nativeGestureEvent(QNativeGestureEvent *event);
    bool nativeGestureEndEvent(QNativeGestureEvent *event);

    /**
     * Resets the internal state of the matcher and activates the
     * prepared action if possible.
     *
     * This should be done when the window has lost the focus for
     * some time, so that several events could be lost
     */
    void reinitialize();

    /**
     * Resets the internal state of the buttons inside matcher and
     * activates the prepared action if possible.
     *
     * This should be done when the window has lost the focus for
     * some time, so that several events could be lost
     */
    void reinitializeButtons();

    /**
     * Resets the internal state of the matcher, tries to resync it to the state
     * passed via argument and activates the prepared action if possible.
     *
     * This synchronization happens when the user hovers Krita windows,
     * **without** having keyboard focus set to it (therefore matcher cannot
     * get key press and release events), and is also used for various other fixes.
     */
    void handlePolledKeys(const QVector<Qt::Key> &keys);

    /**
     * Sanity check correctness of the internal state of the matcher
     * by comparing it to the standard modifiers that we get with
     * every input event. Right now this sanity check is used on Windows
     * only.
     */
    bool sanityCheckModifiersCorrectness(Qt::KeyboardModifiers modifiers) const;

    /**
     * Return the internal state of the tracked modifiers.
     */
    QVector<Qt::Key> debugPressedKeys() const;

    /**
     * Check if polled keys are present, which signals that we need to call KisInputManager::Private::fixShortcutMatcherModifiersState.
     */
    bool hasPolledKeys();

    /**
     * Krita lost focus, it means that all the running actions should be ended
     * forcefully.
     */
    void lostFocusEvent(const QPointF &localPos);

    /**
     * Is called when a new tool has been activated. The method activates
     * any tool's action if possible with the currently active modifiers.
     */
    void toolHasBeenActivated();

    /**
     * Disables the start of any actions.
     *
     * WARNING: the actions that has been started before this call
     * will *not* be ended. They will be ended in their usual way,
     * when the mouse button will be released.
     */
    void suppressAllActions(bool value);

    /**
     * Disable one-time actions whose shortcuts conflict with the listed shortcuts
     */
    void suppressConflictingKeyActions(const QVector<QKeySequence> &shortcuts);

    /**
     * Disable keyboard actions.
     *
     * This will disable all actions that consist of only
     * keyboard keys being pressed without mouse or stylus
     * buttons being pressed.
     *
     * This is turned on when the tool is in text mode.
     */
    void suppressAllKeyboardActions(bool value);

    /**
     * Remove all shortcuts that have been registered.
     */
    void clearShortcuts();

    void setInputActionGroupsMaskCallback(std::function<KisInputActionGroupsMask()> func);

private:
    friend class KisInputManagerTest;

    void reset();
    void reset(QString msg);

    bool tryRunWheelShortcut(KisSingleActionShortcut::WheelAction wheelAction, QWheelEvent *event);
    template<typename T, typename U> bool tryRunSingleActionShortcutImpl(T param, U *event, const QSet<Qt::Key> &keysState, bool keyboard = true);

    void prepareReadyShortcuts();

    bool tryRunReadyShortcut( Qt::MouseButton button, QEvent* event );
    void tryActivateReadyShortcut();
    bool tryEndRunningShortcut( Qt::MouseButton button, QEvent* event );
    void forceEndRunningShortcut(const QPointF &localPos);
    void forceDeactivateAllActions();

    void setMaxTouchPointEvent(QTouchEvent *event);
    void fireReadyTouchShortcut(QTouchEvent *event);
    KisTouchShortcut *matchTouchShortcut(QTouchEvent *event);
    bool tryRunTouchShortcut(QTouchEvent *event);
    bool tryEndTouchShortcut(QTouchEvent *event);

    bool tryRunNativeGestureShortcut(QNativeGestureEvent *event);
    bool tryEndNativeGestureShortcut(QNativeGestureEvent *event);

private:
    class Private;
    Private * const m_d;
};

#endif /* __KIS_SHORTCUT_MATCHER_H */
