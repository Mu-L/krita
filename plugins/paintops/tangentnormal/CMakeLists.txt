set(kritatangentnormalpaintop_SOURCES
    kis_tangent_normal_paintop_plugin.cpp
    kis_tangent_normal_paintop.cpp
    kis_tangent_normal_paintop_settings_widget.cpp
    kis_normal_preview_widget.cpp
    KisTangentTiltOption.cpp
    KisTangentTiltOptionData.cpp
    KisTangentTiltOptionModel.cpp
    KisTangentTiltOptionWidget.cpp

)

ki18n_wrap_ui(kritatangentnormalpaintop_SOURCES wdgtangenttiltoption.ui )

kis_add_library(kritatangentnormalpaintop MODULE ${kritatangentnormalpaintop_SOURCES})

target_link_libraries(kritatangentnormalpaintop kritalibpaintop)

install(TARGETS kritatangentnormalpaintop  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})
########### install files ###############

install( FILES  krita-tangentnormal.png DESTINATION ${KDE_INSTALL_DATADIR}/krita/images)
install( FILES  krita-tangentnormal-preview.png DESTINATION ${KDE_INSTALL_DATADIR}/krita/images)

