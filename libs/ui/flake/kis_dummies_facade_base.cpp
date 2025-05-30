/*
 *  SPDX-FileCopyrightText: 2011 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QMutex>
#include <QMutexLocker>
#include "kis_dummies_facade_base.h"

#include "kis_image.h"
#include "kis_node_dummies_graph.h"
#include "kis_layer_utils.h"
#include <KisSynchronizedConnection.h>

struct KisDummiesFacadeBase::Private
{
public:
    KisImageWSP image;
    KisNodeSP savedRootNode;

    KisSynchronizedConnection<KisNodeSP, KisNodeAdditionFlags> activateNodeConnection;
    KisSynchronizedConnection<KisNodeSP> nodeChangedConnection;
    KisSynchronizedConnection<KisNodeSP,KisNodeSP,KisNodeSP> addNodeConnection;
    KisSynchronizedConnection<KisNodeSP> removeNodeConnection;

    /**
     * pendingNodeSet contains the set of nodes that will be present in the
     * dummies graph after all the synchronized events are processed by the GUI
     * thread. This set is used to reset the graph when the image changes its root
     * during the 'flatten' operation.
     */
    QList<KisNodeSP> pendingNodeSet;
    QMutex pendingNodeSetLock;

    /**
     * The node activation signal may be emitted while the facade was
     * not connected to anything. In such case we need to save the last-
     * emitted value for cold-initialization on the connection.
     */
    KisNodeWSP lastActivatedNode;
};


KisDummiesFacadeBase::KisDummiesFacadeBase(QObject *parent)
    : QObject(parent),
      m_d(new Private())
{
    m_d->activateNodeConnection.connectOutputSlot(this, &KisDummiesFacadeBase::slotNodeActivationRequested);
    m_d->nodeChangedConnection.connectOutputSlot(this, &KisDummiesFacadeBase::slotNodeChanged);
    m_d->addNodeConnection.connectOutputSlot(this, &KisDummiesFacadeBase::slotContinueAddNode);
    m_d->removeNodeConnection.connectOutputSlot(this, &KisDummiesFacadeBase::slotContinueRemoveNode);
}

KisDummiesFacadeBase::~KisDummiesFacadeBase()
{
    delete m_d;
}

void KisDummiesFacadeBase::setImage(KisImageWSP image)
{
    setImage(image, nullptr);
}

void KisDummiesFacadeBase::setImage(KisImageWSP image, KisNodeSP activeNode)
{
    if (m_d->image) {
        Q_EMIT sigActivateNode(0);
        m_d->lastActivatedNode = 0;
        m_d->image->disconnect(this);
        m_d->image->disconnect(&m_d->nodeChangedConnection);
        m_d->image->disconnect(&m_d->activateNodeConnection);

        KisNodeList nodesToRemove;

        {
            QMutexLocker l(&m_d->pendingNodeSetLock);
            std::swap(nodesToRemove, m_d->pendingNodeSet);
            m_d->pendingNodeSet.clear();
        }

        for (auto it = std::make_reverse_iterator(nodesToRemove.end());
             it != std::make_reverse_iterator(nodesToRemove.begin());
             ++it) {

            m_d->removeNodeConnection.start(*it);
        }
    }

    m_d->image = image;

    if (image) {
        slotNodeAdded(image->root(), KisNodeAdditionFlag::None);

        connect(image, SIGNAL(sigNodeAddedAsync(KisNodeSP, KisNodeAdditionFlags)),
                SLOT(slotNodeAdded(KisNodeSP, KisNodeAdditionFlags)), Qt::DirectConnection);
        connect(image, SIGNAL(sigRemoveNodeAsync(KisNodeSP)),
                SLOT(slotRemoveNode(KisNodeSP)), Qt::DirectConnection);
        connect(image, SIGNAL(sigLayersChangedAsync()),
                SLOT(slotLayersChanged()), Qt::DirectConnection);

        m_d->nodeChangedConnection.connectInputSignal(image, &KisImage::sigNodeChanged);
        m_d->activateNodeConnection.connectInputSignal(image, &KisImage::sigNodeAddedAsync);

        if (!activeNode) {
            activeNode = findFirstLayer(image->root());
        }

        m_d->activateNodeConnection.start(activeNode, KisNodeAdditionFlag::None);
    }
}

