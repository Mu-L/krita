/*
 *  SPDX-FileCopyrightText: 2007 Adrian Page <adrian@pagenet.plus.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KIS_OPENGL_H_
#define KIS_OPENGL_H_

/** @file */

#include <QtGlobal>
#include <QFlags>

#include <QSurfaceFormat>
#include <QStringList>

#include "kis_config.h"
#include <KisQStringListFwd.h>

#include "kritaui_export.h"

class QOpenGLContext;
class QString;
class QSurfaceFormat;

/**
 * This class manages a shared OpenGL context and provides utility
 * functions for checking capabilities and error reporting.
 */
class KRITAUI_EXPORT KisOpenGL
{
public:
    enum FilterMode {
        NearestFilterMode,  // nearest
        BilinearFilterMode, // linear, no mipmap
        TrilinearFilterMode, // LINEAR_MIPMAP_LINEAR
        HighQualityFiltering // Mipmaps + custom shader
    };

    enum OpenGLRenderer {
        RendererNone = 0x00,
        RendererAuto = 0x01,
        RendererDesktopGL = 0x02,
        RendererOpenGLES = 0x04,
        RendererSoftware = 0x08
    };
    Q_DECLARE_FLAGS(OpenGLRenderers, OpenGLRenderer)

    enum AngleRenderer {
        AngleRendererDefault    = 0x0000,
        AngleRendererD3d11      = 0x0002,
        AngleRendererD3d9       = 0x0004,
        AngleRendererD3d11Warp  = 0x0008, // "Windows Advanced Rasterization Platform"
    };

    struct KRITAUI_EXPORT RendererConfig {
        QSurfaceFormat format;
        AngleRenderer angleRenderer = AngleRendererDefault;

        OpenGLRenderer rendererId() const;
    };

public:
    static RendererConfig selectSurfaceConfig(KisOpenGL::OpenGLRenderer preferredRenderer,
                                              KisConfig::RootSurfaceFormat preferredRootSurfaceFormat,
                                              bool enableDebug);

    static void setDefaultSurfaceConfig(const RendererConfig &config);

    static OpenGLRenderer getCurrentOpenGLRenderer();
    static OpenGLRenderer getQtPreferredOpenGLRenderer();
    static OpenGLRenderers getSupportedOpenGLRenderers();
    static OpenGLRenderer getUserPreferredOpenGLRendererConfig();
    static void setUserPreferredOpenGLRendererConfig(OpenGLRenderer renderer);
    static QString convertOpenGLRendererToConfig(OpenGLRenderer renderer);
    static OpenGLRenderer convertConfigToOpenGLRenderer(QString renderer);

    /// Request OpenGL version 3.2
    static void initialize();

    /// Initialize shared OpenGL context
    static void initializeContext(QOpenGLContext *ctx);

    static const QString &getDebugText();

    static QStringList getOpenGLWarnings();

    static QString currentDriver();
    static bool supportsLoD();
    static bool hasOpenGL3();
    static bool hasOpenGLES();
    static bool supportsVAO();

    /// Check for OpenGL
    static bool hasOpenGL();

    /**
     * @brief supportsFilter
     * @return True if OpenGL provides fence sync methods.
     */
    static bool supportsFenceSync();

    static bool supportsBufferMapping();

    static bool forceDisableTextureBuffers();
    static bool shouldUseTextureBuffers(bool userPreference);

    static bool useTextureBufferInvalidation();

    /**
     * @brief supportsRenderToFBO
     * @return True if OpenGL can render to FBO, used
     * currently for rendering cursor with image overlay
     * fx.
     */
    static bool useFBOForToolOutlineRendering();

    /**
     * Returns true if we have a driver that has bugged support to sync objects (a fence)
     * and false otherwise.
     */
    static bool needsFenceWorkaround();

    static void testingInitializeDefaultSurfaceFormat();
    static void setDebugSynchronous(bool value);

    static void glInvalidateBufferData(uint buffer);

private:
    static void fakeInitWindowsOpenGL(KisOpenGL::OpenGLRenderers supportedRenderers, KisOpenGL::OpenGLRenderer preferredByQt);

    KisOpenGL();


};

#ifdef Q_OS_WIN
Q_DECLARE_OPERATORS_FOR_FLAGS(KisOpenGL::OpenGLRenderers);
#endif

#endif // KIS_OPENGL_H_
