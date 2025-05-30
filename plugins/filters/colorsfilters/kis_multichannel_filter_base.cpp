/*
 * This file is part of Krita
 *
 * SPDX-FileCopyrightText: 2018 Jouni Pentikainen <joupent@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_multichannel_filter_base.h"

#include <Qt>
#include <QLayout>
#include <QPixmap>
#include <QPainter>
#include <QDomDocument>
#include <QHBoxLayout>
#include <QMessageBox>
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
#include <filter/kis_filter_category_ids.h>
#include <filter/kis_filter_configuration.h>
#include <kis_selection.h>
#include <kis_paint_device.h>
#include <kis_processing_information.h>

#include "kis_histogram.h"
#include "kis_painter.h"
#include "widgets/kis_curve_widget.h"

#include "kis_multichannel_utils.h"

KisMultiChannelFilter::KisMultiChannelFilter(const KoID& id, const QString &entry)
        : KisColorTransformationFilter(id, FiltersCategoryAdjustId, entry)
{
    setSupportsPainting(true);
    setColorSpaceIndependence(TO_LAB16);
}

bool KisMultiChannelFilter::needsTransparentPixels(const KisFilterConfigurationSP config, const KoColorSpace *cs) const
{
    Q_UNUSED(config);
    return cs->colorModelId() == AlphaColorModelID;
}

QVector<VirtualChannelInfo> KisMultiChannelFilter::getVirtualChannels(const KoColorSpace *cs, int maxChannels)
{
    return KisMultiChannelUtils::getVirtualChannels(cs, maxChannels);
}

int KisMultiChannelFilter::findChannel(const QVector<VirtualChannelInfo> &virtualChannels,
                                       const VirtualChannelInfo::Type &channelType)
{
    return KisMultiChannelUtils::findChannel(virtualChannels, channelType);
}


KisMultiChannelFilterConfiguration::KisMultiChannelFilterConfiguration(int channelCount, const QString & name, qint32 version, KisResourcesInterfaceSP resourcesInterface)
        : KisColorTransformationConfiguration(name, version, resourcesInterface)
        , m_channelCount(channelCount)
{
}

KisMultiChannelFilterConfiguration::KisMultiChannelFilterConfiguration(const KisMultiChannelFilterConfiguration &rhs)
    : KisColorTransformationConfiguration(rhs),
      m_channelCount(rhs.m_channelCount),
      m_curves(rhs.m_curves),
      m_transfers(rhs.m_transfers)
{
}

KisMultiChannelFilterConfiguration::~KisMultiChannelFilterConfiguration()
{}

void KisMultiChannelFilterConfiguration::init()
{
    m_curves.clear();
    
    KisColorTransformationConfiguration::setProperty("nTransfers", m_channelCount);

    for (int i = 0; i < m_channelCount; ++i) {
        m_curves.append(getDefaultCurve());

        const QString name = QLatin1String("curve") + QString::number(i);
        const QString value = m_curves.last().toString();
        KisColorTransformationConfiguration::setProperty(name, value);
    }

    updateTransfers();
}

bool KisMultiChannelFilterConfiguration::isCompatible(const KisPaintDeviceSP dev) const
{
    return (int)dev->compositionSourceColorSpace()->channelCount() == m_channelCount;
}

void KisMultiChannelFilterConfiguration::setCurves(QList<KisCubicCurve> &curves)
{
    // Clean unused properties
    if (curves.size() < m_curves.size()) {
        for (int i = curves.size(); i < m_curves.size(); ++i) {
            const QString name = QLatin1String("curve") + QString::number(i);
            KisColorTransformationConfiguration::removeProperty(name);
        }
    }

    m_curves.clear();
    m_curves = curves;
    m_channelCount = curves.size();
    m_activeCurve = qMin(m_activeCurve, m_channelCount - 1);

    updateTransfers();

    // Update properties for python
    KisColorTransformationConfiguration::setProperty("nTransfers", m_channelCount);

    for (int i = 0; i < m_curves.size(); ++i) {
        const QString name = QLatin1String("curve") + QString::number(i);
        const QString value = m_curves[i].toString();
        KisColorTransformationConfiguration::setProperty(name, value);
    }
}

void KisMultiChannelFilterConfiguration::setActiveCurve(int value)
{
    m_activeCurve = value;
    KisColorTransformationConfiguration::setProperty("activeCurve", value);
}

void KisMultiChannelFilterConfiguration::updateTransfer(int index)
{
    KIS_SAFE_ASSERT_RECOVER_RETURN(index >= 0 && index < m_curves.size());
    m_transfers[index] = m_curves[index].uint16Transfer();
}

void KisMultiChannelFilterConfiguration::updateTransfers()
{
    m_transfers.resize(m_channelCount);
    for (int i = 0; i < m_channelCount; i++) {
        m_transfers[i] = m_curves[i].uint16Transfer();
    }
}

const QVector<QVector<quint16> >&
KisMultiChannelFilterConfiguration::transfers() const
{
    return m_transfers;
}

const QList<KisCubicCurve>&
KisMultiChannelFilterConfiguration::curves() const
{
    return m_curves;
}

void KisMultiChannelFilterConfiguration::fromLegacyXML(const QDomElement& root)
{
    fromXML(root);
}

void KisMultiChannelFilterConfiguration::fromXML(const QDomElement& root)
{
    QList<KisCubicCurve> curves;
    quint16 numTransfers = 0;
    quint16 numTransfersWithAlpha = 0;
    int activeCurve = -1;
    int version;
    version = root.attribute("version").toInt();

    QDomElement e = root.firstChild().toElement();
    QString attributeName;
    KisCubicCurve curve;
    quint16 index;
    QRegExp curveRegexp("curve(\\d+)");

    while (!e.isNull()) {
        if ((attributeName = e.attribute("name")) == "activeCurve") {
            activeCurve = e.text().toInt();
        } else if ((attributeName = e.attribute("name")) == "nTransfers") {
            numTransfers = e.text().toUShort();
        } else if ((attributeName = e.attribute("name")) == "nTransfersWithAlpha") {
            numTransfersWithAlpha = e.text().toUShort();
        } else {
            if (curveRegexp.indexIn(attributeName, 0) != -1) {

                index = curveRegexp.cap(1).toUShort();
                index = qMin(index, quint16(curves.count()));

                if (!e.text().isEmpty()) {
                    curve = KisCubicCurve(e.text());
                }
                curves.insert(index, curve);
            }
        }
        e = e.nextSiblingElement();
    }

    /**
     * In Krita 2.9 we stored alpha channel under a separate tag, so we
     * should addend it separately if present
     */
    if (numTransfersWithAlpha > numTransfers) {
        e = root.firstChild().toElement();
        while (!e.isNull()) {
            if ((attributeName = e.attribute("name")) == "alphaCurve") {
                if (!e.text().isEmpty()) {
                    curves.append(KisCubicCurve(e.text()));
                }
            }
            e = e.nextSiblingElement();
        }
    }

    //prepend empty curves for the brightness contrast filter.
    if(getString("legacy") == "brightnesscontrast") {
        if (getString("colorModel") == LABAColorModelID.id()) {
            curves.append(KisCubicCurve());
            curves.append(KisCubicCurve());
            curves.append(KisCubicCurve());
        } else {
            int extraChannels = 5;
            if (getString("colorModel") == CMYKAColorModelID.id()) {
                extraChannels = 6;
            } else if (getString("colorModel") == GrayAColorModelID.id()) {
                extraChannels = 0;
            }
            for(int c = 0; c < extraChannels; c ++) {
                curves.insert(0, KisCubicCurve());
            }
        }
    }
    if (!numTransfers)
        return;

    setVersion(version);
    setCurves(curves);
    setActiveCurve(activeCurve);
}

