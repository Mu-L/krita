/*
 *  SPDX-FileCopyrightText: 2011 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_scratch_pad_event_filter.h"
#include "kis_scratch_pad.h"

#include <QWidget>
#include <QDebug>
#include <QEvent>
#include <QMouseEvent>
#include <QTabletEvent>

KisScratchPadEventFilter::KisScratchPadEventFilter(QWidget *parent)
    : QObject(parent),
      m_tabletPressed(false),
      m_pressedButton(Qt::NoButton)
{
    parent->installEventFilter(this);
    m_scratchPad = qobject_cast<KisScratchPad *>(parent);
}

void KisScratchPadEventFilter::setWidgetToDocumentTransform(const QTransform &transform)
{
    m_widgetToDocument = transform;
}

QWidget* KisScratchPadEventFilter::parentWidget()
{
    return static_cast<QWidget*>(parent());
}

KoPointerEvent* KisScratchPadEventFilter::createMouseEvent(QEvent *event)
{
    QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
    return new KoPointerEvent(mouseEvent, m_widgetToDocument.map(mouseEvent->pos()));
}

KoPointerEvent* KisScratchPadEventFilter::createTabletEvent(QEvent *event)
{
    QTabletEvent *tabletEvent = static_cast<QTabletEvent*>(event);
    const QPointF pos = tabletEvent->posF();
    return new KoPointerEvent(tabletEvent, m_widgetToDocument.map(pos));
}


bool KisScratchPadEventFilter::eventFilter(QObject *obj, QEvent *event)
{
    Q_UNUSED(obj);
    bool result = false;

    QScopedPointer<KoPointerEvent> ev;

    if (shouldResetWheelDelta(event)) {
        m_scratchPad->resetWheelDelta();
    }

    switch(event->type()) {
    case QEvent::MouseButtonPress:
        if (m_pressedButton != Qt::NoButton) break;

        ev.reset(createMouseEvent(event));
        m_pressedButton = ev->button();
        m_scratchPad->pointerPress(ev.data());
        result = true;
        break;
    case QEvent::MouseButtonRelease:
        ev.reset(createMouseEvent(event));
        if (ev->button() != m_pressedButton) break;

        m_pressedButton = Qt::NoButton;
        m_scratchPad->pointerRelease(ev.data());
        result = true;
        break;
    case QEvent::MouseMove:
        if(m_tabletPressed) break;
        ev.reset(createMouseEvent(event));
        m_scratchPad->pointerMove(ev.data());
        result = true;
        break;
    case QEvent::TabletPress:
        if (m_pressedButton != Qt::NoButton) break;

        m_tabletPressed = true;
        ev.reset(createTabletEvent(event));
        m_pressedButton = ev->button();
        m_scratchPad->pointerPress(ev.data());
        result = true;
        break;
    case QEvent::TabletRelease:
        ev.reset(createTabletEvent(event));
        if (ev->button() != m_pressedButton) break;

        m_pressedButton = Qt::NoButton;
        m_scratchPad->pointerRelease(ev.data());
        m_tabletPressed = false;
        result = true;
        break;
    case QEvent::TabletMove:
        ev.reset(createTabletEvent(event));
        m_scratchPad->pointerMove(ev.data());
        result = true;
        break;
    case QEvent::FocusIn:
    case QEvent::FocusOut:
    case QEvent::Show:
    case QEvent::Hide:
        m_scratchPad->resetState();
        m_tabletPressed = false;
        m_pressedButton = Qt::NoButton;
        break;
    case QEvent::Wheel:
        if (m_pressedButton != Qt::NoButton) break;

        m_pressedButton = Qt::NoButton;
        m_scratchPad->wheelDelta(static_cast<QWheelEvent*>(event));
        result = true;
        break;
    default:
        result = false;
    }

    if (ev) {
        result = ev->isAccepted();
        event->setAccepted(result);
    }

    return result;
}

bool KisScratchPadEventFilter::shouldResetWheelDelta(QEvent * event)
{
    return
        event->type() == QEvent::FocusIn ||
        event->type() == QEvent::FocusOut ||
        event->type() == QEvent::MouseButtonPress ||
        event->type() == QEvent::MouseButtonRelease ||
        event->type() == QEvent::MouseButtonDblClick ||
        event->type() == QEvent::TabletPress ||
        event->type() == QEvent::TabletRelease ||
        event->type() == QEvent::Enter ||
        event->type() == QEvent::Leave ||
        event->type() == QEvent::TouchBegin ||
        event->type() == QEvent::TouchEnd ||
        event->type() == QEvent::TouchCancel ||
        event->type() == QEvent::NativeGesture;
}

