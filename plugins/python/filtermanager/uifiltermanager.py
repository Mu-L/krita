# SPDX-License-Identifier: CC0-1.0

from . import filtermanagerdialog
from .components import (filtercombobox, filtermanagertreemodel)
try:
    from PyQt6.QtCore import Qt
    from PyQt6.QtWidgets import (QFormLayout, QAbstractItemView, QDialogButtonBox,
                                 QVBoxLayout, QFrame, QTreeView)
except:
    from PyQt5.QtCore import Qt
    from PyQt5.QtWidgets import (QFormLayout, QAbstractItemView, QDialogButtonBox,
                                 QVBoxLayout, QFrame, QTreeView)
from krita import Krita
from builtins import i18n, i18nc


class UIFilterManager(object):

    def __init__(self):
        self.mainDialog = filtermanagerdialog.FilterManagerDialog()
        self.mainLayout = QVBoxLayout(self.mainDialog)
        self.formLayout = QFormLayout()
        self.buttonBox = QDialogButtonBox(
            QDialogButtonBox.StandardButton.Ok | QDialogButtonBox.StandardButton.Cancel)

        self.kritaInstance = Krita.instance()
        self._filters = sorted(self.kritaInstance.filters())
        self._documents = self.kritaInstance.documents()
        self.treeModel = filtermanagertreemodel.FilterManagerTreeModel(self)

        self.documentsTreeView = QTreeView()
        self.filterComboBox = filtercombobox.FilterComboBox(self)

        self.buttonBox.accepted.connect(self.confirmButton)
        self.buttonBox.rejected.connect(self.mainDialog.close)

        self.documentsTreeView.setSelectionMode(
            QAbstractItemView.SelectionMode.SingleSelection)
        self.mainDialog.setWindowModality(Qt.WindowModality.NonModal)

    def initialize(self):
        self.documentsTreeView.setModel(self.treeModel)
        self.documentsTreeView.setWindowTitle(i18n("Document Tree Model"))
        self.documentsTreeView.resizeColumnToContents(0)
        self.documentsTreeView.resizeColumnToContents(1)
        self.documentsTreeView.resizeColumnToContents(2)

        self.formLayout.addRow(i18nc("Python filters", "Filters:"), self.filterComboBox)

        self.line = QFrame()
        self.line.setFrameShape(QFrame.Shape.HLine)
        self.line.setFrameShadow(QFrame.Shadow.Sunken)

        self.mainLayout.addWidget(self.documentsTreeView)
        self.mainLayout.addLayout(self.formLayout)
        self.mainLayout.addWidget(self.line)
        self.mainLayout.addWidget(self.buttonBox)

        self.mainDialog.resize(500, 300)
        self.mainDialog.setWindowTitle(i18n("Filter Manager"))
        self.mainDialog.setSizeGripEnabled(True)
        self.mainDialog.show()
        self.mainDialog.activateWindow()

    def confirmButton(self):
        documentsIndexes = []

        selectionModel = self.documentsTreeView.selectionModel()
        for index in selectionModel.selectedRows():
            node = self.treeModel.data(index, Qt.ItemDataRole.UserRole + 1)
            documentIndex = self.treeModel.data(index, Qt.ItemDataRole.UserRole + 2)
            _type = self.treeModel.data(index, Qt.ItemDataRole.UserRole + 3)

            if _type == 'Document':
                self.applyFilterOverDocument(self.documents[documentIndex])
            else:
                self.applyFilterOverNode(node, self.documents[documentIndex])

            documentsIndexes.append(documentIndex)

        self.refreshDocumentsProjections(set(documentsIndexes))

    def refreshDocumentsProjections(self, indexes):
        for index in indexes:
            document = self.documents[index]
            document.refreshProjection()

    def applyFilterOverNode(self, node, document):
        _filter = self.kritaInstance.filter(self.filterComboBox.currentText())
        _filter.apply(node, 0, 0, document.width(), document.height())

    def applyFilterOverDocument(self, document):
        """This method applies the selected filter just to topLevelNodes,
        then if topLevelNodes are GroupLayers, that filter will not be
        applied."""

        for node in document.topLevelNodes():
            self.applyFilterOverNode(node, document)

    @property
    def filters(self):
        return self._filters

    @property
    def documents(self):
        return self._documents
