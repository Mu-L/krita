/*
 *  SPDX-FileCopyrightText: 2012 Sven Langkamp <sven.langkamp@gmail.com>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "compositiondocker_dock.h"

#include <QGridLayout>
#include <QListView>
#include <QHeaderView>
#include <QPainter>
#include <QInputDialog>
#include <QThread>
#include <QAction>
#include <QStandardPaths>
#include <QMenu>
#include <QAction>
#include <QProgressDialog>

#include <klocalizedstring.h>
#include <kactioncollection.h>

#include <kis_icon.h>
#include <KoCanvasBase.h>
#include <KoFileDialog.h>

#include <KisPart.h>
#include <KisViewManager.h>
#include <kis_canvas2.h>
#include <KisKineticScroller.h>

#include <KisDocument.h>
#include <kis_group_layer.h>
#include <kis_painter.h>
#include <kis_paint_layer.h>
#include <kis_action.h>
#include <kis_action_manager.h>
#include <kis_action_registry.h>

#include <dialogs/KisAsyncAnimationFramesSaveDialog.h>
#include <animation/KisAnimationRenderingOptions.h>
#include <animation/KisAnimationRender.h>
#include <kis_image_animation_interface.h>
#include <kis_time_span.h>
#include <KisMimeDatabase.h>


#include "compositionmodel.h"


CompositionDockerDock::CompositionDockerDock( )
    : QDockWidget(i18n("Compositions"))
    , m_canvas(0)
{
    QWidget* widget = new QWidget(this);
    setupUi(widget);
    m_model = new CompositionModel(this);
    compositionView->setModel(m_model);
    compositionView->installEventFilter(this);
    deleteButton->setIcon(KisIconUtils::loadIcon("edit-delete"));
    saveButton->setIcon(KisIconUtils::loadIcon("list-add"));
    moveUpButton->setIcon(KisIconUtils::loadIcon("arrow-up"));
    moveDownButton->setIcon(KisIconUtils::loadIcon("arrow-down"));

    deleteButton->setToolTip(i18n("Delete Composition"));
    saveButton->setToolTip(i18n("New Composition"));
    exportCompositions->setToolTip(i18n("Export Composition"));
    moveUpButton->setToolTip(i18n("Move Composition Up"));
    moveDownButton->setToolTip(i18n("Move Composition Down"));

    setWidget(widget);

    connect( compositionView, SIGNAL(doubleClicked(QModelIndex)),
             this, SLOT(activated(QModelIndex)) );

    compositionView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect( compositionView, SIGNAL(customContextMenuRequested(QPoint)),
             this, SLOT(customContextMenuRequested(QPoint)));

    connect( deleteButton, SIGNAL(clicked(bool)), this, SLOT(deleteClicked()));
    connect( saveButton, SIGNAL(clicked(bool)), this, SLOT(saveClicked()));
    connect( moveUpButton, SIGNAL(clicked(bool)), this, SLOT(moveCompositionUp()));
    connect( moveDownButton, SIGNAL(clicked(bool)), this, SLOT(moveCompositionDown()));

    QAction* imageAction = new QAction(KisIconUtils::loadIcon("document-export-16"), i18n("Export Images"), this);
    connect(imageAction, SIGNAL(triggered(bool)), this, SLOT(exportImageClicked()));

    QAction* animationAction = new QAction(KisIconUtils::loadIcon("addblankframe-16"), i18n("Export Animations"), this);
    connect(animationAction, SIGNAL(triggered(bool)), this, SLOT(exportAnimationClicked()));

    exportCompositions->setDefaultAction(imageAction);

    QMenu* exportMenu = new QMenu(this);
    exportMenu->addAction(imageAction);
    exportMenu->addAction(animationAction);

    exportCompositions->setMenu(exportMenu);

    connect(exportMenu, &QMenu::triggered, [this](QAction* triggered){
        exportCompositions->setDefaultAction(triggered);
    });

    saveNameEdit->setPlaceholderText(i18n("Insert Name"));

    QScroller *scroller = KisKineticScroller::createPreconfiguredScroller(compositionView);
    if (scroller) {
        connect(scroller, SIGNAL(stateChanged(QScroller::State)), this, SLOT(slotScrollerStateChanged(QScroller::State)));
    }

}

CompositionDockerDock::~CompositionDockerDock()
{

}

void CompositionDockerDock::setCanvas(KoCanvasBase * canvas)
{
    if (m_canvas && m_canvas->viewManager()) {
        Q_FOREACH (KisAction *action, m_actions) {
            m_canvas->viewManager()->actionManager()->takeAction(action);
        }
    }

    unsetCanvas();
    setEnabled(canvas != 0);

    m_canvas = dynamic_cast<KisCanvas2*>(canvas);
    if (m_canvas && m_canvas->viewManager()) {
        if (m_actions.isEmpty()) {
            KisAction *updateAction = m_canvas->viewManager()->actionManager()->createAction("update_composition");
            connect(updateAction, SIGNAL(triggered()), this, SLOT(updateComposition()));
            m_actions.append(updateAction);

            KisAction *renameAction = m_canvas->viewManager()->actionManager()->createAction("rename_composition");
            connect(renameAction, SIGNAL(triggered()), this, SLOT(renameComposition()));
            m_actions.append(renameAction);

        } else {
            Q_FOREACH (KisAction *action, m_actions) {
                m_canvas->viewManager()->actionManager()->addAction(action->objectName(), action);
            }
        }
        updateModel();
    }
}

void CompositionDockerDock::unsetCanvas()
{
    setEnabled(false);
    m_canvas = 0;
    m_model->setCompositions(QList<KisLayerCompositionSP>());
}

void CompositionDockerDock::activated(const QModelIndex& index)
{
    KisLayerCompositionSP composition = m_model->compositionFromIndex(index);
    composition->apply();
}

void CompositionDockerDock::deleteClicked()
{
    QModelIndex index = compositionView->currentIndex();
    if (m_canvas && m_canvas->viewManager() && m_canvas->viewManager()->image() && index.isValid()) {
        KisLayerCompositionSP composition = m_model->compositionFromIndex(index);
        m_canvas->viewManager()->image()->removeComposition(composition);
        updateModel();

        const int compositionCount = compositionView->model()->rowCount();
        if (compositionCount > 0) {
            if (index.row() == compositionCount) {
                compositionView->setCurrentIndex(index.siblingAtRow(compositionCount-1));
            } else {
                compositionView->setCurrentIndex(index);
            }
        }
    }
}

void CompositionDockerDock::saveClicked()
{
    KisImageWSP image = m_canvas->viewManager()->image();
    if (!image) return;

    // format as 001, 002 ...
    QString name = saveNameEdit->text();
    if (name.isEmpty()) {
        bool found = false;
        int i = 1;
        do {
            name = QString("%1").arg(i, 3, 10, QChar('0'));
            found = false;
            Q_FOREACH (KisLayerCompositionSP composition, m_canvas->viewManager()->image()->compositions()) {
                if (composition->name() == name) {
                    found = true;
                    break;
                }
            }
            i++;
        } while(found && i < 1000);
    }
    KisLayerCompositionSP composition(new KisLayerComposition(image, name));
    composition->store();
    image->addComposition(composition);
    saveNameEdit->clear();
    updateModel();
    compositionView->setCurrentIndex(m_model->index(image->compositions().count()-1, 0));
    image->setModifiedWithoutUndo();
}

void CompositionDockerDock::moveCompositionUp()
{
    QModelIndex index = compositionView->currentIndex();
    if (m_canvas && m_canvas->viewManager() && m_canvas->viewManager()->image() && index.isValid()) {
        KisLayerCompositionSP composition = m_model->compositionFromIndex(index);
        m_canvas->viewManager()->image()->moveCompositionUp(composition);
        updateModel();
        compositionView->setCurrentIndex(m_model->index(m_canvas->viewManager()->image()->compositions().indexOf(composition),0));
    }
}

void CompositionDockerDock::moveCompositionDown()
{
    QModelIndex index = compositionView->currentIndex();
    if (m_canvas && m_canvas->viewManager() && m_canvas->viewManager()->image() && index.isValid()) {
        KisLayerCompositionSP composition = m_model->compositionFromIndex(index);
        m_canvas->viewManager()->image()->moveCompositionDown(composition);
        updateModel();
        compositionView->setCurrentIndex(m_model->index(m_canvas->viewManager()->image()->compositions().indexOf(composition),0));
    }
}

void CompositionDockerDock::updateModel()
{
    if (m_model && m_canvas && m_canvas->viewManager() && m_canvas->viewManager()->image()) {
        m_model->setCompositions(m_canvas->viewManager()->image()->compositions());
    }
}

void CompositionDockerDock::exportImageClicked()
{
    if (m_canvas && m_canvas->viewManager() && m_canvas->viewManager()->image()) {
        QString path;

        KoFileDialog dialog(0, KoFileDialog::OpenDirectory, "compositiondockerdock");
        dialog.setCaption(i18n("Select a Directory"));
        dialog.setDefaultDir(QStandardPaths::writableLocation(QStandardPaths::HomeLocation));
        path = dialog.filename();

        if (path.isNull()) return;

        if (!path.endsWith('/')) {
            path.append('/');
        }

        KisImageSP image = m_canvas->viewManager()->image();
        QString filename = m_canvas->viewManager()->document()->localFilePath();
        if (!filename.isEmpty()) {
            QFileInfo info(filename);
            path += info.completeBaseName() + '_';
        }

        KisLayerCompositionSP currentComposition = toQShared(new KisLayerComposition(image, "temp"));
        currentComposition->store();

        const QString exportWindowTitle = i18n("Exporting Compositions...");
        QSharedPointer<QProgressDialog> m_progressBar(new QProgressDialog(exportWindowTitle, i18n("Cancel"), 0, 0, this));
        const int compositionCount = image->compositions().size();
        m_progressBar->setCancelButton(0); //  TODO: This task should be threaded in the future so that canceling is possible,
                                           // Additionally, we should also be copying the image and working off that copy to
                                           // improve runtime performance and allow for the current krita instance to work
                                           // without locking up. **For now, we will simply make this operation uncancelable.**
        m_progressBar->setMaximum(compositionCount);
        m_progressBar->setMinimum(0);
        m_progressBar->setValue(0);
        int compositionsExported = 0;

        Q_FOREACH (KisLayerCompositionSP composition, image->compositions()) {
            if (m_progressBar->wasCanceled()) {
                break;
            }

            if (!composition->isExportEnabled()) {
                compositionsExported++;
                continue;
            }

            if (m_progressBar->isHidden()) {
                m_progressBar->show();
            }

            m_progressBar->setLabelText(i18n("Exporting composition: %1...", composition->name()));

            // TODO: Extension of above, but this algorithm is highly inefficient.
            // We should simply a) Deep Copy Image, b) Apply Composition, c) RefreshImageGraph,
            // d) Export Image.
            // This task can then be threaded to work in parallel.
            composition->apply();
            image->refreshGraphAsync();
            image->waitForDone();

            QRect r = image->bounds();

            KisDocument *d = KisPart::instance()->createDocument();

            KisImageSP dst = new KisImage(d->createUndoStore(), r.width(), r.height(), image->colorSpace(), composition->name());
            dst->setResolution(image->xRes(), image->yRes());
            d->setCurrentImage(dst);
            KisPaintLayer* paintLayer = new KisPaintLayer(dst, "projection", OPACITY_OPAQUE_U8);
            KisPainter gc(paintLayer->paintDevice());
            gc.bitBlt(QPoint(0, 0), image->rootLayer()->projection(), r);
            dst->addNode(paintLayer, dst->rootLayer(), KisLayerSP(0));

            dst->refreshGraphAsync();
            dst->waitForDone();

            d->setFileBatchMode(true);

            d->exportDocumentSync(path + composition->name() + ".png", "image/png");
            compositionsExported++;
            m_progressBar->setValue(compositionsExported);
            d->deleteLater();
        }

        currentComposition->apply();

        image->refreshGraphAsync();
        image->waitForDone();
    }

}

void CompositionDockerDock::exportAnimationClicked()
{
    KisConfig cfg(true);
    KisPropertiesConfigurationSP settings = cfg.exportConfiguration("ANIMATION_EXPORT");
    KisAnimationRenderingOptions exportOptions;
    exportOptions.fromProperties(settings);

    if (m_canvas &&
        m_canvas->viewManager() &&
        m_canvas->viewManager()->image() &&
        m_canvas->viewManager()->image()->animationInterface() &&
        m_canvas->viewManager()->document()) {

        QString path;

        KoFileDialog dialog(0, KoFileDialog::OpenDirectory, "compositiondockerdock");
        dialog.setCaption(i18n("Select a Directory"));
        dialog.setDefaultDir(QStandardPaths::writableLocation(QStandardPaths::HomeLocation));
        path = dialog.filename();

        if (path.isNull()) return;

        if (!path.endsWith('/')) {
            path.append('/');
        }

        KisImageSP image = m_canvas->viewManager()->image();
        QString filename = m_canvas->viewManager()->document()->localFilePath();
        if (!filename.isEmpty()) {
            QFileInfo info(filename);
            path += info.completeBaseName();
        }

        KisLayerCompositionSP currentComposition = toQShared(new KisLayerComposition(image, "temp"));
        currentComposition->store();

        const QString videoExtension = KisMimeDatabase::suffixesForMimeType(exportOptions.videoMimeType).first();

        Q_FOREACH (KisLayerCompositionSP composition, image->compositions()) {
            if(!composition->isExportEnabled())
                continue;

            composition->apply();
            image->refreshGraphAsync();
            image->waitForDone();

            KisTimeSpan range = image->animationInterface()->documentPlaybackRange();

            exportOptions.firstFrame = range.start();
            exportOptions.lastFrame = range.end();
            exportOptions.width = image->width();
            exportOptions.height = image->height();
            exportOptions.basename = QString("frame");
            exportOptions.videoFileName = QString("%1/%2/video.%3").arg(path, composition->name(), videoExtension);
            exportOptions.directory = QString("%1/%2").arg(path, composition->name());
            exportOptions.wantsOnlyUniqueFrameSequence = true;

            bool success = KisAnimationRender::render(m_canvas->viewManager()->document(), m_canvas->viewManager(), exportOptions);

            if (!success) break;
        }

        currentComposition->apply();
        image->refreshGraphAsync();
        image->waitForDone();
    }
}

bool CompositionDockerDock::eventFilter(QObject* obj, QEvent* event)
{
    if (event->type() == QEvent::KeyPress ) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Up || keyEvent->key() == Qt::Key_Down) {
            // new index will be set after the method is called
            QTimer::singleShot(0, this, SLOT(activateCurrentIndex()));
        }
        return false;
    } else {
        return QObject::eventFilter(obj, event);
    }
}

void CompositionDockerDock::activateCurrentIndex()
{
    QModelIndex index = compositionView->currentIndex();
    if (index.isValid()) {
        activated(index);
    }
}

void CompositionDockerDock::customContextMenuRequested(QPoint pos)
{
    if (m_actions.isEmpty()) return;

    QMenu menu;
    Q_FOREACH (KisAction *action, m_actions) {
        menu.addAction(action);

    }
    menu.exec(compositionView->mapToGlobal(pos));
}

void CompositionDockerDock::updateComposition()
{
    QModelIndex index = compositionView->currentIndex();
    if (m_canvas && m_canvas->viewManager() && m_canvas->viewManager()->image() && index.isValid()) {
        KisLayerCompositionSP composition = m_model->compositionFromIndex(index);
        composition->store();
        m_canvas->image()->setModifiedWithoutUndo();
    }
}

void CompositionDockerDock::renameComposition()
{
    dbgKrita << "rename";
    QModelIndex index = compositionView->currentIndex();
    if (m_canvas && m_canvas->viewManager() && m_canvas->viewManager()->image() && index.isValid()) {
        KisLayerCompositionSP composition = m_model->compositionFromIndex(index);
        bool ok;
        QString name = QInputDialog::getText(this, i18n("Rename Composition"),
                                             i18n("New Name:"), QLineEdit::Normal,
                                             composition->name(), &ok);
        if (ok && !name.isEmpty()) {
            composition->setName(name);
            m_canvas->image()->setModifiedWithoutUndo();
        }
    }
}