/**
 * Inherited from KisPropertiesConfiguration
 */
//void KisMultiChannelFilterConfiguration::fromXML(const QString& s)

void addParamNode(QDomDocument& doc,
                  QDomElement& root,
                  const QString &name,
                  const QString &value)
{
    QDomText text = doc.createTextNode(value);
    QDomElement t = doc.createElement("param");
    t.setAttribute("name", name);
    t.appendChild(text);
    root.appendChild(t);
}

void KisMultiChannelFilterConfiguration::toXML(QDomDocument& doc, QDomElement& root) const
{
    /**
     * @code
     * <params version=1>
     *       <param name="nTransfers">3</param>
     *       <param name="curve0">0,0;0.5,0.5;1,1;</param>
     *       <param name="curve1">0,0;1,1;</param>
     *       <param name="curve2">0,0;1,1;</param>
     * </params>
     * @endcode
     */

    root.setAttribute("version", version());

    QDomText text;
    QDomElement t;

    addParamNode(doc, root, "nTransfers", QString::number(m_channelCount));

    if (m_activeCurve >= 0) {
        // save active curve if only it has non-default value
        addParamNode(doc, root, "activeCurve", QString::number(m_activeCurve));
    }

    KisCubicCurve curve;
    QString paramName;

    for (int i = 0; i < m_curves.size(); ++i) {
        QString name = QLatin1String("curve") + QString::number(i);
        QString value = m_curves[i].toString();

        addParamNode(doc, root, name, value);
    }
}

