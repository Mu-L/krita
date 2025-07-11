# SPDX-License-Identifier: CC0-1.0

from krita import Extension
from builtins import i18n
from . import uiexportlayers


class ExportLayersExtension(Extension):

    def __init__(self, parent):
        super(ExportLayersExtension, self).__init__(parent)

    def setup(self):
        pass

    def createActions(self, window):
        action = window.createAction("export_layers", i18n("Export Layers"))
        action.setToolTip(i18n("Plugin to export layers from a document."))
        action.triggered.connect(self.initialize)

    def initialize(self):
        self.uiexportlayers = uiexportlayers.UIExportLayers()
        self.uiexportlayers.initialize()
