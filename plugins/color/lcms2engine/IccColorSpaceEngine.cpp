/*
 *  SPDX-FileCopyrightText: 2007-2008 Cyrille Berger <cberger@cberger.net>
 *  SPDX-FileCopyrightText: 2011 Srikanth Tiyyagura <srikanth.tulasiram@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "IccColorSpaceEngine.h"

#include <klocalizedstring.h>

#include <KoColorModelStandardIds.h>
#include <kis_assert.h>

#include "LcmsColorSpace.h"

// -- KoLcmsColorConversionTransformation --

class KoLcmsColorConversionTransformation : public KoColorConversionTransformation
{
public:
    KoLcmsColorConversionTransformation(const KoColorSpace *srcCs, quint32 srcColorSpaceType, LcmsColorProfileContainer *srcProfile,
                                        const KoColorSpace *dstCs, quint32 dstColorSpaceType, LcmsColorProfileContainer *dstProfile,
                                        Intent renderingIntent,
                                        ConversionFlags conversionFlags)
        : KoColorConversionTransformation(srcCs, dstCs, renderingIntent, conversionFlags)
        , m_transform(0)
    {
        Q_ASSERT(srcCs);
        Q_ASSERT(dstCs);
        Q_ASSERT(renderingIntent < 4);

        if ((srcProfile->isLinear() || dstProfile->isLinear()) &&
            !conversionFlags.testFlag(KoColorConversionTransformation::NoOptimization)) {

            conversionFlags |= KoColorConversionTransformation::NoOptimization;
        }
        conversionFlags |= KoColorConversionTransformation::CopyAlpha;

        m_transform = cmsCreateTransform(srcProfile->lcmsProfile(),
                                         srcColorSpaceType,
                                         dstProfile->lcmsProfile(),
                                         dstColorSpaceType,
                                         renderingIntent,
                                         conversionFlags);

        Q_ASSERT(m_transform);
    }

    ~KoLcmsColorConversionTransformation() override
    {
        cmsDeleteTransform(m_transform);
    }

public:

    void transform(const quint8 *src, quint8 *dst, qint32 numPixels) const override
    {
        Q_ASSERT(m_transform);

        cmsDoTransform(m_transform, const_cast<quint8 *>(src), dst, numPixels);

    }
private:
    mutable cmsHTRANSFORM m_transform;
};

class KoLcmsColorProofingConversionTransformation : public KoColorProofingConversionTransformation
{
public:
    KoLcmsColorProofingConversionTransformation(const KoColorSpace *srcCs, quint32 srcColorSpaceType, LcmsColorProfileContainer *srcProfile,
                                                const KoColorSpace *dstCs, quint32 dstColorSpaceType, LcmsColorProfileContainer *dstProfile,
                                                const KoColorSpace *proofingSpace,
                                                Intent renderingIntent,
                                                Intent proofingIntent,
                                                bool bpcFirstTransform,
                                                quint8 *gamutWarning,
                                                double adaptationState,
                                                ConversionFlags displayConversionFlags
                                                )
        : KoColorProofingConversionTransformation(srcCs, dstCs, proofingSpace, renderingIntent, proofingIntent, bpcFirstTransform, gamutWarning, adaptationState, displayConversionFlags)
        , m_transform(0)
    {
        Q_ASSERT(srcCs);
        Q_ASSERT(dstCs);
        Q_ASSERT(renderingIntent < 4);

        bool doBPC1 = bpcFirstTransform;
        bool doBPC2 = displayConversionFlags.testFlag(KoColorConversionTransformation::BlackpointCompensation);

        if ((srcProfile->isLinear() || dstProfile->isLinear()) &&
            !displayConversionFlags.testFlag(KoColorConversionTransformation::NoOptimization)) {
            displayConversionFlags |= KoColorConversionTransformation::NoOptimization;
        }
        displayConversionFlags |= KoColorConversionTransformation::CopyAlpha;

        quint16 alarm[cmsMAXCHANNELS];//this seems to be bgr???
        alarm[0] = (cmsUInt16Number)gamutWarning[2]*256;
        alarm[1] = (cmsUInt16Number)gamutWarning[1]*256;
        alarm[2] = (cmsUInt16Number)gamutWarning[0]*256;
        cmsSetAlarmCodes(alarm);

        KIS_ASSERT(dynamic_cast<const IccColorProfile *>(proofingSpace->profile()));

        // This more or less does the same thing as cmsCreateProofingTransform in LCMS' cmsxform.c file,
        // except we try to allow enabling blackpoint compentation on the second bpc too.
        cmsHPROFILE proof = dynamic_cast<const IccColorProfile *>(proofingSpace->profile())->asLcms()->lcmsProfile();
        cmsHPROFILE profiles[] = {srcProfile->lcmsProfile(),
                                  proof,
                                  proof,
                                  dstProfile->lcmsProfile()};
        cmsBool bpc[] = {doBPC1, doBPC1, doBPC2, doBPC2};
        // Note that of the two transforms that create the proofing transform, the proofing intent is the second intent, not the first!
        cmsUInt32Number intents[] = {renderingIntent, renderingIntent, INTENT_RELATIVE_COLORIMETRIC, proofingIntent};
        cmsFloat64Number adaptation[] = {adaptationState, adaptationState, adaptationState, adaptationState};
        m_transform = cmsCreateExtendedTransform(cmsGetProfileContextID(srcProfile->lcmsProfile()), 4, profiles, bpc, intents, adaptation, proof, 1, srcColorSpaceType, dstColorSpaceType, displayConversionFlags);

        Q_ASSERT(m_transform);
    }

    ~KoLcmsColorProofingConversionTransformation() override
    {
        cmsDeleteTransform(m_transform);
    }

public:

    void transform(const quint8 *src, quint8 *dst, qint32 numPixels) const override
    {
        Q_ASSERT(m_transform);

        cmsDoTransform(m_transform, const_cast<quint8 *>(src), dst, numPixels);

    }
private:
    mutable cmsHTRANSFORM m_transform;
};

struct IccColorSpaceEngine::Private {
};

IccColorSpaceEngine::IccColorSpaceEngine() : KoColorSpaceEngine("icc", i18n("ICC Engine")), d(new Private)
{
}

IccColorSpaceEngine::~IccColorSpaceEngine()
{
    delete d;
}

const KoColorProfile* IccColorSpaceEngine::addProfile(const QString &filename)
{
    KoColorSpaceRegistry *registry = KoColorSpaceRegistry::instance();

    KoColorProfile *profile = new IccColorProfile(filename);
    Q_CHECK_PTR(profile);

    // this our own loading code; sometimes it fails because of an lcms error
    profile->load();

    // and then lcms can read the profile from file itself without problems,
    // quite often, and we can initialize it
    if (!profile->valid()) {
        cmsHPROFILE cmsp = cmsOpenProfileFromFile(filename.toLatin1(), "r");
        if (cmsp) {
            profile = LcmsColorProfileContainer::createFromLcmsProfile(cmsp);
        }
    }

    if (profile->valid()) {
        dbgPigment << "Valid profile : " << profile->fileName() << profile->name();
        registry->addProfile(profile);
    } else {
        dbgPigment << "Invalid profile : " << profile->fileName() << profile->name();
        delete profile;
        profile = 0;
    }

    return profile;
}

const KoColorProfile* IccColorSpaceEngine::addProfile(const QByteArray &data)
{
    KoColorSpaceRegistry *registry = KoColorSpaceRegistry::instance();

    KoColorProfile *profile = new IccColorProfile(data);
    Q_CHECK_PTR(profile);

    if (profile->valid()) {
        dbgPigment << "Valid profile : " << profile->fileName() << profile->name();
        registry->addProfile(profile);
    } else {
        dbgPigment << "Invalid profile : " << profile->fileName() << profile->name();
        delete profile;
        profile = 0;
    }

    return profile;
}

const KoColorProfile *IccColorSpaceEngine::getProfile(const QVector<double> &colorants, ColorPrimaries colorPrimaries, TransferCharacteristics transferFunction)
{
    KoColorSpaceRegistry *registry = KoColorSpaceRegistry::instance();

    KIS_SAFE_ASSERT_RECOVER(
        (!colorants.isEmpty() || colorPrimaries != PRIMARIES_UNSPECIFIED)
        && transferFunction != TRC_UNSPECIFIED)
    {
        if (transferFunction == TRC_LINEAR) {
            colorPrimaries = PRIMARIES_ITU_R_BT_2020_2_AND_2100_0;
        } else {
            colorPrimaries = PRIMARIES_ITU_R_BT_709_5;
        }

        if (transferFunction == TRC_UNSPECIFIED) {
            transferFunction = TRC_IEC_61966_2_1;
        }
    }

    const KoColorProfile *profile = new IccColorProfile(colorants, colorPrimaries, transferFunction);
    Q_CHECK_PTR(profile);

    if (profile->valid()) {
        dbgPigment << "Valid profile : " << profile->fileName() << profile->name();
        registry->addProfile(profile);
    } else {
        dbgPigment << "Invalid profile : " << profile->fileName() << profile->name();
        delete profile;
        profile = nullptr;
    }

    return profile;
}

void IccColorSpaceEngine::removeProfile(const QString &filename)
{
    KoColorSpaceRegistry *registry = KoColorSpaceRegistry::instance();

    KoColorProfile *profile = new IccColorProfile(filename);
    Q_CHECK_PTR(profile);
    profile->load();

    if (profile->valid() && registry->profileByName(profile->name())) {
        registry->removeProfile(profile);
    }
}

KoColorConversionTransformation *IccColorSpaceEngine::createColorTransformation(const KoColorSpace *srcColorSpace,
                                                                                const KoColorSpace *dstColorSpace,
                                                                                KoColorConversionTransformation::Intent renderingIntent,
                                                                                KoColorConversionTransformation::ConversionFlags conversionFlags) const
{
    KIS_ASSERT(srcColorSpace);
    KIS_ASSERT(dstColorSpace);
    KIS_ASSERT(dynamic_cast<const IccColorProfile *>(srcColorSpace->profile()));
    KIS_ASSERT(dynamic_cast<const IccColorProfile *>(dstColorSpace->profile()));

    return new KoLcmsColorConversionTransformation(
                srcColorSpace, computeColorSpaceType(srcColorSpace),
                dynamic_cast<const IccColorProfile *>(srcColorSpace->profile())->asLcms(), dstColorSpace, computeColorSpaceType(dstColorSpace),
                dynamic_cast<const IccColorProfile *>(dstColorSpace->profile())->asLcms(), renderingIntent, conversionFlags);

}
KoColorProofingConversionTransformation *IccColorSpaceEngine::createColorProofingTransformation(const KoColorSpace *srcColorSpace,
                                                                                                const KoColorSpace *dstColorSpace,
                                                                                                const KoColorSpace *proofingSpace,
                                                                                                KoColorConversionTransformation::Intent renderingIntent,
                                                                                                KoColorConversionTransformation::Intent proofingIntent,
                                                                                                bool firstTransformBPC,
                                                                                                quint8 *gamutWarning,
                                                                                                double adaptationState,
                                                                                                KoColorConversionTransformation::ConversionFlags displayConversionFlags) const
{
    KIS_ASSERT(srcColorSpace);
    KIS_ASSERT(dstColorSpace);
    KIS_ASSERT(dynamic_cast<const IccColorProfile *>(srcColorSpace->profile()));
    KIS_ASSERT(dynamic_cast<const IccColorProfile *>(dstColorSpace->profile()));

    return new KoLcmsColorProofingConversionTransformation(
                srcColorSpace, computeColorSpaceType(srcColorSpace),
                dynamic_cast<const IccColorProfile *>(srcColorSpace->profile())->asLcms(), dstColorSpace, computeColorSpaceType(dstColorSpace),
                dynamic_cast<const IccColorProfile *>(dstColorSpace->profile())->asLcms(), proofingSpace, renderingIntent, proofingIntent, firstTransformBPC, gamutWarning,
                adaptationState, displayConversionFlags
                );
}

quint32 IccColorSpaceEngine::computeColorSpaceType(const KoColorSpace *cs) const
{
    Q_ASSERT(cs);

    if (const KoLcmsInfo *lcmsInfo = dynamic_cast<const KoLcmsInfo *>(cs)) {
        return lcmsInfo->colorSpaceType();
    } else {
        QString modelId = cs->colorModelId().id();
        QString depthId = cs->colorDepthId().id();
        // Compute the depth part of the type
        quint32 depthType;

        if (depthId == Integer8BitsColorDepthID.id()) {
            depthType = BYTES_SH(1);
        } else if (depthId == Integer16BitsColorDepthID.id()) {
            depthType = BYTES_SH(2);
        } else if (depthId == Float16BitsColorDepthID.id()) {
            depthType = BYTES_SH(2) | FLOAT_SH(1);
        } else if (depthId == Float32BitsColorDepthID.id()) {
            depthType = BYTES_SH(4) | FLOAT_SH(1);
        } else if (depthId == Float64BitsColorDepthID.id()) {
            depthType = BYTES_SH(0) | FLOAT_SH(1);
        } else {
            qWarning() << "Unknown bit depth";
            return 0;
        }
        // Compute the model part of the type
        quint32 modelType = 0;

        if (modelId == RGBAColorModelID.id()) {
            if (depthId.startsWith(QLatin1Char('U'))) {
                modelType = (COLORSPACE_SH(PT_RGB) | EXTRA_SH(1) | CHANNELS_SH(3) | DOSWAP_SH(1) | SWAPFIRST_SH(1));
            } else if (depthId.startsWith(QLatin1Char('F'))) {
                modelType = (COLORSPACE_SH(PT_RGB) | EXTRA_SH(1) | CHANNELS_SH(3));
            }
        } else if (modelId == XYZAColorModelID.id()) {
            modelType = (COLORSPACE_SH(PT_XYZ) | EXTRA_SH(1) | CHANNELS_SH(3));
        } else if (modelId == LABAColorModelID.id()) {
            modelType = (COLORSPACE_SH(PT_Lab) | EXTRA_SH(1) | CHANNELS_SH(3));
        } else if (modelId == CMYKAColorModelID.id()) {
            modelType = (COLORSPACE_SH(PT_CMYK) | EXTRA_SH(1) | CHANNELS_SH(4));
        } else if (modelId == GrayAColorModelID.id()) {
            modelType = (COLORSPACE_SH(PT_GRAY) | EXTRA_SH(1) | CHANNELS_SH(1));
        } else if (modelId == GrayColorModelID.id()) {
            modelType = (COLORSPACE_SH(PT_GRAY) | CHANNELS_SH(1));
        } else if (modelId == YCbCrAColorModelID.id()) {
            modelType = (COLORSPACE_SH(PT_YCbCr) | EXTRA_SH(1) | CHANNELS_SH(3));
        } else {
            qWarning() << "Cannot convert colorspace to lcms modeltype";
            return 0;
        }
        return depthType | modelType;
    }
}

bool IccColorSpaceEngine::supportsColorSpace(const QString &colorModelId, const QString &colorDepthId, const KoColorProfile *profile) const
{
    Q_UNUSED(colorDepthId);
    return colorModelId != RGBAColorModelID.id() || !profile || profile->name() != "High Dynamic Range UHDTV Wide Color Gamut Display (Rec. 2020) - SMPTE ST 2084 PQ EOTF";
}
