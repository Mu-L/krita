set(kritametadataeditor_SOURCES metadataeditor.cc kis_entry_editor.cc kis_meta_data_editor.cc kis_meta_data_model.cpp )

ki18n_wrap_ui(kritametadataeditor_SOURCES 
    editors/dublincore.ui
    editors/exif.ui
)

kis_add_library(kritametadataeditor MODULE ${kritametadataeditor_SOURCES})
target_link_libraries(kritametadataeditor kritaui )
install(TARGETS kritametadataeditor  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})
install( FILES metadataeditor.xmlgui DESTINATION ${KDE_INSTALL_DATADIR}/kritaplugins )
