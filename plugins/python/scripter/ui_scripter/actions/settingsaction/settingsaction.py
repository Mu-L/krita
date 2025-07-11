"""
SPDX-FileCopyrightText: 2017 Eliakin Costa <eliakim170@gmail.com>

SPDX-License-Identifier: GPL-2.0-or-later
"""
try:
    from PyQt6.QtGui import QAction
    from PyQt6.QtCore import Qt
except:
    from PyQt5.QtWidgets import QAction
    from PyQt5.QtCore import Qt
from . import settingsdialog
from builtins import i18n


class SettingsAction(QAction):

    def __init__(self, scripter, parent=None):
        super(SettingsAction, self).__init__(parent)
        self.scripter = scripter

        self.triggered.connect(self.openSettings)

        self.settingsDialog = settingsdialog.SettingsDialog(self.scripter)
        self.settingsDialog.setWindowModality(Qt.WindowModality.WindowModal)
        self.settingsDialog.setFixedSize(400, 250)

        self.setText(i18n("Settings"))

    @property
    def parent(self):
        return 'File',

    def openSettings(self):
        self.settingsDialog.show()
        self.settingsDialog.exec()

    def readSettings(self):
        self.settingsDialog.readSettings(self.scripter.settings)

    def writeSettings(self):
        self.settingsDialog.writeSettings(self.scripter.settings)
