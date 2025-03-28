/*
 *  SPDX-FileCopyrightText: 2024 Wolthera van Hövell tot Westerflier <griffinvalley@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "TabSizeModel.h"

namespace  {

auto getValue =  lager::lenses::getset (
            [] (const KoSvgText::TabSizeInfo &data) -> qreal {
    if (data.isNumber) {
        return data.value;
    } else {
        return data.length.value;
    }
}, [] (KoSvgText::TabSizeInfo data, const qreal &val) -> KoSvgText::TabSizeInfo {
        if (data.isNumber) {
        data.value = val;
        } else {
            data.length.value = val;
        }
        return data;
});

auto getUnit =  lager::lenses::getset (
            [] (const KoSvgText::TabSizeInfo &data) -> TabSizeModel::TabSizeType {
    if (data.isNumber) {
        return TabSizeModel::TabSizeType::Spaces;
    } else {
        switch(data.length.unit) {
        case KoSvgText::CssLengthPercentage::Absolute:
            return TabSizeModel::TabSizeType::Absolute;
        case KoSvgText::CssLengthPercentage::Em:
            return TabSizeModel::TabSizeType::Em;
        case KoSvgText::CssLengthPercentage::Ex:
            return TabSizeModel::TabSizeType::Ex;
        case KoSvgText::CssLengthPercentage::Cap:
            return TabSizeModel::TabSizeType::Cap;
        case KoSvgText::CssLengthPercentage::Ch:
            return TabSizeModel::TabSizeType::Ch;
        case KoSvgText::CssLengthPercentage::Ic:
            return TabSizeModel::TabSizeType::Ic;
        case KoSvgText::CssLengthPercentage::Lh:
            return TabSizeModel::TabSizeType::Lh;
        default:
            break;
        }
        return TabSizeModel::TabSizeType::Absolute;
    }
}, [] (KoSvgText::TabSizeInfo data, const TabSizeModel::TabSizeType &val) -> KoSvgText::TabSizeInfo {
        if (val == TabSizeModel::TabSizeType::Spaces) {
            data.isNumber = true;
        } else {
            data.isNumber = false;
        switch(val) {
        case TabSizeModel::TabSizeType::Absolute:
            data.length.unit = KoSvgText::CssLengthPercentage::Absolute;
        break;
        case TabSizeModel::TabSizeType::Em:
            data.length.unit = KoSvgText::CssLengthPercentage::Em;
        break;
        case TabSizeModel::TabSizeType::Ex:
            data.length.unit = KoSvgText::CssLengthPercentage::Ex;
        break;
        case TabSizeModel::TabSizeType::Cap:
            data.length.unit = KoSvgText::CssLengthPercentage::Cap;
        break;
        case TabSizeModel::TabSizeType::Ch:
            data.length.unit = KoSvgText::CssLengthPercentage::Ch;
        break;
        case TabSizeModel::TabSizeType::Ic:
            data.length.unit = KoSvgText::CssLengthPercentage::Ic;
        break;
        case TabSizeModel::TabSizeType::Lh:
            data.length.unit = KoSvgText::CssLengthPercentage::Lh;
        break;
        default:
        break;
        }
        }
        return data;

});
}

TabSizeModel::TabSizeModel(lager::cursor<KoSvgText::TabSizeInfo> _data)
    : data(_data)
    , LAGER_QT(value) {data.zoom(getValue)}
    , LAGER_QT(unit) {data.zoom(getUnit)}
{

}
