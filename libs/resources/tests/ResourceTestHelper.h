/*
 * SPDX-FileCopyrightText: 2018 boud <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef RESOURCETESTHELPER_H
#define RESOURCETESTHELPER_H

#include <QImageReader>
#include <QDir>
#include <QStandardPaths>
#include <QDirIterator>

#include <KisMimeDatabase.h>
#include <KisResourceLoaderRegistry.h>

#include <KisResourceLocator.h>
#include <KisResourceCacheDb.h>
#include "KisResourceTypes.h"
#include <DummyResource.h>
#include <KisStoragePlugin.h>
#include <simpletest.h>
#include "kis_debug.h"
#include <KisSqlQueryLoader.h>
#include <KisDatabaseTransactionLock.h>
#include <KisResourceModelProvider.h>

#ifndef FILES_DATA_DIR
#error "FILES_DATA_DIR not set. A directory with the data used for testing installing resources"
#endif

namespace ResourceTestHelper {

const QString &filesDestDir() {
    static const QString s_path = QDir::cleanPath(
            QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/testdest") + '/';
    return s_path;
}

void rmTestDb() {
    QDir dbLocation(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
    QFile(dbLocation.path() + "/" + KisResourceCacheDb::resourceCacheDbFilename).remove();
    dbLocation.rmpath(dbLocation.path());
}


class KisDummyResourceLoader : public KisResourceLoaderBase {
public:
    KisDummyResourceLoader(const QString &id, const QString &folder, const QString &name, const QStringList &mimetypes)
        : KisResourceLoaderBase(id, folder, name, mimetypes)
    {
    }

    virtual KoResourceSP create(const QString &name)
    {
        QSharedPointer<DummyResource> resource = QSharedPointer<DummyResource>::create(name, resourceType());
        return resource;
    }
};

void createDummyLoaderRegistry() {

    KisResourceLoaderRegistry *reg = KisResourceLoaderRegistry::instance();
    reg->add(new KisDummyResourceLoader(ResourceType::PaintOpPresets, ResourceType::PaintOpPresets,  i18n("Brush presets"), QStringList() << "application/x-krita-paintoppreset"));
    reg->add(new KisDummyResourceLoader(ResourceSubType::GbrBrushes, ResourceType::Brushes, i18n("Brush tips"), QStringList() << "image/x-gimp-brush"));
    reg->add(new KisDummyResourceLoader(ResourceSubType::GihBrushes, ResourceType::Brushes, i18n("Brush tips"), QStringList() << "image/x-gimp-brush-animated"));
    reg->add(new KisDummyResourceLoader(ResourceSubType::SvgBrushes, ResourceType::Brushes, i18n("Brush tips"), QStringList() << "image/svg+xml"));
    reg->add(new KisDummyResourceLoader(ResourceSubType::PngBrushes, ResourceType::Brushes, i18n("Brush tips"), QStringList() << "image/png"));
    reg->add(new KisDummyResourceLoader(ResourceSubType::SegmentedGradients, ResourceType::Gradients, i18n("Gradients"), QStringList() << "application/x-gimp-gradient"));
    reg->add(new KisDummyResourceLoader(ResourceSubType::StopGradients, ResourceType::Gradients, i18n("Gradients"), QStringList() << "image/svg+xml"));
    reg->add(new KisDummyResourceLoader(ResourceType::Palettes, ResourceType::Palettes, i18n("Palettes"),
                                        QStringList() << KisMimeDatabase::mimeTypeForSuffix("kpl")
                                        << KisMimeDatabase::mimeTypeForSuffix("gpl")
                                        << KisMimeDatabase::mimeTypeForSuffix("pal")
                                        << KisMimeDatabase::mimeTypeForSuffix("act")
                                        << KisMimeDatabase::mimeTypeForSuffix("aco")
                                        << KisMimeDatabase::mimeTypeForSuffix("css")
                                        << KisMimeDatabase::mimeTypeForSuffix("colors")
                                        << KisMimeDatabase::mimeTypeForSuffix("xml")
                                        << KisMimeDatabase::mimeTypeForSuffix("sbz")));

    QList<QByteArray> src = QImageReader::supportedMimeTypes();
    QStringList allImageMimes;
    Q_FOREACH(const QByteArray ba, src) {
        allImageMimes << QString::fromUtf8(ba);
    }
    allImageMimes << KisMimeDatabase::mimeTypeForSuffix("pat");

    reg->add(new KisDummyResourceLoader(ResourceType::Patterns, ResourceType::Patterns, i18n("Patterns"), allImageMimes));
    reg->add(new KisDummyResourceLoader(ResourceType::Workspaces, ResourceType::Workspaces, i18n("Workspaces"), QStringList() << "application/x-krita-workspace"));
    reg->add(new KisDummyResourceLoader(ResourceType::Symbols, ResourceType::Symbols, i18n("SVG symbol libraries"), QStringList() << "image/svg+xml"));
    reg->add(new KisDummyResourceLoader(ResourceType::WindowLayouts, ResourceType::WindowLayouts, i18n("Window layouts"), QStringList() << "application/x-krita-windowlayout"));
    reg->add(new KisDummyResourceLoader(ResourceType::Sessions, ResourceType::Sessions, i18n("Sessions"), QStringList() << "application/x-krita-session"));
    reg->add(new KisDummyResourceLoader(ResourceType::GamutMasks, ResourceType::GamutMasks, i18n("Gamut masks"), QStringList() << "application/x-krita-gamutmask"));

}

bool cleanDstLocation(const QString &dstLocation)
{
    if (QDir(dstLocation).exists()) {
        {
            QDirIterator iter(dstLocation, QStringList() << "*", QDir::Files, QDirIterator::Subdirectories);
            while (iter.hasNext()) {
                iter.next();
                QFile f(iter.filePath());
                f.remove();
                //qDebug() << (r ? "Removed" : "Failed to remove") << iter.filePath();
            }
        }
        {
            QDirIterator iter(dstLocation, QStringList() << "*", QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
            while (iter.hasNext()) {
                iter.next();
                QDir(iter.filePath()).rmdir(iter.filePath());
                //qDebug() << (r ? "Removed" : "Failed to remove") << iter.filePath();
            }
        }

        return QDir().rmpath(dstLocation);
    }
    return true;
}

void initTestDb()
{
    rmTestDb();
    cleanDstLocation(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
}

void overrideResourceVersion(KoResourceSP resource, int version)
{
    resource->setVersion(version);
}

void testVersionedStorage(KisStoragePlugin &storage, const QString &resourceType, const QString &resourceUrl, const QString &optionalFolderCheck = QString())
{
    const QFileInfo fileInfo(resourceUrl);

    auto verifyFileExists = [optionalFolderCheck, resourceType] (KoResourceSP res) {
        if (optionalFolderCheck.isEmpty()) return;

        const QString filePath = optionalFolderCheck + "/" + resourceType + "/" + res->filename();

        if (!QFileInfo(filePath).exists()) {
            qWarning() << "Couldn't find a file in the resource storage:";
            qWarning() << "    " << ppVar(res->filename());
            qWarning() << "    " << ppVar(optionalFolderCheck);
            qWarning() << "    " << ppVar(filePath);
        }

        QVERIFY(QFileInfo(filePath).exists());
    };

    KoResourceSP res1 = storage.resource(resourceUrl);
    QCOMPARE(res1->filename(), fileInfo.fileName()); // filenames are not URLs
    QCOMPARE(res1->version(), -1); // storages don't work with versions
    QCOMPARE(res1->valid(), true);

    const QString originalSomething = res1.dynamicCast<DummyResource>()->something();

    KoResourceSP res2 = storage.resource(resourceUrl);
    QCOMPARE(res2->filename(), fileInfo.fileName());
    QCOMPARE(res2->version(), -1); // storages don't work with versions
    QCOMPARE(res2->valid(), true);

    QVERIFY(res1 != res2);

    res2.dynamicCast<DummyResource>()->setSomething("It's changed");
    QCOMPARE(res1.dynamicCast<DummyResource>()->something(), originalSomething);
    QCOMPARE(res2.dynamicCast<DummyResource>()->something(), "It's changed");

    KoResourceSP res3 = storage.resource(resourceUrl);
    QCOMPARE(res3->filename(), fileInfo.fileName());
    QCOMPARE(res3->version(), -1); // storages don't work with versions
    QCOMPARE(res3->valid(), true);
    QCOMPARE(res3.dynamicCast<DummyResource>()->something(), originalSomething);

    const QString versionedName = fileInfo.baseName() + ".0001." + fileInfo.suffix();

    storage.saveAsNewVersion(resourceType, res2);
    QCOMPARE(res2->filename(), versionedName);
    QCOMPARE(res2->version(), -1); // storages don't work with versions
    QCOMPARE(res2->valid(), true);
    verifyFileExists(res2);

    KoResourceSP res4 = storage.resource(resourceType + "/" + versionedName);
    QCOMPARE(res4->filename(), versionedName);
    QCOMPARE(res4->version(), -1); // storages don't work with versions
    QCOMPARE(res4->valid(), true);
    QCOMPARE(res4.dynamicCast<DummyResource>()->something(), "It's changed");
    verifyFileExists(res4);

    overrideResourceVersion(res4, 10000);
    storage.saveAsNewVersion(resourceType, res4);
    QCOMPARE(res4->filename(), fileInfo.baseName() + ".10000." + fileInfo.suffix());
    verifyFileExists(res4);

    overrideResourceVersion(res4, -1);
    const QString versionedName2 = fileInfo.baseName() + ".10001." + fileInfo.suffix();

    storage.saveAsNewVersion(resourceType, res4);
    QCOMPARE(res4->filename(), versionedName2);
    QCOMPARE(res4->version(), -1); // storages don't work with versions
    QCOMPARE(res4->valid(), true);
    verifyFileExists(res4);
}

void testVersionedStorageIterator(KisStoragePlugin &storage, const QString &resourceType, const QString &resourceUrl)
{
    const QString basename = QFileInfo(resourceUrl).baseName();

    QSharedPointer<KisResourceStorage::ResourceIterator> iter = storage.resources(resourceType);
    QVERIFY(iter->hasNext());
    int count = 0;
    int numVersions = 0;
    while (iter->hasNext()) {
        iter->next();

        //qDebug() << iter->url() << ppVar(iter->guessedVersion()) << ppVar(iter->lastModified());

        if (iter->url().contains(basename)) {

            // because of versioning, the URL should have been changed
            QVERIFY(iter->url() != resourceUrl);

            //qDebug() << iter->url() << ppVar(iter->guessedVersion()) << ppVar(iter->lastModified());

            count++;

            auto verIt = iter->versions();
            while (verIt->hasNext()) {
                verIt->next();

                qDebug() << verIt->url() << ppVar(verIt->guessedVersion());
                numVersions++;
                QVERIFY(verIt->url().contains(basename));
            }
        }

        KoResourceSP res = iter->resource();
        QVERIFY(res);
    }

    QCOMPARE(count, 1);
    QCOMPARE(numVersions, 4);
};

bool recreateDatabaseForATest(KisResourceLocator *locator, const QString &srcLocation, const QString &dstLocation)
{
    auto listDbResources = [](const QString &dbResourceType) {
        KisSqlQueryLoader loader("inline://list_all_db_tables",
                                 "SELECT name FROM sqlite_master WHERE sql IS NOT NULL and name != \"sqlite_sequence\" "
                                 "and type = :db_resource_type",
                                 KisSqlQueryLoader::single_statement_mode);
        loader.query().bindValue(":db_resource_type", dbResourceType);
        loader.exec();

        QVector<QString> dbResources;
        while (loader.query().next()) {
            dbResources.append(loader.query().value(0).toString());
        }
        return dbResources;
    };

    auto dropDbResource = [](const QString &dbResourceType, const QString &dbResourceName) {
        KisSqlQueryLoader loader("inline://drop_db_resource_" + dbResourceType,
                                 QString("DROP %1 %2").arg(dbResourceType.toUpper(), dbResourceName));
        loader.exec();

        QVector<QString> dbResources;
        while (loader.query().next()) {
            dbResources.append(loader.query().value(0).toString());
        }
        return dbResources;
    };

    if (QSqlDatabase::database(QSqlDatabase::defaultConnection, false).isOpen()) {
        try {
            KisResourceModelProvider::testingCloseAllQueries();

            // foreign keys should be disabled outside the transaction's scope!
            KisResourceCacheDb::setForeignKeysStateImpl(false);

            KisDatabaseTransactionLock transactionLock(QSqlDatabase::database());

            Q_FOREACH (const QString &dbResourceType, QStringList({"table", "index", "trigger", "view"})) {
                auto resources = listDbResources(dbResourceType);
                Q_FOREACH (const auto &resource, resources) {
                    // qDebug() << "dropping" << ppVar(dbResourceType) << ppVar(resource);
                    dropDbResource(dbResourceType, resource);
                }
            }

            // defuse the lock and save the results
            transactionLock.commit();

            KisResourceCacheDb::setForeignKeysStateImpl(true);

        } catch (const KisSqlQueryLoader::SQLException &e) {
            qWarning().noquote() << "ERROR: failed to execute query:" << e.message;
            qWarning().noquote() << "       file:" << e.filePath;
            qWarning().noquote() << "       statement:" << e.statementIndex;
            qWarning().noquote() << "       error:" << e.sqlError.text();

            return false;
        }
    }

    ResourceTestHelper::cleanDstLocation(dstLocation);

    // Reinitialize the database from scratch
    KisResourceCacheDb::initialize(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));

    KisResourceLocator::LocatorError r = locator->initialize(srcLocation);

    if (!locator->errorMessages().isEmpty()) {
        qDebug() << locator->errorMessages();
    }
    if (r != KisResourceLocator::LocatorError::Ok) {
        return false;
    }

    return true;
}

}

#endif // RESOURCETESTHELPER_H
