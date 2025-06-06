/*
 *  SPDX-FileCopyrightText: 2016 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */
#include "Channel.h"

#include <cstring>

#include <QByteArray>
#include <QDataStream>
#include <QIODevice>

#include <KoColorModelStandardIds.h>
#include <KoConfig.h>
#include <KoColorSpaceRegistry.h>
#include <KoColorSpace.h>
#include <kis_sequential_iterator.h>
#include <kis_layer.h>

#ifdef HAVE_OPENEXR
#include <half.h>
#endif

struct Channel::Private {
    Private() {}

    KisNodeSP node;
    KoChannelInfo *channel {0};

};

Channel::Channel(KisNodeSP node, KoChannelInfo *channel, QObject *parent)
    : QObject(parent)
    , d(new Private)
{
    d->node = node;
    d->channel = channel;
}

Channel::~Channel()
{
    delete d;
}


bool Channel::operator==(const Channel &other) const
{
    return (d->node == other.d->node
            && d->channel == other.d->channel);
}

bool Channel::operator!=(const Channel &other) const
{
    return !(operator==(other));
}


bool Channel::visible() const
{
    if (!d->node || !d->channel) return false;
    if (!d->node->inherits("KisLayer")) return false;

    const QList<KoChannelInfo *> channelInfo = d->node->colorSpace()->channels();

    for (uint i = 0; i < channelInfo.size(); ++i) {
        if (channelInfo[i] == d->channel) {
            KisLayerSP layer = qobject_cast<KisLayer*>(d->node.data());
            const QBitArray& flags = layer->channelFlags();
            return flags.isEmpty() || flags.testBit(i);
        }
    }

    return false;
}

void Channel::setVisible(bool value)
{
    if (!d->node || !d->channel) return;
    if (!d->node->inherits("KisLayer")) return;

    const QList<KoChannelInfo *> channelInfo = d->node->colorSpace()->channels();

    KisLayerSP layer = qobject_cast<KisLayer*>(d->node.data());
    QBitArray flags = layer->channelFlags();
    if (flags.isEmpty()) {
        flags.fill(1, channelInfo.size());
    }

    for (uint i = 0; i < channelInfo.size(); ++i) {
        if (channelInfo[i] == d->channel) {
            flags.setBit(i, value);
            layer->setChannelFlags(flags);
            break;
        }
    }

}

QString Channel::name() const
{
    return d->channel->name();
}

int Channel::position() const
{
    return d->channel->pos();
}

int Channel::channelSize() const
{
    return d->channel->size();
}

QRect Channel::bounds() const
{
    if (!d->node || !d->channel) return QRect();

    QRect rect = d->node->exactBounds();

    KisPaintDeviceSP dev;
    if (d->node->colorSpace()->colorDepthId() == Integer8BitsColorDepthID) {
        dev = new KisPaintDevice(KoColorSpaceRegistry::instance()->alpha8());
    }
    else if (d->node->colorSpace()->colorDepthId() ==  Integer16BitsColorDepthID) {
        dev = new KisPaintDevice(KoColorSpaceRegistry::instance()->alpha16());
    }
#ifdef HAVE_OPENEXR
    else if (d->node->colorSpace()->colorDepthId() == Float16BitsColorDepthID) {
        dev = new KisPaintDevice(KoColorSpaceRegistry::instance()->alpha16f());
    }
#endif
    else if (d->node->colorSpace()->colorDepthId() == Float32BitsColorDepthID) {
        dev = new KisPaintDevice(KoColorSpaceRegistry::instance()->alpha32f());
    }

    KisSequentialConstIterator srcIt(d->node->projection(), rect);
    KisSequentialIterator dstIt(dev, rect);

    while(srcIt.nextPixel() && dstIt.nextPixel()) {
        const quint8 *srcPtr = srcIt.rawDataConst();
        memcpy(dstIt.rawData(), srcPtr + d->channel->pos(), d->channel->size());

    }

    if (dev) {
        return dev->exactBounds();
    }

    return QRect();
}

QByteArray Channel::pixelData(const QRect &rect) const
{
    QByteArray ba;

    if (!d->node || !d->channel) return ba;

    QDataStream stream(&ba, QIODevice::WriteOnly);
    KisSequentialConstIterator srcIt(d->node->projection(), rect);

    if (d->node->colorSpace()->colorDepthId() == Integer8BitsColorDepthID) {
        while(srcIt.nextPixel()) {
            quint8 v;
            memcpy(&v, srcIt.rawDataConst() + d->channel->pos(), sizeof(v));
            stream << v;
        }
    }
    else if (d->node->colorSpace()->colorDepthId() ==  Integer16BitsColorDepthID) {
        while(srcIt.nextPixel()) {
            quint16 v;
            memcpy(&v, srcIt.rawDataConst() + d->channel->pos(), sizeof(v));
            stream << v;
        }
    }
#ifdef HAVE_OPENEXR
    else if (d->node->colorSpace()->colorDepthId() == Float16BitsColorDepthID) {
        while(srcIt.nextPixel()) {
            half v;
            memcpy(&v, srcIt.rawDataConst() + d->channel->pos(), sizeof(v));
            stream << (float) v;
        }
    }
#endif
    else if (d->node->colorSpace()->colorDepthId() == Float32BitsColorDepthID) {
        while(srcIt.nextPixel()) {
            float v;
            memcpy(&v, srcIt.rawDataConst() + d->channel->pos(), sizeof(v));
            stream << v;
        }

    }

    return ba;
}

void Channel::setPixelData(QByteArray value, const QRect &rect)
{
    if (!d->node || !d->channel || d->node->paintDevice() == 0) return;

    QDataStream stream(&value, QIODevice::ReadOnly);
    KisSequentialIterator dstIt(d->node->paintDevice(), rect);

    if (d->node->colorSpace()->colorDepthId() == Integer8BitsColorDepthID) {
        while (dstIt.nextPixel()) {
            quint8 v;
            stream >> v;
            memcpy(dstIt.rawData() + d->channel->pos(), &v, sizeof(v));
        }
    }
    else if (d->node->colorSpace()->colorDepthId() ==  Integer16BitsColorDepthID) {
        while (dstIt.nextPixel()) {
            quint16 v;
            stream >> v;
            memcpy(dstIt.rawData() + d->channel->pos(), &v, sizeof(v));
        }
    }
#ifdef HAVE_OPENEXR
    else if (d->node->colorSpace()->colorDepthId() == Float16BitsColorDepthID) {
        while (dstIt.nextPixel()) {
            float f;
            stream >> f;
            half v = f;
            memcpy(dstIt.rawData() + d->channel->pos(), &v, sizeof(v));
        }

    }
#endif
    else if (d->node->colorSpace()->colorDepthId() == Float32BitsColorDepthID) {
        while (dstIt.nextPixel()) {
            float v;
            stream >> v;
            memcpy(dstIt.rawData() + d->channel->pos(), &v, sizeof(v));
        }
    }
}