bool KisMultiChannelFilterConfiguration::compareTo(const KisPropertiesConfiguration *rhs) const
{
    const KisMultiChannelFilterConfiguration *otherConfig = dynamic_cast<const KisMultiChannelFilterConfiguration *>(rhs);

    return otherConfig
        && KisFilterConfiguration::compareTo(rhs)
        && m_channelCount == otherConfig->m_channelCount
        && m_curves == otherConfig->m_curves
        && m_transfers == otherConfig->m_transfers
        && m_activeCurve == otherConfig->m_activeCurve;
}

void KisMultiChannelFilterConfiguration::setProperty(const QString& name, const QVariant& value)
{
    if (name == "nTransfers") {
        KIS_SAFE_ASSERT_RECOVER_RETURN(value.canConvert<int>());

        const qint32 newChannelCount = value.toInt();

        if (newChannelCount == m_channelCount) {
            return;
        }

        KisColorTransformationConfiguration::setProperty(name, value);

        m_transfers.resize(newChannelCount);
        if (newChannelCount > m_channelCount) {
            for (qint32 i = m_channelCount; i < newChannelCount; ++i) {
                m_curves.append(getDefaultCurve());
                updateTransfer(i);

                const QString name = QLatin1String("curve") + QString::number(i);
                const QString value = m_curves.last().toString();
                KisColorTransformationConfiguration::setProperty(name, value);
            }
        } else {
            for (qint32 i = newChannelCount; i < m_channelCount; ++i) {
                m_curves.removeLast();

                const QString name = QLatin1String("curve") + QString::number(i);
                KisColorTransformationConfiguration::removeProperty(name);
            }
        }

        m_channelCount = newChannelCount;
        invalidateColorTransformationCache();


        return;
    }

    if (name == "activeCurve") {
        setActiveCurve(qBound(0, value.toInt(), m_channelCount));
    }

    int curveIndex;
    if (!curveIndexFromCurvePropertyName(name, curveIndex) ||
        curveIndex < 0 || curveIndex >= m_channelCount) {
        return;
    }

    KIS_SAFE_ASSERT_RECOVER_RETURN(value.canConvert<QString>());

    m_curves[curveIndex] = KisCubicCurve(value.toString());
    updateTransfer(curveIndex);
    invalidateColorTransformationCache();

    // Query the curve instead of using the value directly, in case of not valid curve string
    KisColorTransformationConfiguration::setProperty(name, m_curves[curveIndex].toString());
}

bool KisMultiChannelFilterConfiguration::curveIndexFromCurvePropertyName(const QString& name, int& curveIndex) const
{
    QRegExp rx("curve(\\d+)");
    if (rx.indexIn(name, 0) == -1) {
        return false;
    }

    curveIndex = rx.cap(1).toUShort();
    return true;
}

