"""
SPDX-FileCopyrightText: 2017 Eliakin Costa <eliakim170@gmail.com>

SPDX-License-Identifier: GPL-2.0-or-later
"""
try:
    from PyQt6.QtGui import QKeySequence, QAction
    from PyQt6.QtCore import Qt
except:
    from PyQt5.QtWidgets import QAction
    from PyQt5.QtGui import QKeySequence
    from PyQt5.QtCore import Qt
from krita import FileDialog
from builtins import i18n


class SaveAction(QAction):

    def __init__(self, scripter, parent=None):
        super(SaveAction, self).__init__(parent)
        self.scripter = scripter
        self.editor = self.scripter.uicontroller.editor

        self.triggered.connect(self.save)

        self.setText(i18n("Save"))
        self.setObjectName('save')
        self.setShortcut(QKeySequence(Qt.Modifier.CTRL | Qt.Key.Key_S))

    @property
    def parent(self):
        return 'File',

    def save(self):
        text = self.editor.toPlainText()
        fileName = ''

        if not self.scripter.documentcontroller.activeDocument:
            fileName = FileDialog.getSaveFileName(self.scripter.uicontroller.mainWidget,
                                                  i18n("Save Python File"), '',
                                                  (i18n("Python Files") + " (*.py)"))
            if not fileName:
                return

            # don't validate file name - trust user to specify the extension they want
            # getSaveFileName will add ".py" if there is no extension.
            # It will strip a trailing period and, in each case,  test for file collisions

        document = self.scripter.documentcontroller.saveDocument(text, fileName)
        if document:
            self.scripter.uicontroller.setStatusBar(document.filePath)
        else:
            self.scripter.uicontroller.setStatusBar('untitled')
        self.editor._documentModified = False
        self.scripter.uicontroller.setStatusModified()
        return document
