/* Copyright (C) 2023 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.0
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.3

RowLayout {
    id: root

    property alias button: _button
    property alias icon: _icon.icon
    property alias controlEnabled: row.enabled
    property alias combo: _combo
    property alias frame: _frame
    property string comboToolTip: ""
    property string buttonToolTip: ""
    readonly property bool off: combo.model.length === 0
    property string comboPlaceholderText: ""
    property bool verticalMode: false
    property bool comboFillWidth: true
    property int comboPrefWidth: _combo.implicitWidth

    Layout.fillWidth: verticalMode ? true : comboFillWidth

    Frame {
        id: _frame

        Layout.fillWidth: true
        background: Item {}

        topPadding: 2
        bottomPadding: 2

        RowLayout {
            id: row

            anchors.fill: parent

            ToolButton {
                id: _icon
                hoverEnabled: false
                down: false

                Layout.alignment: Qt.AlignVCenter
                visible: icon.name.length !== 0
                enabled: !root.off
            }

            ComboBox {
                id: _combo

                Layout.fillWidth: verticalMode ? true : root.comboFillWidth
                Layout.preferredWidth: root.verticalMode ? implicitWidth : root.comboPrefWidth
                Layout.alignment: Qt.AlignVCenter
                enabled: !root.off
                displayText: root.off ? root.comboPlaceholderText : currentText
                ToolTip.visible: hovered
                ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
                ToolTip.text: root.comboToolTip
            }

            Button {
                id: _button

                visible: text.length !== 0
                enabled: !root.off

                ToolTip.visible: root.buttonToolTip.length !== 0 && hovered
                ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
                ToolTip.text: root.buttonToolTip

                Layout.alignment: Qt.AlignVCenter
            }
        }
    }
}
