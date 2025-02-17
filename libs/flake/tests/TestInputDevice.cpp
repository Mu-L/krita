/*
 *  SPDX-FileCopyrightText: 2011 Hanna Skott <hannaetscott@gmail.com>
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "TestInputDevice.h"

#include <simpletest.h>
#include "KoInputDevice.h"

//Tests so the KoInputDevice created is of tablet type
void TestInputDevice::testTabletConstructor()
{
    KoInputDevice::InputDevice parameterTabletDevice = KoInputDevice::InputDevice::Stylus;
    KoInputDevice::Pointer parameterPointerType = KoInputDevice::Pointer::Eraser;
    qint64 parameterUniqueTabletId = 0;
    KoInputDevice toTest(parameterTabletDevice, parameterPointerType, parameterUniqueTabletId);
    bool isMouse = toTest.isMouse();
    QVERIFY(!isMouse);
}
//Tests so the unique tablet ID of the default constructor is -1 like set.
void TestInputDevice::testNoParameterConstructor()
{
    KoInputDevice toTest;
    bool isMouse = toTest.isMouse();
    qint64 theUniqueTabletId = toTest.uniqueTabletId();
    qint64 toCompareWith = -1;
    QVERIFY(isMouse);
    QCOMPARE(theUniqueTabletId, toCompareWith);
}
//Tests so the single reference constructor can take both a tablet and a mouse as input device and set it correctly
void TestInputDevice::testConstructorWithSingleReference()
{
    KoInputDevice::InputDevice parameterTabletDevice = KoInputDevice::InputDevice::Stylus;
    KoInputDevice::Pointer parameterPointerType = KoInputDevice::Pointer::Eraser;
    qint64 parameterUniqueTabletId = 0;
    KoInputDevice parameterTabletDevice2(parameterTabletDevice, parameterPointerType, parameterUniqueTabletId);
    KoInputDevice toTest(parameterTabletDevice2);
    bool isMouse = toTest.isMouse();
    QVERIFY(!isMouse);
    KoInputDevice parameterMouseDevice;
    toTest = KoInputDevice(parameterMouseDevice);
    isMouse = toTest.isMouse();
    QVERIFY(isMouse);
}
//tests so the equality operator is working
void TestInputDevice::testEqualityCheckOperator()
{
    KoInputDevice::InputDevice parameterTabletDevice = KoInputDevice::InputDevice::Stylus;
    KoInputDevice::Pointer parameterPointerType = KoInputDevice::Pointer::Eraser;
    qint64 parameterUniqueTabletId = 0;
    KoInputDevice parameterTabletDevice2(parameterTabletDevice, parameterPointerType, parameterUniqueTabletId);
    KoInputDevice toTest(parameterTabletDevice2);
    KoInputDevice copy = toTest;
    KoInputDevice notSame;
    QVERIFY(toTest==copy);
    QVERIFY(toTest!=notSame);
}
//tests that the device is set properly in the constructor 
void TestInputDevice::testDevice()
{
    KoInputDevice::InputDevice parameterTabletDevice = KoInputDevice::InputDevice::Stylus;
    KoInputDevice::Pointer parameterPointerType = KoInputDevice::Pointer::Eraser; ;
    qint64 parameterUniqueTabletId = 0;
    KoInputDevice parameterTabletDevice2(parameterTabletDevice, parameterPointerType, parameterUniqueTabletId);
    KoInputDevice toTest(parameterTabletDevice2);
    KoInputDevice::InputDevice aTabletDevice = toTest.device();

    QCOMPARE(aTabletDevice, KoInputDevice::InputDevice::Stylus);
}
//tests that the PointerType is set correctly by the constructor
void TestInputDevice::testPointer()
{
    KoInputDevice::InputDevice parameterTabletDevice = KoInputDevice::InputDevice::Stylus;
    KoInputDevice::Pointer parameterPointerType = KoInputDevice::Pointer::Eraser;
    qint64 parameterUniqueTabletId = 0;
    KoInputDevice parameterTabletDevice2(parameterTabletDevice, parameterPointerType, parameterUniqueTabletId);
    KoInputDevice toTest(parameterTabletDevice2);
    KoInputDevice::Pointer aTabletPointer= toTest.pointer();

    QCOMPARE(aTabletPointer, KoInputDevice::Pointer::Eraser);
}
//verifies that the device returned by the mouse() function is indeed a mouse device
void TestInputDevice::testMouse()
{
    KoInputDevice toTest = KoInputDevice::mouse();
    bool isMouse = toTest.isMouse();
    QVERIFY(isMouse);
}
//verifies that the device returned by the stylus() function is indeed a tablet device
void TestInputDevice::testStylus()
{
    KoInputDevice toTest = KoInputDevice::stylus();
    bool isMouse = toTest.isMouse();
    QVERIFY(!isMouse);
}
//verifies that the device returned by the eraser() function is indeed a tablet device
void TestInputDevice::testEraser()
{
    KoInputDevice toTest = KoInputDevice::eraser();
    bool isMouse = toTest.isMouse();
    QVERIFY(!isMouse);
}


SIMPLE_TEST_MAIN(TestInputDevice)
