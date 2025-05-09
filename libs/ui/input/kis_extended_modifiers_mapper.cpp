/*
 *  SPDX-FileCopyrightText: 2013 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_extended_modifiers_mapper.h"

#include <QApplication>
#include <QKeyEvent>

#ifdef Q_OS_MACOS

#include "kis_extended_modifiers_mapper_osx.h"

#endif /* Q_OS_MACOS */

#ifdef Q_OS_WIN

#include <windows.h>
#include <commctrl.h>
#include <winuser.h>

#include "krita_container_utils.h"


QVector<Qt::Key> queryPressedKeysWin()
{
    QVector<Qt::Key> result;
    BYTE vkeys[256];

    if (GetKeyboardState(vkeys)) {
        for (int i = 0; i < 256; i++) {
            if (vkeys[i] & 0x80) {
                if (i == VK_SHIFT) {
                    result << Qt::Key_Shift;
                } else if (i == VK_CONTROL) {
                    result << Qt::Key_Control;
                } else if (i == VK_MENU) {
                    result << Qt::Key_Alt;
                } else if (i == VK_LWIN || i == VK_RWIN) {
                    result << Qt::Key_Meta;
                } else if (i == VK_SPACE) {
                    result << Qt::Key_Space;
                } else if (i >= 0x30 && i <= 0x39) {
                    result << static_cast<Qt::Key>(Qt::Key_0 + i - 0x30);
                } else if (i >= 0x41 && i <= 0x5A) {
                    result << static_cast<Qt::Key>(Qt::Key_A + i - 0x41);
                } else if (i >= 0x60 && i <= 0x69) {
                    result << static_cast<Qt::Key>(Qt::Key_0 + i - 0x60);
                } else if (i >= 0x70 && i <= 0x87) {
                    result << static_cast<Qt::Key>(Qt::Key_F1 + i - 0x70);
                }
            }
        }
    }

    KritaUtils::makeContainerUnique(result);

    return result;
}

#elif defined HAVE_X11
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
#include <QX11Info>
#else
#include <QGuiApplication>
#endif

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <krita_container_utils.h>
#include <X11/XKBlib.h>

struct KeyMapping {
    KeyMapping() {}
    KeyMapping(KeySym sym, Qt::Key key) : x11KeySym(sym), qtKey(key) {}
    KeySym x11KeySym {0};
    Qt::Key qtKey {Qt::Key_unknown};
};

#endif /* HAVE_X11 */

struct KisExtendedModifiersMapper::Private
{
    Private();

#ifdef HAVE_X11

    QVector<KeyMapping> mapping;
    char keysState[32];
    int minKeyCode = 0;
    int maxKeyCode = 0;

    bool checkKeyCodePressedX11(KeyCode key);
#endif /* HAVE_X11 */
};

#ifdef HAVE_X11

KisExtendedModifiersMapper::Private::Private()
{
    if (QGuiApplication::platformName() == QLatin1String("xcb")) {
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
        XDisplayKeycodes(QX11Info::display(), &minKeyCode, &maxKeyCode);
        XQueryKeymap(QX11Info::display(), keysState);
#else
        XDisplayKeycodes(qGuiApp->nativeInterface<QNativeInterface::QX11Application>()->display(),
                     &minKeyCode, &maxKeyCode);
        XQueryKeymap(qGuiApp->nativeInterface<QNativeInterface::QX11Application>()->display(), keysState);
#endif

        mapping.append(KeyMapping(XK_Shift_L, Qt::Key_Shift));
        mapping.append(KeyMapping(XK_Shift_R, Qt::Key_Shift));

        mapping.append(KeyMapping(XK_Control_L, Qt::Key_Control));
        mapping.append(KeyMapping(XK_Control_R, Qt::Key_Control));

        mapping.append(KeyMapping(XK_Meta_L, Qt::Key_Alt));
        mapping.append(KeyMapping(XK_Meta_R, Qt::Key_Alt));
        mapping.append(KeyMapping(XK_Mode_switch, Qt::Key_AltGr));
        mapping.append(KeyMapping(XK_ISO_Level3_Shift, Qt::Key_AltGr));

        mapping.append(KeyMapping(XK_Alt_L, Qt::Key_Alt));
        mapping.append(KeyMapping(XK_Alt_R, Qt::Key_Alt));

        mapping.append(KeyMapping(XK_Super_L, Qt::Key_Meta));
        mapping.append(KeyMapping(XK_Super_R, Qt::Key_Meta));

        mapping.append(KeyMapping(XK_Hyper_L, Qt::Key_Hyper_L));
        mapping.append(KeyMapping(XK_Hyper_R, Qt::Key_Hyper_R));


        mapping.append(KeyMapping(XK_space, Qt::Key_Space));

        for (int qtKey = Qt::Key_0, x11Sym = XK_0;
            qtKey <= Qt::Key_9;
            qtKey++, x11Sym++) {

            mapping.append(KeyMapping(x11Sym, Qt::Key(qtKey)));
        }

        for (int qtKey = Qt::Key_A, x11Sym = XK_a;
            qtKey <= Qt::Key_Z;
            qtKey++, x11Sym++) {

            mapping.append(KeyMapping(x11Sym, Qt::Key(qtKey)));
        }
    }
}

