/*
 *  SPDX-FileCopyrightText: 2010 Lukáš Tvrdý <lukast.dev@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <cmath>

#include <QDomDocument>

#include <kis_fast_math.h>
#include "kis_antialiasing_fade_maker.h"
#include "kis_brush_mask_applicator_factories.h"
#include "kis_brush_mask_applicator_base.h"

#include "kis_curve_rect_mask_generator.h"
#include "kis_curve_rect_mask_generator_p.h"
#include "kis_curve_circle_mask_generator.h"
#include "kis_cubic_curve.h"


KisCurveRectangleMaskGenerator::KisCurveRectangleMaskGenerator(qreal diameter, qreal ratio, qreal fh, qreal fv, int spikes, const KisCubicCurve &curve, bool antialiasEdges)
    : KisMaskGenerator(diameter, ratio, fh, fv, spikes, antialiasEdges, RECTANGLE, SoftId), d(new Private(antialiasEdges))
{
    d->curveResolution = qRound( qMax(width(),height()) * OVERSAMPLING);
    d->curveData = curve.floatTransfer( d->curveResolution + 1);
    d->curvePoints = curve.curvePoints();
    setCurveString(curve.toString());
    d->dirty = false;

    setScale(1.0, 1.0);

    d->applicator.reset(createOptimizedClass<MaskApplicatorFactory<KisCurveRectangleMaskGenerator>>(this));
}

KisCurveRectangleMaskGenerator::KisCurveRectangleMaskGenerator(const KisCurveRectangleMaskGenerator &rhs)
    : KisMaskGenerator(rhs),
      d(new Private(*rhs.d))
{
    d->applicator.reset(createOptimizedClass<MaskApplicatorFactory<KisCurveRectangleMaskGenerator>>(this));
}

KisMaskGenerator* KisCurveRectangleMaskGenerator::clone() const
{
    return new KisCurveRectangleMaskGenerator(*this);
}

void KisCurveRectangleMaskGenerator::setScale(qreal scaleX, qreal scaleY)
{
    KisMaskGenerator::setScale(scaleX, scaleY);

    qreal halfWidth = 0.5 * effectiveSrcWidth();
    qreal halfHeight = 0.5 * effectiveSrcHeight();

    d->xcoeff = 1.0 / halfWidth;
    d->ycoeff = 1.0 / halfHeight;

    d->fadeMaker.setLimits(halfWidth, halfHeight);
}

KisCurveRectangleMaskGenerator::~KisCurveRectangleMaskGenerator()
{
}

quint8 KisCurveRectangleMaskGenerator::Private::value(qreal xr, qreal yr) const
{
    xr = qAbs(xr) * xcoeff;
    yr = qAbs(yr) * ycoeff;

    int sIndex = qRound(xr * (curveResolution));
    int tIndex = qRound(yr * (curveResolution));

    int sIndexInverted = curveResolution - sIndex;
    int tIndexInverted = curveResolution - tIndex;

    qreal blend = (curveData.at(sIndex) * (1.0 - curveData.at(sIndexInverted)) *
                   curveData.at(tIndex) * (1.0 - curveData.at(tIndexInverted)));

    return (1.0 - blend) * 255;
}

quint8 KisCurveRectangleMaskGenerator::valueAt(qreal x, qreal y) const
{
    if (isEmpty()) return 255;
    qreal xr = x;
    qreal yr = qAbs(y);
    fixRotation(xr, yr);

    quint8 value;
    if (d->fadeMaker.needFade(xr, yr, &value)) {
        return value;
    }

    return d->value(xr, yr);
}

void KisCurveRectangleMaskGenerator::toXML(QDomDocument& doc, QDomElement& e) const
{
    KisMaskGenerator::toXML(doc, e);
    e.setAttribute("softness_curve", curveString());
}

void KisCurveRectangleMaskGenerator::setSoftness(qreal softness)
{
    // performance
    if (!d->dirty && softness == 1.0) return;
    d->dirty = true;
    KisMaskGenerator::setSoftness(softness);
    KisCurveCircleMaskGenerator::transformCurveForSoftness(softness,d->curvePoints, d->curveResolution + 1, d->curveData);
    d->dirty = false;
}

bool KisCurveRectangleMaskGenerator::shouldVectorize() const
{
    return !shouldSupersample() && spikes() == 2;
}

KisBrushMaskApplicatorBase *KisCurveRectangleMaskGenerator::applicator() const
{
    return d->applicator.data();
}

void KisCurveRectangleMaskGenerator::setMaskScalarApplicator()
{
    d->applicator.reset(
        createScalarClass<
            MaskApplicatorFactory<KisCurveRectangleMaskGenerator>>(this));
}

