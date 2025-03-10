/* This file is part of the KDE project
   SPDX-FileCopyrightText: 1998, 1999 Torben Weis <weis@kde.org>
   SPDX-FileCopyrightText: 2000-2005 David Faure <faure@kde.org>
   SPDX-FileCopyrightText: 2007 Thorsten Zachmann <zachmann@kde.org>
   SPDX-FileCopyrightText: 2010 Boudewijn Rempt <boud@valdyas.org>
   SPDX-FileCopyrightText: 2015 Michael Abrahams <miabraha@gmail.com>

   SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KIS_PART_H
#define KIS_PART_H

#include <QList>
#include <QPointer>
#include <QUrl>
#include <QUuid>

#include <KisSessionResource.h>

#include "kritaui_export.h"
#include <kconfiggroup.h>
#include <KoConfig.h>
#include <kis_types.h>

namespace KIO {
}

class KisAction;
class KisDocument;
class KisView;
class KisDocument;
class KisIdleWatcher;
class KisAnimationCachePopulator;
class KisMainWindow;
class KisInputManager;
class KisViewManager;


/**
 * KisPart a singleton class which provides the main entry point to the application.
 * Krita supports multiple documents, multiple main windows, and multiple
 * components.  KisPart manages these resources and provides them to the rest of
 * Krita.  It manages lists of Actions and shortcuts as well.
 *
 * The terminology comes from KParts, which is a system allowing one KDE app
 * to be run from inside another, like pressing F4 inside dolphin to run konsole.
 *
 * Needless to say, KisPart hasn't got much to do with KParts anymore.
 */
class KRITAUI_EXPORT KisPart : public QObject
{
    Q_OBJECT

public:

    static KisPart *instance();

    /**
     * Constructor.
     *
     * @param parent may be another KisDocument, or anything else.
     *        Usually passed by KPluginFactory::create.
     */
    explicit KisPart();

    /**
     *  Destructor.
     *
     * The destructor does not delete any attached KisView objects and it does not
     * delete the attached widget as returned by widget().
     */
    ~KisPart() override;

    // ----------------- Document management -----------------

    /**
     * create an empty document. The document is not automatically registered with the part.
     */
    KisDocument *createDocument() const;

    /**
     * create a throwaway empty document. The document does not register a resource storage
     */
    KisDocument *createTemporaryDocument() const;

    /**
     * Add the specified document to the list of documents this KisPart manages.
     */
    void addDocument(KisDocument *document, bool notify = true);

    /**
     * @return a list of all documents this part manages
     */
    QList<QPointer<KisDocument> > documents() const;

    /**
     * @return number of documents this part manages.
     */
    int documentCount() const;

    void removeDocument(KisDocument *document, bool deleteDocument = true);

    // ----------------- MainWindow management -----------------


    /**
     * Create a new main window.
     */
    KisMainWindow *createMainWindow(QUuid id = QUuid());

    /**
     * @brief notifyMainWindowIsBeingCreated emits the sigMainWindowCreated signal
     * @param mainWindow
     */
    void notifyMainWindowIsBeingCreated(KisMainWindow *mainWindow);


    /**
     * Removes a main window from the list of managed windows.
     *
     * This is called by the MainWindow after it finishes its shutdown routine.
     */
    void removeMainWindow(KisMainWindow *mainWindow);

    /**
     * @return the list of main windows.
     */
    const QList<QPointer<KisMainWindow> >& mainWindows() const;

    /**
     * @return the number of shells for the main window
     */
    int mainwindowCount() const;

    void addRecentURLToAllMainWindows(QUrl url, QUrl oldUrl = QUrl());

    /**
     * Registers a file path to be added to the recents list, but do not apply
     * until the file has finished saving.
     */
    void queueAddRecentURLToAllMainWindowsOnFileSaved(QUrl url, QUrl oldUrl = QUrl());

    /**
     * @return the currently active main window.
     */
    KisMainWindow *currentMainwindow() const;

    /**
     * Gets the currently active KisMainWindow as a QWidget, useful when you
     * just need it to be used as a parent to a dialog or window without
     * needing to include `KisMainWindow.h`.
     */
    QWidget *currentMainwindowAsQWidget() const;

    KisMainWindow *windowById(QUuid id) const;

