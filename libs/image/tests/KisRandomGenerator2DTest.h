/*
 *  SPDX-FileCopyrightText: 2007, 2009 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_RANDOM_GENERATOR_2D_TEST_H
#define KIS_RANDOM_GENERATOR_2D_TEST_H

#include <simpletest.h>

class KisRandomGenerator2DTest : public QObject
{
    Q_OBJECT


private Q_SLOTS:

    void testEvolution();
    void twoSeeds();
    void twoCalls();
    void testConstantness();

private:

    void twoCalls(quint64 seed);
    void testConstantness(quint64 seed);
    void twoSeeds(quint64 seed1, quint64 seed2);

};

#endif