KisMultiChannelConfigWidget::KisMultiChannelConfigWidget(QWidget * parent, KisPaintDeviceSP dev, Qt::WindowFlags f)
        : KisConfigWidget(parent, f)
        , m_dev(dev)
        , m_page(new WdgPerChannel(this))
{
    Q_ASSERT(m_dev);

    const KoColorSpace *targetColorSpace = dev->compositionSourceColorSpace();
    m_virtualChannels = KisMultiChannelFilter::getVirtualChannels(targetColorSpace);
}

/**
 * Initialize the dialog.
 * Note: m_virtualChannels must be populated before calling this
 */
void KisMultiChannelConfigWidget::init() {
    QHBoxLayout * layout = new QHBoxLayout(this);
    Q_CHECK_PTR(layout);
    layout->setContentsMargins(0,0,0,0);
    layout->addWidget(m_page);

    resetCurves();

    const int virtualChannelCount = m_virtualChannels.size();
    for (int i = 0; i < virtualChannelCount; i++) {
        const VirtualChannelInfo &info = m_virtualChannels[i];
        m_page->cmbChannel->addItem(info.name(), i);
    }

    connect(m_page->cmbChannel, SIGNAL(activated(int)), this, SLOT(slotChannelSelected(int)));
    connect((QObject*)(m_page->chkLogarithmic), SIGNAL(toggled(bool)), this, SLOT(logHistView()));
    connect((QObject*)(m_page->resetButton), SIGNAL(clicked()), this, SLOT(resetCurve()));

    // create the horizontal and vertical gradient labels
    m_page->hgradient->setPixmap(createGradient(Qt::Horizontal));
    m_page->vgradient->setPixmap(createGradient(Qt::Vertical));

    // init histogram calculator
    const KoColorSpace *targetColorSpace = m_dev->compositionSourceColorSpace();
    QList<QString> keys =
        KoHistogramProducerFactoryRegistry::instance()->keysCompatibleWith(targetColorSpace);

    if (keys.size() > 0) {
        KoHistogramProducerFactory *hpf;
        hpf = KoHistogramProducerFactoryRegistry::instance()->get(keys.at(0));
        m_histogram = new KisHistogram(m_dev, m_dev->exactBounds(), hpf->generate(), LINEAR);
    }

    m_page->curveWidget->setCurve(m_curves[0]);
    connect(m_page->curveWidget, SIGNAL(modified()), this, SLOT(slotCurveModified()));

    {
        KisSignalsBlocker b(m_page->curveWidget);
        setActiveChannel(0);
    }
}

KisMultiChannelConfigWidget::~KisMultiChannelConfigWidget()
{
    KIS_ASSERT(m_histogram);
    delete m_histogram;
}

void KisMultiChannelConfigWidget::resetCurves()
{
    const KisPropertiesConfigurationSP &defaultConfiguration = getDefaultConfiguration();
    const auto *defaults = dynamic_cast<const KisMultiChannelFilterConfiguration*>(defaultConfiguration.data());

    KIS_SAFE_ASSERT_RECOVER_RETURN(defaults);
    m_curves = defaults->curves();

    const int virtualChannelCount = m_virtualChannels.size();
    for (int i = 0; i < virtualChannelCount; i++) {
        const VirtualChannelInfo &info = m_virtualChannels[i];
        m_curves[i].setName(info.name());
    }
}

