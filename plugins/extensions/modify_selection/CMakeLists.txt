set(kritamodifyselection_SOURCES
    dlg_grow_selection.cc
    dlg_shrink_selection.cc
    dlg_border_selection.cc
    dlg_feather_selection.cc
    modify_selection.cc
    modify_selection_operations.cpp
    )


ki18n_wrap_ui(kritamodifyselection_SOURCES
    wdg_grow_selection.ui
    wdg_shrink_selection.ui
    wdg_border_selection.ui
    wdg_feather_selection.ui
    )

kis_add_library(kritamodifyselection MODULE ${kritamodifyselection_SOURCES})
target_link_libraries(kritamodifyselection kritaui)
install(TARGETS kritamodifyselection  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})
install( FILES  modify_selection.xmlgui  DESTINATION ${KDE_INSTALL_DATADIR}/kritaplugins)