KisNodeSP KisDummiesFacadeBase::lastActivatedNode() const
{
    return m_d->lastActivatedNode;
}

KisImageWSP KisDummiesFacadeBase::image() const
{
    return m_d->image;
}

KisNodeSP KisDummiesFacadeBase::findFirstLayer(KisNodeSP root)
{
    KisNodeSP child = root->firstChild();
    while(child && !child->inherits("KisLayer")) {
        child = child->nextSibling();
    }
    return child;
}

void KisDummiesFacadeBase::slotNodeChanged(KisNodeSP node)
{
    KisNodeDummy *dummy = dummyForNode(node);

    /**
     * In some "buggy" code the node-changed signal may be emitted
     * before the node will become a part of the node graph. It is
     * a bug, we a really minor one. It should not cause any data
     * losses to the user.
     */
    KIS_SAFE_ASSERT_RECOVER_RETURN(dummy);

    Q_EMIT sigDummyChanged(dummy);
}

void KisDummiesFacadeBase::slotLayersChanged()
{
    setImage(m_d->image);
}

void KisDummiesFacadeBase::slotNodeActivationRequested(KisNodeSP node, KisNodeAdditionFlags flags)
{
    if (flags.testFlag(KisNodeAdditionFlag::DontActivateNode)) return;

    if (!node || !node->graphListener()) return;

    if (!node->inherits("KisSelectionMask") &&
        !node->inherits("KisReferenceImagesLayer") &&
        !node->inherits("KisDecorationsWrapperLayer")) {

        Q_EMIT sigActivateNode(node);
        m_d->lastActivatedNode = node;
    }
}

void KisDummiesFacadeBase::slotNodeAdded(KisNodeSP node, KisNodeAdditionFlags flags)
{
    Q_UNUSED(flags)

    {
        QMutexLocker l(&m_d->pendingNodeSetLock);
        m_d->pendingNodeSet.append(node);
    }

    m_d->addNodeConnection.start(node, node->parent(), node->prevSibling());

    KisNodeSP childNode = node->firstChild();
    while (childNode) {
        slotNodeAdded(childNode, flags);
        childNode = childNode->nextSibling();
    }
}

void KisDummiesFacadeBase::slotRemoveNode(KisNodeSP node)
{
    {
        QMutexLocker l(&m_d->pendingNodeSetLock);
        KIS_SAFE_ASSERT_RECOVER_RETURN(m_d->pendingNodeSet.contains(node));
    }

    KisNodeSP childNode = node->lastChild();
    while (childNode) {
        slotRemoveNode(childNode);
        childNode = childNode->prevSibling();
    }

    {
        QMutexLocker l(&m_d->pendingNodeSetLock);
        m_d->pendingNodeSet.removeOne(node);
    }
    m_d->removeNodeConnection.start(node);
}

void KisDummiesFacadeBase::slotContinueAddNode(KisNodeSP node, KisNodeSP parent, KisNodeSP aboveThis)
{
    KisNodeDummy *parentDummy = parent ? dummyForNode(parent) : 0;
    KisNodeDummy *aboveThisDummy = aboveThis ? dummyForNode(aboveThis) : 0;
    // Add one because this node does not exist yet
    int index = parentDummy && aboveThisDummy ?
        parentDummy->indexOf(aboveThisDummy) + 1 : 0;
    Q_EMIT sigBeginInsertDummy(parentDummy, index, node->metaObject()->className());

    addNodeImpl(node, parent, aboveThis);

    Q_EMIT sigEndInsertDummy(dummyForNode(node));
}

void KisDummiesFacadeBase::slotContinueRemoveNode(KisNodeSP node)
{
    KisNodeDummy *dummy = dummyForNode(node);
    Q_EMIT sigBeginRemoveDummy(dummy);

    removeNodeImpl(node);

    Q_EMIT sigEndRemoveDummy();
}
