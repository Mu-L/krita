/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2011 Jan Hambrecht <jaham@gmx.net>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef SVGSHAPE_H
#define SVGSHAPE_H

#include "kritaflake_export.h"

class SvgSavingContext;
class SvgLoadingContext;
#include <QDomDocument>

/// An interface providing svg loading and saving routines
class KRITAFLAKE_EXPORT SvgShape
{
public:
    virtual ~SvgShape();

    /// Saves data utilizing specified svg saving context
    virtual bool saveSvg(SvgSavingContext &context);

    /// Loads data from specified svg element
    virtual bool loadSvg(const QDomElement &element, SvgLoadingContext &context);

    void saveMetadata(SvgSavingContext &context);
};

#endif // SVGSHAPE_H