bool KisExtendedModifiersMapper::Private::checkKeyCodePressedX11(KeyCode key)
{
    int byte = key / 8;
    char mask = 1 << (key % 8);

    return keysState[byte] & mask;
}

#else /* HAVE_X11 */

KisExtendedModifiersMapper::Private::Private()
{
}

#endif /* HAVE_X11 */


KisExtendedModifiersMapper::KisExtendedModifiersMapper()
    : m_d(new Private)
{
}

KisExtendedModifiersMapper::~KisExtendedModifiersMapper()
{
}

#ifdef Q_OS_MACOS
void KisExtendedModifiersMapper::setLocalMonitor(bool activate, KisShortcutMatcher *matcher)
{
    Q_UNUSED(matcher);
    activateLocalMonitor(activate);
}
#endif

KisExtendedModifiersMapper::ExtendedModifiers
KisExtendedModifiersMapper::queryExtendedModifiers()
{
    ExtendedModifiers modifiers;

#ifdef HAVE_X11

    if (QGuiApplication::platformName() == QLatin1String("xcb")) {
        for (int keyCode = m_d->minKeyCode; keyCode <= m_d->maxKeyCode; keyCode++) {
            if (m_d->checkKeyCodePressedX11(keyCode)) {
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
                KeySym sym = XkbKeycodeToKeysym(QX11Info::display(), keyCode,
#else
                KeySym sym = XkbKeycodeToKeysym(qGuiApp->nativeInterface<QNativeInterface::QX11Application>()->display(), keyCode,
#endif
                                                0, 0);
                Q_FOREACH (const KeyMapping &map, m_d->mapping) {
                    if (map.x11KeySym == sym) {
                        modifiers << map.qtKey;
                        break;
                    }
                }
            }
        }
    }

    // in X11 some keys may have multiple keysyms,
    // (Alt Key == XK_Meta_{L,R}, XK_Meta_{L,R})
    KritaUtils::makeContainerUnique(modifiers);

#elif defined Q_OS_WIN

    modifiers = queryPressedKeysWin();

#elif defined Q_OS_MACOS
    modifiers = queryPressedKeysMac();
#else

    Qt::KeyboardModifiers standardModifiers = queryStandardModifiers();

    if (standardModifiers & Qt::ShiftModifier) {
        modifiers << Qt::Key_Shift;
    }

    if (standardModifiers & Qt::ControlModifier) {
        modifiers << Qt::Key_Control;
    }

    if (standardModifiers & Qt::AltModifier) {
        modifiers << Qt::Key_Alt;
    }

    if (standardModifiers & Qt::MetaModifier) {
        modifiers << Qt::Key_Meta;
    }

#endif

    return modifiers;
}

Qt::KeyboardModifiers KisExtendedModifiersMapper::queryStandardModifiers()
{
    return QApplication::queryKeyboardModifiers();
}

Qt::Key KisExtendedModifiersMapper::workaroundShiftAltMetaHell(const QKeyEvent *keyEvent)
{
    Qt::Key key = (Qt::Key)keyEvent->key();

    if (keyEvent->key() == Qt::Key_Meta &&
        keyEvent->modifiers().testFlag(Qt::ShiftModifier)) {

        key = Qt::Key_Alt;
    }

    return key;
}