void KisMultiChannelConfigWidget::setConfiguration(const KisPropertiesConfigurationSP config)
{
    const KisMultiChannelFilterConfiguration * cfg = dynamic_cast<const KisMultiChannelFilterConfiguration *>(config.data());
    if (!cfg) {
        return;
    }

    if (cfg->curves().empty()) {
        /**
         * HACK ALERT: our configuration factory generates
         * default configuration with nTransfers==0.
         * Catching it here. Just set everything to defaults instead.
         */
        const KisPropertiesConfigurationSP &defaultConfiguration = getDefaultConfiguration();
        const auto *defaults = dynamic_cast<const KisMultiChannelFilterConfiguration*>(defaultConfiguration.data());
        KIS_SAFE_ASSERT_RECOVER_RETURN(defaults);

        if (!defaults->curves().isEmpty()) {
            setConfiguration(defaultConfiguration);
            return;
        }
    } else if (cfg->curves().size() > m_virtualChannels.size()) {
        QMessageBox::warning(this, i18nc("@title:window", "Krita"), i18n("The current configuration was created for a different colorspace and cannot be used. All curves will be reset."));
        warnKrita << "WARNING: trying to load a curve with invalid number of channels!";
        warnKrita << "WARNING:   expected:" << m_virtualChannels.size();
        warnKrita << "WARNING:        got:" << cfg->curves().size();
        return;
    } else {
        if (cfg->curves().size() == m_virtualChannels.size()) {
            for (int ch = 0; ch < cfg->curves().size(); ch++) {
                m_curves[ch] = cfg->curves()[ch];
            }
        } else {
            // The configuration does not cover all our channels.
            // This happens when loading a document from an older version, which supported fewer channels.
            // Reset to make sure the unspecified channels have their default values.
            resetCurves();

            auto compareChannels =
                [] (const VirtualChannelInfo &lhs, const VirtualChannelInfo &rhs) -> bool {
                return lhs.type() == rhs.type() &&
                    (lhs.type() != VirtualChannelInfo::REAL || lhs.pixelIndex() == rhs.pixelIndex());
            };

            const KoColorSpace *targetColorSpace = m_dev->compositionSourceColorSpace();


            /**
             * Adjust the layout of channels in the configuration to the layout of the
             * current version of Krita. When we pass number of loaded channels
             * to getVirtualChannels() it automatically detects the version of Krita
             * the configuration was created in.
             */
            QVector<VirtualChannelInfo> detectedCurves = KisMultiChannelUtils::getVirtualChannels(targetColorSpace, cfg->curves().size());

            for (auto detectedIt = detectedCurves.begin(); detectedIt != detectedCurves.end(); ++detectedIt) {
                auto dstIt = std::find_if(m_virtualChannels.begin(), m_virtualChannels.end(),
                                          [=] (const VirtualChannelInfo &info) {
                                              return compareChannels(*detectedIt, info);
                                          });
                if (dstIt != m_virtualChannels.end()) {
                    const int srcIndex = std::distance(detectedCurves.begin(), detectedIt);
                    const int dstIndex = std::distance(m_virtualChannels.begin(), dstIt);
                    m_curves[dstIndex] = cfg->curves()[srcIndex];
                } else {
                    warnKrita << "WARNING: failed to find mapping of the channel in the filter configuration:";
                    warnKrita << "WARNING:   channel:" << ppVar(detectedIt->name()) << ppVar(detectedIt->type())<< ppVar(detectedIt->pixelIndex());
                    warnKrita << "WARNING:";

                    for (auto it = detectedCurves.begin(); it != detectedCurves.end(); ++it) {
                        warnKrita << "WARNING:   detected channels" << std::distance(detectedCurves.begin(), it) << ":" << it->name();
                    }

                    for (auto it = m_virtualChannels.begin(); it != m_virtualChannels.end(); ++it) {
                        warnKrita << "WARNING:   read channels" << std::distance(m_virtualChannels.begin(), it) << ":" << it->name();
                    }
                }
            }
        }
    }

    const int activeChannel =
        config->hasProperty("activeCurve") ?
        qBound(0, config->getInt("activeCurve"), m_curves.size()) :
        findDefaultVirtualChannelSelection();

    if (activeChannel == m_activeVChannel) {
        m_page->curveWidget->setCurve(m_curves[m_activeVChannel]);
    } else {
        setActiveChannel(activeChannel);
    }
}

int KisMultiChannelConfigWidget::findDefaultVirtualChannelSelection()
{
    return 0;
}

