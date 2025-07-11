/*
 *  SPDX-FileCopyrightText: 2020 Agata Cacko <cacko.azh@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __TEST_KO_ZOOM_ACTION_H
#define __TEST_KO_ZOOM_ACTION_H

#include <simpletest.h>

#include <KoZoomAction.h>

class TestKoZoomAction : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testZoomActionState_data();
    void testZoomActionState();
    void testInitWithDefault();
};

#endif /* __TEST_KO_ZOOM_ACTION_H */
