# SPDX-License-Identifier: CC0-1.0

try:
    from PyQt6.QtCore import Qt
    from PyQt6.QtWidgets import (QWidget, QVBoxLayout, QPushButton,
                                 QLineEdit, QScrollArea, QGridLayout,
                                 QLabel, QDialogButtonBox)
    from PyQt6.QtGui import QKeySequence
except:
    from PyQt5.QtCore import Qt
    from PyQt5.QtWidgets import (QWidget, QVBoxLayout, QPushButton,
                                 QLineEdit, QScrollArea, QGridLayout,
                                 QLabel, QDialogButtonBox)
    from PyQt5.QtGui import QKeySequence
from . import tenscriptsdialog
from krita import Krita, FileDialog
from builtins import i18n, Application

class UITenScripts(object):

    def __init__(self):
        self.kritaInstance = Krita.instance()
        self.mainDialog = tenscriptsdialog.TenScriptsDialog(
            self, self.kritaInstance.activeWindow().qwindow())

        self.buttonBox = QDialogButtonBox(self.mainDialog)
        self.layout = QVBoxLayout(self.mainDialog)
        self.baseWidget = QWidget()
        self.baseArea = QWidget()
        self.scrollArea = QScrollArea()
        self.scriptsLayout = QGridLayout()

        self.buttonBox.accepted.connect(self.mainDialog.accept)
        self.buttonBox.rejected.connect(self.mainDialog.reject)

        self.buttonBox.setOrientation(Qt.Orientation.Horizontal)
        self.buttonBox.setStandardButtons(
            QDialogButtonBox.StandardButton.Ok | QDialogButtonBox.StandardButton.Cancel)
        self.scrollArea.setWidgetResizable(True)

    def initialize(self, tenscripts):
        self.tenscripts = tenscripts

        self._loadGridLayout()
        self._fillScripts()

        self.baseArea.setLayout(self.scriptsLayout)
        self.scrollArea.setWidget(self.baseArea)

        self.layout.addWidget(self.scrollArea)

        self.layout.addWidget(
            QLabel(i18n("Shortcuts are configurable through the <i>Keyboard Shortcuts</i> "
                        "interface in Krita's settings.")))

        self.layout.addWidget(self.buttonBox)

        self.mainDialog.show()
        self.mainDialog.activateWindow()
        self.mainDialog.exec()

    def addNewRow(self, index):
        rowPosition = self.scriptsLayout.rowCount()
        label = QLabel()
        directoryTextField = QLineEdit()
        directoryDialogButton = QPushButton(i18n("..."))
        directoryClearButton = QPushButton(self.kritaInstance.icon('close-tab'), "")

        action = Application.action(self.tenscripts.indexToAction[index])

        directoryTextField.setReadOnly(True)
        label.setText(action.shortcut().toString(QKeySequence.SequenceFormat.NativeText))
        directoryTextField.setToolTip(i18n("Selected path"))
        directoryDialogButton.setToolTip(i18n("Select the script"))
        directoryDialogButton.clicked.connect(self._selectScript)
        directoryClearButton.clicked.connect(self._clearScript)

        self.scriptsLayout.addWidget(
            label, rowPosition, 0, Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignTop)
        self.scriptsLayout.addWidget(
            directoryTextField, rowPosition, 1, Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignTop)
        self.scriptsLayout.addWidget(
            directoryClearButton, rowPosition, 2, Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignTop)
        self.scriptsLayout.addWidget(
            directoryDialogButton, rowPosition, 3, Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignTop)

    def saved_scripts(self):
        _saved_scripts = []
        index = 0

        for _ in range(self.scriptsLayout.rowCount()-1):
            textField = self.scriptsLayout.itemAt(index + 1).widget()
            _saved_scripts.append(textField.text())
            index += 4

        return _saved_scripts

    def _clearScript(self):
        obj = self.mainDialog.sender()
        textField = self.scriptsLayout.itemAt(
            self.scriptsLayout.indexOf(obj)-1).widget()
        textField.setText("")

    def _selectScript(self):
        dialog = FileDialog(self.mainDialog)
        dialog.setNameFilter(i18n("Python files (*.py)"))
        selectedFile = dialog.filename()

        if selectedFile:
            obj = self.mainDialog.sender()
            textField = self.scriptsLayout.itemAt(
                self.scriptsLayout.indexOf(obj) - 2).widget()
            textField.setText(selectedFile)

    def _loadGridLayout(self):
        for index in range(0, 10):
            self.addNewRow(index)

    def _fillScripts(self):
        scripts = self.tenscripts.scripts
        num_rows = self.scriptsLayout.rowCount() - 1
        index = 0

        for row in range(min(len(scripts), num_rows)):
            textField = self.scriptsLayout.itemAt(index + 1).widget()
            textField.setText(scripts[row])
            index += 4
