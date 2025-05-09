/*
 *  SPDX-FileCopyrightText: 2017 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisAsyncAnimationFramesSaveDialog.h"

#include <kis_image.h>
#include <kis_time_span.h>

#include <KisAsyncAnimationFramesSavingRenderer.h>
#include "kis_properties_configuration.h"

#include "KisMimeDatabase.h"

#include <QFileInfo>
#include <QDir>
#include <QMessageBox>
#include <QApplication>

struct KisAsyncAnimationFramesSaveDialog::Private {
    Private(KisImageSP _image,
            const KisTimeSpan &_range,
            const QString &baseFilename,
            int _sequenceNumberingOffset,
            bool _onlyNeedsUniqueFrames,
            KisPropertiesConfigurationSP _exportConfiguration)
        : originalImage(_image),
          range(_range),
          onlyNeedsUniqueFrames(_onlyNeedsUniqueFrames),
          sequenceNumberingOffset(_sequenceNumberingOffset),
          exportConfiguration(_exportConfiguration)
    {
        int baseLength = baseFilename.lastIndexOf(".");
        if (baseLength > -1) {
            filenamePrefix = baseFilename.left(baseLength);
            filenameSuffix = baseFilename.right(baseFilename.length() - baseLength);
        } else {
            filenamePrefix = baseFilename;
        }

        outputMimeType = KisMimeDatabase::mimeTypeForFile(baseFilename, false).toLatin1();
    }

    KisImageSP originalImage;
    KisTimeSpan range;

    QString filenamePrefix;
    QString filenameSuffix;
    QByteArray outputMimeType;
    bool onlyNeedsUniqueFrames;

    int sequenceNumberingOffset;
    KisPropertiesConfigurationSP exportConfiguration;
};

KisAsyncAnimationFramesSaveDialog::KisAsyncAnimationFramesSaveDialog(KisImageSP originalImage,
                                                                     const KisTimeSpan &range,
                                                                     const QString &baseFilename,
                                                                     int startNumberingAt,
                                                                     bool onlyNeedsUniqueFrames,
                                                                     KisPropertiesConfigurationSP exportConfiguration)
    : KisAsyncAnimationRenderDialogBase(i18n("Saving frames..."), originalImage, 0),
      m_d(new Private(originalImage, range, baseFilename, qMax(startNumberingAt - range.start(), range.start() * -1), onlyNeedsUniqueFrames, exportConfiguration))
{


}

KisAsyncAnimationFramesSaveDialog::~KisAsyncAnimationFramesSaveDialog()
{
}

KisAsyncAnimationRenderDialogBase::Result KisAsyncAnimationFramesSaveDialog::regenerateRange(KisViewManager *viewManager)
{
    QFileInfo fileInfo(savedFilesMaskWildcard());
    QDir dir(fileInfo.absolutePath());

    if (!dir.exists()) {
        dir.mkpath(fileInfo.absolutePath());
    }
    KIS_SAFE_ASSERT_RECOVER_NOOP(dir.exists());

    // Check for overwrite. (Batch mode always overwrites.)
    QStringList preexistingFileNames = dir.entryList({ fileInfo.fileName() });
    if (!preexistingFileNames.isEmpty() && !batchMode()) {
        QStringList truncatedList = preexistingFileNames;

        while (truncatedList.size() > 3) {
            truncatedList.takeLast();
        }

        QString exampleFiles = truncatedList.join(", ");
        if (truncatedList.size() != preexistingFileNames.size()) {
            exampleFiles += QString(", ...");
        }

        QMessageBox::StandardButton result =
                QMessageBox::warning(qApp->activeWindow(),
                                     i18n("Delete old frames?"),
                                     i18n("Frames with the same naming "
                                          "scheme exist in the destination "
                                          "directory. They are going to be "
                                          "deleted, continue?\n\n"
                                          "Directory: %1\n"
                                          "Files: %2",
                                          fileInfo.absolutePath(), exampleFiles),
                                     QMessageBox::Yes | QMessageBox::No,
                                     QMessageBox::No);

        if (result == QMessageBox::No) {
            return RenderCancelled;
        }
    }

    // Remove any preexisting files.
    Q_FOREACH (const QString &file, preexistingFileNames) {
        if (!dir.remove(file)) {
            if (!batchMode()) {
                QMessageBox::critical(qApp->activeWindow(),
                                      i18n("Failed to delete"),
                                      i18n("Failed to delete an old frame file:\n\n"
                                           "%1\n\n"
                                           "Rendering cancelled.", dir.absoluteFilePath(file)));
            }

            qWarning() << "WARNING: KisAsyncAnimFramesSaveDialog: Failed to delete old frame file(s):" << dir.absoluteFilePath(file);
            return RenderFailed;
        }
    }

    // Save new frame files.
    KisAsyncAnimationRenderDialogBase::Result renderingResult = KisAsyncAnimationRenderDialogBase::regenerateRange(viewManager);

    // If we cancel rendering or fail rendering process,
    // lets clean up any files that may have been created
    // to keep the next render from having artifacts.
    preexistingFileNames = savedFiles();
    if (renderingResult != RenderComplete) {
        Q_FOREACH (const QString &file, preexistingFileNames) {
            if (dir.exists(file)) {
                (void)dir.remove(file);
            }
        }
    }

    return renderingResult;
}

QList<int> KisAsyncAnimationFramesSaveDialog::calcDirtyFrames() const
{
    QList<int> result;
    for (int frame = m_d->range.start(); frame <= m_d->range.end(); frame++) {
        KisTimeSpan heldFrameTimeRange = KisTimeSpan::calculateIdenticalFramesRecursive(m_d->originalImage->root(), frame);

        if (!m_d->onlyNeedsUniqueFrames) {
            // Clamp holds that begin before the rendered range onto it
            heldFrameTimeRange &= m_d->range;
        }

        KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(heldFrameTimeRange.isValid(), result);

        result.append(heldFrameTimeRange.start());

        if (heldFrameTimeRange.isInfinite()) {
            break;
        } else {
            frame = heldFrameTimeRange.end();
        }
    }
    return result;
}

KisAsyncAnimationRendererBase *KisAsyncAnimationFramesSaveDialog::createRenderer(KisImageSP image)
{
    return new KisAsyncAnimationFramesSavingRenderer(image,
                                                     m_d->filenamePrefix,
                                                     m_d->filenameSuffix,
                                                     m_d->outputMimeType,
                                                     m_d->range,
                                                     m_d->sequenceNumberingOffset,
                                                     m_d->onlyNeedsUniqueFrames,
                                                     m_d->exportConfiguration);
}

void KisAsyncAnimationFramesSaveDialog::initializeRendererForFrame(KisAsyncAnimationRendererBase *renderer, KisImageSP image, int frame)
{
    Q_UNUSED(renderer);
    Q_UNUSED(image);
    Q_UNUSED(frame);
}

QString KisAsyncAnimationFramesSaveDialog::savedFilesMask() const
{
    return m_d->filenamePrefix + "%04d" + m_d->filenameSuffix;
}

QString KisAsyncAnimationFramesSaveDialog::savedFilesMaskWildcard() const
{
    return m_d->filenamePrefix + "????" + m_d->filenameSuffix;
}

QStringList KisAsyncAnimationFramesSaveDialog::savedFiles() const
{
    QStringList files;

    for (int i = m_d->range.start(); i <= m_d->range.end(); i++) {
        const int num = m_d->sequenceNumberingOffset + i;
        QString name = QString("%1").arg(num, 4, 10, QChar('0'));
        name = m_d->filenamePrefix + name + m_d->filenameSuffix;
        files.append(QFileInfo(name).fileName());
    }

    return files;
}

QStringList KisAsyncAnimationFramesSaveDialog::savedUniqueFiles() const
{
    QStringList files;

    const QList<int> frames = calcDirtyFrames();

    Q_FOREACH (int frame, frames) {
        const int num = m_d->sequenceNumberingOffset + frame;
        QString name = QString("%1").arg(num, 4, 10, QChar('0'));
        name = m_d->filenamePrefix + name + m_d->filenameSuffix;
        files.append(QFileInfo(name).fileName());
    }

    return files;
}

QList<int> KisAsyncAnimationFramesSaveDialog::getUniqueFrames() const
{
    return this->calcDirtyFrames();
}
