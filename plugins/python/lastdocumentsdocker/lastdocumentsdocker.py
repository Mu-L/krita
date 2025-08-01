# SPDX-License-Identifier: CC0-1.0

try:
    from PyQt6.QtWidgets import QWidget, QVBoxLayout, QListView, QPushButton
except:
    from PyQt5.QtWidgets import QWidget, QVBoxLayout, QListView, QPushButton
from krita import DockWidget
from builtins import i18n
from . import lastdocumentslistmodel


class LastDocumentsDocker(DockWidget):

    def __init__(self):
        super(LastDocumentsDocker, self).__init__()

        self.baseWidget = QWidget()
        self.layout = QVBoxLayout()
        self.listView = QListView()
        self.loadButton = QPushButton(i18n("Refresh"))
        self.listModel = lastdocumentslistmodel.LastDocumentsListModel(self.devicePixelRatioF())

        self.listView.setModel(self.listModel)
        self.listView.setFlow(QListView.Flow.LeftToRight)

        self.layout.addWidget(self.listView)
        self.layout.addWidget(self.loadButton)

        self.baseWidget.setLayout(self.layout)
        self.setWidget(self.baseWidget)

        self.loadButton.clicked.connect(self.refreshRecentDocuments)
        self.setWindowTitle(i18n("Last Documents Docker"))

    def canvasChanged(self, canvas):
        pass

    def refreshRecentDocuments(self):
        self.listModel.loadRecentDocuments()
