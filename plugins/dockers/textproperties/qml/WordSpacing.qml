/*
 *  SPDX-FileCopyrightText: 2024 Wolthera van Hövell tot Westerflier <griffinvalley@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
import QtQuick 2.0
import QtQuick.Controls 2.0
import QtQuick.Layouts 1.12
import org.krita.flake.text 1.0
import org.krita.components 1.0

TextPropertyBase {
    propertyTitle: i18nc("@title:group", "Word Spacing");
    propertyName: "word-spacing";
    propertyType: TextPropertyConfigModel.Character;
    toolTip: i18nc("@info:tooltip",
                   "Word spacing controls the size of word-break characters, such as the space character.");
    searchTerms: i18nc("comma separated search terms for the word-spacing property, matching is case-insensitive",
                       "word-spacing, tracking, white space,");
    property alias wordSpacing: wordSpacingUnitCmb.dataValue;
    property alias wordSpacingUnit: wordSpacingUnitCmb.dataUnit;


    onPropertiesUpdated: {
        blockSignals = true;
        wordSpacingUnitCmb.dpi = canvasDPI;
        wordSpacingUnitCmb.setTextProperties(properties);
        wordSpacingUnitCmb.setDataValueAndUnit(properties.wordSpacing.value, properties.wordSpacing.unitType);

        propertyState = [properties.wordSpacingState];
        setVisibleFromProperty();
        blockSignals = false;
    }
    onWordSpacingChanged: {
        if (!blockSignals) {
            properties.wordSpacing.value = wordSpacing;
        }
    }

    onWordSpacingUnitChanged: {
        if (!blockSignals) {
            properties.wordSpacing.unitType = wordSpacingUnit;
        }
    }

    onEnableProperty: properties.wordSpacingState = KoSvgTextPropertiesModel.PropertySet;

    RowLayout {
        spacing: columnSpacing;
        width: parent.width;

        RevertPropertyButton {
            revertState: properties.wordSpacingState;
            onClicked: properties.wordSpacingState = KoSvgTextPropertiesModel.PropertyUnset;
        }

        Label {
            text: propertyTitle;
            elide: Text.ElideRight;
            Layout.fillWidth: true;
            font.italic: properties.wordSpacingState === KoSvgTextPropertiesModel.PropertyTriState;
        }

        DoubleSpinBox {
            id: wordSpacingSpn
            editable: true;
            Layout.fillWidth: true;
            from: -999 * multiplier;
            to: 999 * multiplier;

            onValueChanged: wordSpacingUnitCmb.userValue = value;
        }

        UnitComboBox {
            id: wordSpacingUnitCmb
            spinBoxControl: wordSpacingSpn;
            isFontSize: false;
            dpi: dpi;
            onUserValueChanged: wordSpacingSpn.value = userValue;
            Layout.preferredWidth: minimumUnitBoxWidth;
            Layout.maximumWidth: implicitWidth;
            allowPercentage: false; // CSS-Text-4 has percentages for word-spacing, but so does SVG 1.1, and they both are implemented differently.
        }
    }
}
