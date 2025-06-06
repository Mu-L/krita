/*
 * This file is part of Krita
 *
 * SPDX-FileCopyrightText: 2018 Jouni Pentikainen <joupent@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_cross_channel_filter.h"

#include <Qt>
#include <QLayout>
#include <QPixmap>
#include <QPainter>
#include <QDomDocument>
#include <QHBoxLayout>
#include <QRegExp>

#include "KoChannelInfo.h"
#include "KoBasicHistogramProducers.h"
#include "KoColorModelStandardIds.h"
#include "KoColorSpace.h"
#include "KoColorTransformation.h"
#include "KoCompositeColorTransformation.h"
#include "KoCompositeOp.h"
#include "KoID.h"

#include "kis_signals_blocker.h"

#include "kis_bookmarked_configuration_manager.h"
#include "kis_config_widget.h"
#include <filter/kis_filter_configuration.h>
#include <kis_selection.h>
#include <kis_paint_device.h>
#include <kis_processing_information.h>
#include <libs/global/kis_dom_utils.h>

#include "kis_histogram.h"
#include "kis_painter.h"
#include "widgets/kis_curve_widget.h"
#include <KisGlobalResourcesInterface.h>

#include "../../color/colorspaceextensions/kis_hsv_adjustment.h"

// KisCrossChannelFilterConfiguration

KisCrossChannelFilterConfiguration::KisCrossChannelFilterConfiguration(int channelCount, const KoColorSpace *cs, KisResourcesInterfaceSP resourcesInterface)
    : KisMultiChannelFilterConfiguration(channelCount, "crosschannel", 1, resourcesInterface)
    , m_colorSpace(cs)
{
    init();

    int defaultDriver = 0;

    if (cs) {
        QVector<VirtualChannelInfo> virtualChannels = KisMultiChannelFilter::getVirtualChannels(cs);
        defaultDriver = qMax(0, KisMultiChannelFilter::findChannel(virtualChannels, VirtualChannelInfo::LIGHTNESS));
    }

    m_driverChannels.fill(defaultDriver, channelCount);
}

KisCrossChannelFilterConfiguration::KisCrossChannelFilterConfiguration(const KisCrossChannelFilterConfiguration &rhs)
    : KisMultiChannelFilterConfiguration(rhs),
      m_driverChannels(rhs.m_driverChannels)
{
}

KisCrossChannelFilterConfiguration::~KisCrossChannelFilterConfiguration()
{}

KisFilterConfigurationSP KisCrossChannelFilterConfiguration::clone() const
{
    return new KisCrossChannelFilterConfiguration(*this);
}

const QVector<int> KisCrossChannelFilterConfiguration::driverChannels() const
{
    return m_driverChannels;
}

void KisCrossChannelFilterConfiguration::setDriverChannels(QVector<int> driverChannels)
{
    KIS_SAFE_ASSERT_RECOVER_RETURN(driverChannels.size() == m_curves.size());

    // Clean unused properties
    if (driverChannels.size() < m_driverChannels.size()) {
        for (int i = driverChannels.size(); i < m_driverChannels.size(); ++i) {
            const QString name = QLatin1String("driver") + QString::number(i);
            KisColorTransformationConfiguration::removeProperty(name);
        }
    }

    m_driverChannels = driverChannels;

    // Update properties for python
    for (int i = 0; i < m_driverChannels.size(); ++i) {
        const QString name = QLatin1String("driver") + QString::number(i);
        const int value = m_driverChannels[i];
        KisColorTransformationConfiguration::setProperty(name, value);
    }
}

void KisCrossChannelFilterConfiguration::fromXML(const QDomElement& root)
{
    KisMultiChannelFilterConfiguration::fromXML(root);

    QVector<int> driverChannels;
    driverChannels.resize(m_curves.size());

    QRegExp rx("driver(\\d+)");
    for (QDomElement e = root.firstChildElement(); !e.isNull(); e = e.nextSiblingElement()) {
        const QString attributeName = e.attribute("name");

        if (rx.exactMatch(attributeName)) {
            int channel = rx.cap(1).toUShort();
            int driver = KisDomUtils::toInt(e.text());

            if (0 <= channel && channel < driverChannels.size()) {
                driverChannels[channel] = driver;
            }
        }
    }

    setDriverChannels(driverChannels);
}

void KisCrossChannelFilterConfiguration::toXML(QDomDocument& doc, QDomElement& root) const
{
    KisMultiChannelFilterConfiguration::toXML(doc, root);

    for (int i = 0; i < m_driverChannels.size(); i++) {
        QDomElement param = doc.createElement("param");
        param.setAttribute("name", QString("driver%1").arg(i));

        QDomText text = doc.createTextNode(KisDomUtils::toString(m_driverChannels[i]));
        param.appendChild(text);

        root.appendChild(param);
    }
}

KisCubicCurve KisCrossChannelFilterConfiguration::getDefaultCurve()
{
    const QList<QPointF> points { QPointF(0.0f, 0.5f), QPointF(1.0f, 0.5f) };
    return KisCubicCurve(points);
}

bool KisCrossChannelFilterConfiguration::compareTo(const KisPropertiesConfiguration *rhs) const
{
    const KisCrossChannelFilterConfiguration *otherConfig = dynamic_cast<const KisCrossChannelFilterConfiguration *>(rhs);

    return otherConfig
        && KisMultiChannelFilterConfiguration::compareTo(rhs)
        && m_driverChannels == otherConfig->m_driverChannels;
}

void KisCrossChannelFilterConfiguration::setProperty(const QString& name, const QVariant& value)
{
    if (name == "nTransfers") {
        KIS_SAFE_ASSERT_RECOVER_RETURN(value.canConvert<int>());

        const qint32 newChannelCount = value.toInt();
        const qint32 prevChannelCount = m_channelCount;

        if (newChannelCount == prevChannelCount) {
            return;
        }

        KisMultiChannelFilterConfiguration::setProperty(name, value);
        
        m_driverChannels.resize(newChannelCount);

        if (newChannelCount > prevChannelCount) {
            int defaultDriver = 0;

            if (m_colorSpace) {
                QVector<VirtualChannelInfo> virtualChannels = KisMultiChannelFilter::getVirtualChannels(m_colorSpace);
                defaultDriver = qMax(0, KisMultiChannelFilter::findChannel(virtualChannels, VirtualChannelInfo::LIGHTNESS));
            }

            for (qint32 i = prevChannelCount; i < newChannelCount; ++i) {
                m_driverChannels[i] = defaultDriver;

                const QString name = QLatin1String("driver") + QString::number(i);
                KisColorTransformationConfiguration::setProperty(name, defaultDriver);
            }
        } else {
            for (qint32 i = newChannelCount; i < prevChannelCount; ++i) {
                const QString name = QLatin1String("driver") + QString::number(i);
                KisColorTransformationConfiguration::removeProperty(name);
            }
        }

        return;
    }

    if (name == "activeCurve") {
        setActiveCurve(qBound(0, value.toInt(), m_channelCount));
    }

    int channelIndex;
    if (!channelIndexFromDriverPropertyName(name, channelIndex)) {
        KisMultiChannelFilterConfiguration::setProperty(name, value);
        return;
    }
    if (channelIndex < 0 || channelIndex >= m_channelCount) {
        return;
    }

    KIS_SAFE_ASSERT_RECOVER_RETURN(value.canConvert<int>());

    const int driver = value.toInt();
    m_driverChannels[channelIndex] = driver;
    KisColorTransformationConfiguration::setProperty(name, driver);
}

bool KisCrossChannelFilterConfiguration::channelIndexFromDriverPropertyName(const QString& name, int& driverIndex) const
{
    QRegExp rx("driver(\\d+)");
    if (rx.indexIn(name, 0) == -1) {
        return false;
    }

    driverIndex = rx.cap(1).toUShort();
    return true;
}

KisCrossChannelConfigWidget::KisCrossChannelConfigWidget(QWidget * parent, KisPaintDeviceSP dev, Qt::WindowFlags f)
        : KisMultiChannelConfigWidget(parent, dev, f)
{
    const int virtualChannelCount = m_virtualChannels.size();
    m_driverChannels.resize(virtualChannelCount);

    init();

    for (int i = 0; i < virtualChannelCount; i++) {
        const VirtualChannelInfo &info = m_virtualChannels[i];

        if (info.type() == VirtualChannelInfo::ALL_COLORS) {
            continue;
        }

        m_page->cmbDriverChannel->addItem(info.name(), i);
    }

    connect(m_page->cmbDriverChannel, SIGNAL(activated(int)), this, SLOT(slotDriverChannelSelected(int)));
}

// KisCrossChannelConfigWidget

KisCrossChannelConfigWidget::~KisCrossChannelConfigWidget()
{}

void KisCrossChannelConfigWidget::setConfiguration(const KisPropertiesConfigurationSP config)
{
    const auto *cfg = dynamic_cast<const KisCrossChannelFilterConfiguration*>(config.data());
    KIS_ASSERT(cfg);

    m_driverChannels = cfg->driverChannels();
    KisMultiChannelConfigWidget::setConfiguration(config);
    updateChannelControls();
}

int KisCrossChannelConfigWidget::findDefaultVirtualChannelSelection()
{
    // Show the first channel with a curve, or saturation by default

    int initialChannel = -1;
    for (int i = 0; i < m_virtualChannels.size(); i++) {
        if (!m_curves[i].isConstant(0.5)) {
            initialChannel = i;
            break;
        }
    }

    if (initialChannel < 0) {
        initialChannel = qMax(0, KisMultiChannelFilter::findChannel(m_virtualChannels, VirtualChannelInfo::SATURATION));
    }

    return initialChannel;
}


KisPropertiesConfigurationSP KisCrossChannelConfigWidget::configuration() const
{
    auto *cfg = new KisCrossChannelFilterConfiguration(m_virtualChannels.count(), m_dev->colorSpace(), KisGlobalResourcesInterface::instance());
    KisPropertiesConfigurationSP cfgSP = cfg;

    m_curves[m_activeVChannel] = m_page->curveWidget->curve();
    cfg->setCurves(m_curves);
    cfg->setActiveCurve(m_activeVChannel);
    cfg->setDriverChannels(m_driverChannels);

    return cfgSP;
}

void KisCrossChannelConfigWidget::updateChannelControls()
{
    m_curveControlsManager.reset(new KisCurveWidgetControlsManagerInt(m_page->curveWidget,
                                                                      m_page->intIn, m_page->intOut, 0, 100, -100, 100));

    const int index = m_page->cmbDriverChannel->findData(m_driverChannels[m_activeVChannel]);
    m_page->cmbDriverChannel->setCurrentIndex(index);
}


KisPropertiesConfigurationSP KisCrossChannelConfigWidget::getDefaultConfiguration()
{
    return new KisCrossChannelFilterConfiguration(m_virtualChannels.size(), m_dev->colorSpace(), KisGlobalResourcesInterface::instance());
}

void KisCrossChannelConfigWidget::slotDriverChannelSelected(int index)
{
    const int channel = m_page->cmbDriverChannel->itemData(index).toInt();

    KIS_SAFE_ASSERT_RECOVER_RETURN(0 <= channel && channel < m_virtualChannels.size());
    m_driverChannels[m_activeVChannel] = channel;

    updateChannelControls();
    Q_EMIT sigConfigurationItemChanged();
}

// KisCrossChannelFilter

KisCrossChannelFilter::KisCrossChannelFilter() : KisMultiChannelFilter(id(), i18n("&Cross-channel adjustment curves..."))
{}

KisCrossChannelFilter::~KisCrossChannelFilter()
{}

KisConfigWidget * KisCrossChannelFilter::createConfigurationWidget(QWidget *parent, const KisPaintDeviceSP dev, bool) const
{
    return new KisCrossChannelConfigWidget(parent, dev);
}

KisFilterConfigurationSP  KisCrossChannelFilter::factoryConfiguration(KisResourcesInterfaceSP resourcesInterface) const
{
    return new KisCrossChannelFilterConfiguration(0, nullptr, resourcesInterface);
}

int mapChannel(const VirtualChannelInfo &channel) {
    switch (channel.type()) {
    case VirtualChannelInfo::REAL: {
        int pixelIndex = channel.pixelIndex();
        KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(0 <= pixelIndex && pixelIndex < 4, 0);
        return pixelIndex;
    }
    case VirtualChannelInfo::ALL_COLORS:
        return KisHSVCurve::AllColors;
    case VirtualChannelInfo::HUE:
        return KisHSVCurve::Hue;
    case VirtualChannelInfo::SATURATION:
        return KisHSVCurve::Saturation;
    case VirtualChannelInfo::LIGHTNESS:
        return KisHSVCurve::Value;
    };

    KIS_SAFE_ASSERT_RECOVER_NOOP(false);
    return 0;
}

KoColorTransformation* KisCrossChannelFilter::createTransformation(const KoColorSpace* cs, const KisFilterConfigurationSP config) const
{
    const KisCrossChannelFilterConfiguration* configBC =
        dynamic_cast<const KisCrossChannelFilterConfiguration*>(config.data());
    Q_ASSERT(configBC);

    const QVector<QVector<quint16> > &originalTransfers = configBC->transfers();
    const QList<KisCubicCurve> &curves = configBC->curves();
    const QVector<int> &drivers = configBC->driverChannels();

    const QVector<VirtualChannelInfo> virtualChannels =
        KisMultiChannelFilter::getVirtualChannels(cs, originalTransfers.size());

    if (originalTransfers.size() > int(virtualChannels.size())) {
        // We got an illegal number of colorchannels :(
        return 0;
    }

    QVector<KoColorTransformation*> transforms;
    // Channel order reversed in order to adjust saturation before hue. This allows mapping grays to colors.
    for (int i = virtualChannels.size() - 1; i >= 0; i--) {
        if (!curves[i].isConstant(0.5)) {
            int channel = mapChannel(virtualChannels[i]);
            int driverChannel = mapChannel(virtualChannels[drivers[i]]);
            QHash<QString, QVariant> params;
            params["channel"] = channel;
            params["driverChannel"] = driverChannel;
            params["curve"] = QVariant::fromValue(originalTransfers[i]);
            params["relative"] = true;
            params["lumaRed"]   = cs->lumaCoefficients()[0];
            params["lumaGreen"] = cs->lumaCoefficients()[1];
            params["lumaBlue"]  = cs->lumaCoefficients()[2];

            transforms << cs->createColorTransformation("hsv_curve_adjustment", params);
        }
    }

    return KoCompositeColorTransformation::createOptimizedCompositeTransform(transforms);
}
