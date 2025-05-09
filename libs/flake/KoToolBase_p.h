/*
 * SPDX-FileCopyrightText: 2006-2010 Thomas Zander <zander@kde.org>
 * SPDX-FileCopyrightText: 2010 Halla Rempt <halla@valdyas.org>
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef KOTOOLBASE_P_H
#define KOTOOLBASE_P_H

#include "KoDocumentResourceManager.h"
#include "KoCanvasResourceProvider.h"
#include "KoCanvasBase.h"
#include "KoShapeController.h"
#include <QHash>
#include <QWidget>
#include <QPointer>
#include <string.h> // for the qt version check

class QAction;
class KoToolBase;
class KoToolFactoryBase;

class KoToolBasePrivate
{
public:
    KoToolBasePrivate(KoToolBase *qq, KoCanvasBase *canvas_)
        : currentCursor(Qt::ArrowCursor),
        q(qq),
        canvas(canvas_),
        isInTextMode(false),
        isActivated(false)
    {
    }

    virtual ~KoToolBasePrivate()
    {
        Q_FOREACH (QPointer<QWidget> optionWidget, optionWidgets) {
            if (optionWidget) {
                optionWidget->setParent(0);
                delete optionWidget;
            }
        }
        optionWidgets.clear();
    }

    void connectSignals()
    {
        if (canvas) { // in the case of KoToolManagers dummy tool it can be zero :(
            KoCanvasResourceProvider * crp = canvas->resourceManager();
            Q_ASSERT_X(crp, "KoToolBase::KoToolBase", "No Canvas KoResourceManager");
            if (crp)
                q->connect(crp, SIGNAL(canvasResourceChanged(int, const QVariant &)),
                        SLOT(canvasResourceChanged(int, const QVariant &)));

            KoDocumentResourceManager *scrm = canvas->shapeController()->resourceManager();
            if (scrm) {
                q->connect(scrm, SIGNAL(resourceChanged(int, const QVariant &)),
                        SLOT(documentResourceChanged(int, const QVariant &)));
            }
        }
    }

    struct ToolCanvasResources {
        QHash<int, KoAbstractCanvasResourceInterfaceSP> abstractResources;
        QHash<int, KoDerivedResourceConverterSP> converters;
    };

    QList<QPointer<QWidget> > optionWidgets; ///< the optionwidgets associated with this tool
    bool optionWidgetsCreated {false};
    QCursor currentCursor;
    KoToolBase *q;
    KoToolFactoryBase *factory {0};
    KoCanvasBase *canvas; ///< the canvas interface this tool will work for.
    bool isInTextMode;
    bool maskSyntheticEvents{false}; ///< Whether this tool masks synthetic events
    bool isActivated;
    QRectF lastDecorationsRect;
    bool isOpacityPresetMode{false}; ///< Whether the opacity is preset or tool
    ToolCanvasResources toolCanvasResources;

};

#endif