    /**
     * @return the application-wide KisIdleWatcher.
     */
    KisIdleWatcher *idleWatcher() const;

    // ----------------- Cache Populator Management -----------------
    /**
     * @return the application-wide AnimationCachePopulator.
     */
    KisAnimationCachePopulator *cachePopulator() const;

    class KisPlaybackEngine* playbackEngine() const;

    /**
     * Adds a frame time index to a priority stack, which should be
     * cached immediately and irregardless of whether it is the
     * the currently occupied frame. The process of regeneration is
     * started immediately.
     */
    void prioritizeFrameForCache(KisImageSP image, int frame);

public Q_SLOTS:

    /**
     * This slot loads an existing file.
     * @param path the file to load
     */
    void openExistingFile(const QString &path);

    /**
     * This slot loads a template and deletes the sender.
     * @param url the template to load
     */
    void openTemplate(const QUrl &url);


    /**
     * @brief startCustomDocument adds the given document to the document list and deletes the sender()
     * @param doc
     */
    void startCustomDocument(KisDocument *doc);

private Q_SLOTS:

    void updateIdleWatcherConnections();

    void updateShortcuts();

Q_SIGNALS:
    /**
     * emitted when a new document is opened. (for the idle watcher)
     */
    void documentOpened(const QString &ref);

    /**
     * emitted when an old document is closed. (for the idle watcher)
     */
    void documentClosed(const QString &ref);

    /**
     * Emitted when the animation PlaybackEngine is changed.
     * GUI objects that want to control playback should watch this signal 
     * and connect to the new playbackEgine as needed.
    */
   void playbackEngineChanged(KisPlaybackEngine *newPlaybackEngine);

    // These signals are for libkis or sketch
    void sigViewAdded(KisView *view);
    void sigViewRemoved(KisView *view);
    void sigDocumentAdded(KisDocument *document);
    void sigDocumentSaved(const QString &url);
    void sigDocumentRemoved(const QString &filename);
    void sigMainWindowIsBeingCreated(KisMainWindow *window);
    void sigMainWindowCreated();

public:

    KisInputManager *currentInputManager();

    //------------------ View management ------------------

    /**
     * Create a new view for the document. The view is added to the list of
     * views, and if the document wasn't known yet, it's registered as well.
     */
    KisView *createView(KisDocument *document,
                        KisViewManager *viewManager,
                        QWidget *parent);

    /**
     * Adds a view to the document. If the part doesn't know yet about
     * the document, it is registered.
     *
     * This calls KisView::updateReadWrite to tell the new view
     * whether the document is readonly or not.
     */
    void addView(KisView *view);

    /**
     * Removes a view of the document.
     */
    void removeView(KisView *view);

    /**
     * @return a list of views this document is displayed in
     */
    QList<QPointer<KisView> > views() const;

    /**
     * @return number of views this document is displayed in
     */
    int viewCount(KisDocument *doc) const;

    //------------------ Session management ------------------

    void showSessionManager();

    void startBlankSession();

    /**
     * Restores a saved session by name
     */
    bool restoreSession(const QString &sessionName);
    bool restoreSession(KisSessionResourceSP session);

    void setCurrentSession(KisSessionResourceSP session);

    /**
     * Attempts to save the session and close all windows.
     * This may involve asking the user to save open files.
     * @return false, if closing was cancelled by the user
     */
    bool closeSession(bool keepWindows = false);

    /**
     * Are we in the process of closing the application through closeSession().
     */
    bool closingSession() const;

    /**
     * This function returns true if the instance has already been initialized,
     * false otherwise. This to prevent premature initialization that causes crash
     * on android see `1fbb25506a`
     * @see QGlobalStatic::exists()
     */
    static bool exists();

    //------------------ Animation PlaybackEngine management ------------------
public:
    void upgradeToPlaybackEngineMLT(class KoCanvasBase *canvas);

    /**
     * Called on application to make sure that the engine is unloaded
     * before the MLT library is actually unloaded
     */
    void unloadPlaybackEngine();

private:

    void setPlaybackEngine(KisPlaybackEngine *p_playbackEngine);

private Q_SLOTS:

    void slotDocumentSaved(const QString &filePath);

private:

    Q_DISABLE_COPY(KisPart)

    class Private;
    Private *const d;

};

#endif