inline QPixmap KisMultiChannelConfigWidget::createGradient(Qt::Orientation orient /*, int invert (not used yet) */)
{
    int width;
    int height;
    int *i, inc, col;
    int x = 0, y = 0;

    if (orient == Qt::Horizontal) {
        i = &x; inc = 1; col = 0;
        width = 256; height = 1;
    } else {
        i = &y; inc = -1; col = 255;
        width = 1; height = 256;
    }

    QPixmap gradientpix(width, height);
    QPainter p(&gradientpix);
    p.setPen(QPen(QColor(0, 0, 0), 1, Qt::SolidLine));
    for (; *i < 256; (*i)++, col += inc) {
        p.setPen(QColor(col, col, col));
        p.drawPoint(x, y);
    }
    return gradientpix;
}

inline QPixmap KisMultiChannelConfigWidget::getHistogram()
{
    int i;
    int height = 256;
    QPixmap pix(256, height);
    KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(m_histogram, pix);


    bool logarithmic = m_page->chkLogarithmic->isChecked();

    if (logarithmic)
        m_histogram->setHistogramType(LOGARITHMIC);
    else
        m_histogram->setHistogramType(LINEAR);


    QPalette appPalette = QApplication::palette();

    pix.fill(QColor(appPalette.color(QPalette::Base)));

    QPainter p(&pix);
    p.setPen(QColor(appPalette.color(QPalette::Text)));
    p.save();
    p.setOpacity(0.2);

    const VirtualChannelInfo &info = m_virtualChannels[m_activeVChannel];


    if (info.type() == VirtualChannelInfo::REAL) {
        m_histogram->setChannel(info.pixelIndex());

        double highest = (double)m_histogram->calculations().getHighest();

        qint32 bins = m_histogram->producer()->numberOfBins();

        if (m_histogram->getHistogramType() == LINEAR) {
            double factor = (double)height / highest;
            for (i = 0; i < bins; ++i) {
                p.drawLine(i, height, i, height - int(m_histogram->getValue(i) * factor));
            }
        } else {
            double factor = (double)height / (double)log(highest);
            for (i = 0; i < bins; ++i) {
                p.drawLine(i, height, i, height - int(log((double)m_histogram->getValue(i)) * factor));
            }
        }
    }

    p.restore();

    return pix;
}

void KisMultiChannelConfigWidget::slotChannelSelected(int index)
{
    const int virtualChannel = m_page->cmbChannel->itemData(index).toInt();
    setActiveChannel(virtualChannel);
}

void KisMultiChannelConfigWidget::slotCurveModified()
{
    if (m_activeVChannel >= 0) {
        KIS_SAFE_ASSERT_RECOVER_RETURN(m_activeVChannel < m_curves.size());
        m_curves[m_activeVChannel] = m_page->curveWidget->curve();
    }
    Q_EMIT sigConfigurationItemChanged();
}

void KisMultiChannelConfigWidget::setActiveChannel(int ch)
{
    if (ch == m_activeVChannel) return;
    KIS_SAFE_ASSERT_RECOVER_RETURN(ch >= 0);
    KIS_SAFE_ASSERT_RECOVER_RETURN(ch < m_curves.size());

    m_activeVChannel = ch;
    m_page->curveWidget->setCurve(m_curves[m_activeVChannel]);
    m_page->curveWidget->setPixmap(getHistogram());

    const int index = m_page->cmbChannel->findData(m_activeVChannel);
    m_page->cmbChannel->setCurrentIndex(index);

    updateChannelControls();
}

void KisMultiChannelConfigWidget::logHistView()
{
    m_page->curveWidget->setPixmap(getHistogram());
}

void KisMultiChannelConfigWidget::resetCurve()
{
    const KisPropertiesConfigurationSP &defaultConfiguration = getDefaultConfiguration();
    const auto *defaults = dynamic_cast<const KisMultiChannelFilterConfiguration*>(defaultConfiguration.data());
    KIS_SAFE_ASSERT_RECOVER_RETURN(defaults);

    auto defaultCurves = defaults->curves();
    KIS_SAFE_ASSERT_RECOVER_RETURN(defaultCurves.size() > m_activeVChannel);

    m_page->curveWidget->setCurve(defaultCurves[m_activeVChannel]);
}
