/*
 *  SPDX-FileCopyrightText: 2025 Wolthera van Hövell tot Westerflier <griffinvalley@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
import QtQuick 2.0
import QtQuick.Controls 2.0
import QtQuick.Layouts 1.12
import org.krita.flake.text 1.0

/**
  CSS font-kerning is formally a three-way state between auto, normal and none. SVG 1.1 had it as an
  auto-length, but this has been deprecated in SVG 2.0. Because of this, we only really support auto and none.
  */
TextPropertyBase {
    propertyName: i18nc("@label", "Font Kerning");
    propertyType: TextPropertyBase.Character;
    toolTip: i18nc("@info:tooltip",
                   "Turn font kerning on or off. Font kerning enables per-glyph spacing adjustments.");
    searchTerms: i18nc("comma separated search terms for the font-kerning property, matching is case-insensitive",
                       "font-kerning, tracking");

    property alias kerning: fontKerningCbx.checked;
    onPropertiesUpdated: {
        blockSignals = true;
        kerning = properties.fontKerning;
        visible = properties.fontKerningState !== KoSvgTextPropertiesModel.PropertyUnset;
        blockSignals = false;
    }

    onKerningChanged: {
        if (!blockSignals) {
            properties.fontKerning = kerning;
        }
    }

    onEnableProperty: properties.fontKerningState = KoSvgTextPropertiesModel.PropertySet;
    GridLayout {
        columns: 2;
        columnSpacing: columnSpacing;
        width: parent.width;

        RevertPropertyButton {
            revertState: properties.fontKerningState;
            onClicked: properties.fontKerningState = KoSvgTextPropertiesModel.PropertyUnset;
        }

        CheckBox {
            id: fontKerningCbx;
            text: propertyName;
            Layout.fillWidth: true;
        }
    }
}
