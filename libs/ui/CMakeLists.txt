
if (ANDROID)
    # We explicitly set QT_OPENGL_ES_3 here because Qt for some reason does
    # not enable this for Android.
    # See also: https://bugreports.qt.io/browse/QTBUG-64703
    # This is not needed for ARM GNU/Linux environments.
    add_definitions(-DQT_OPENGL_ES_3)
    if (QT_MAJOR_VERSION STREQUAL "5")
        include_directories (${Qt5AndroidExtras_INCLUDE_DIRS})
    endif()
endif()

add_subdirectory( tests )

if (APPLE)
    find_library(FOUNDATION_LIBRARY Foundation)
    find_library(APPKIT_LIBRARY AppKit)
endif ()

set(kritaui_LIB_SRCS
    canvas/kis_canvas_widget_base.cpp
    canvas/kis_canvas2.cpp
    canvas/kis_canvas_updates_compressor.cpp
    canvas/kis_canvas_controller.cpp
    canvas/kis_display_color_converter.cpp
    canvas/kis_display_filter.cpp
    canvas/kis_exposure_gamma_correction_interface.cpp
    canvas/kis_tool_proxy.cpp
    canvas/kis_canvas_decoration.cc
    canvas/kis_coordinates_converter.cpp
    canvas/kis_grid_manager.cpp
    canvas/kis_grid_decoration.cpp
    canvas/kis_grid_config.cpp
    canvas/kis_prescaled_projection.cpp
    canvas/kis_qpainter_canvas.cpp
    canvas/kis_projection_backend.cpp
    canvas/kis_update_info.cpp
    canvas/kis_image_patch.cpp
    canvas/kis_image_pyramid.cpp
    canvas/kis_infinity_manager.cpp
    canvas/kis_change_guides_command.cpp
    canvas/kis_guides_decoration.cpp
    canvas/kis_guides_manager.cpp
    canvas/kis_guides_config.cpp
    canvas/kis_snap_config.cpp
    canvas/kis_snap_line_strategy.cpp
    canvas/KisSnapPointStrategy.cpp
    canvas/KisSnapPixelStrategy.cpp
    canvas/KisMirrorAxisConfig.cpp
    canvas/KoCanvasControllerWidget.cpp
    canvas/KoCanvasControllerWidgetViewport_p.cpp
    canvas/KisCanvasState.cpp

    dialogs/kis_about_application.cpp
    dialogs/kis_dlg_adj_layer_props.cc
    dialogs/kis_dlg_adjustment_layer.cc
    dialogs/kis_dlg_filter.cpp
    dialogs/kis_dlg_generator_layer.cpp
    dialogs/kis_dlg_file_layer.cpp
    dialogs/kis_dlg_filter.cpp
    dialogs/kis_dlg_stroke_selection_properties.cpp
    dialogs/kis_dlg_image_properties.cc
    dialogs/kis_dlg_layer_properties.cc
    dialogs/kis_dlg_paste_format.cpp
    dialogs/kis_dlg_preferences.cc
    dialogs/slider_and_spin_box_sync.cpp
    dialogs/kis_dlg_layer_style.cpp
    dialogs/kis_dlg_png_import.cpp
    dialogs/kis_dlg_import_image_sequence.cpp
    dialogs/kis_delayed_save_dialog.cpp
    dialogs/KisSessionManagerDialog.cpp
    dialogs/KisNewWindowLayoutDialog.cpp
    dialogs/KisDlgChangeCloneSource.cpp
    dialogs/KisRecoverNamedAutosaveDialog.cpp
    dialogs/KisDlgSavePreset.cpp
    dialogs/kis_dlg_missing_color_profile.cpp
    dialogs/kis_dlg_hlg_import.cpp
    dialogs/KisDlgConfigureCumulativeUndo.cpp
    dialogs/KisCumulativeUndoModel.cpp
    dialogs/KisColorSpaceConversionDialog.cpp
    dialogs/KisProofingConfigModel.cpp
    dialogs/KisFrameRateLimitModel.cpp

    flake/kis_node_dummies_graph.cpp
    flake/kis_dummies_facade_base.cpp
    flake/kis_dummies_facade.cpp
    flake/kis_node_shapes_graph.cpp
    flake/kis_node_shape.cpp
    flake/kis_shape_controller.cpp
    flake/kis_shape_layer.cc
    flake/kis_shape_layer_canvas.cpp
    flake/kis_shape_selection.cpp
    flake/kis_shape_selection_canvas.cpp
    flake/kis_shape_selection_model.cpp
    flake/kis_take_all_shapes_command.cpp

    kis_aspect_ratio_locker.cpp
    kis_bookmarked_configurations_editor.cc
    kis_bookmarked_configurations_model.cc
    kis_bookmarked_filter_configurations_model.cc
    KisPaintopPropertiesBase.cpp
    KisPlaybackEngine.cpp
    KisPlaybackEngineQT.cpp
    KisPresetShadowUpdater.cpp
    kis_canvas_resource_provider.cpp
    kis_derived_resources.cpp
    kis_categories_mapper.cpp
    kis_categorized_list_model.cpp
    kis_categorized_item_delegate.cpp
    kis_clipboard.cc
    kis_config.cc
    KisOcioConfiguration.cpp
    kis_control_frame.cpp
    kis_composite_ops_model.cc
    KisDisplayConfig.cpp
    kis_paint_ops_model.cpp
    kis_custom_pattern.cc
    kis_file_layer.cpp
    kis_change_file_layer_command.h
    kis_safe_document_loader.cpp
    kis_splash_screen.cpp
    kis_filter_manager.cc
    kis_filters_model.cc
    kis_image_manager.cc
    kis_image_view_converter.cpp
    kis_import_catcher.cc
    kis_layer_manager.cc
    kis_mask_manager.cc
    kis_mimedata.cpp
    kis_node_commands_adapter.cpp
    kis_node_manager.cpp
    kis_node_juggler_compressed.cpp
    kis_node_selection_adapter.cpp
    kis_node_insertion_adapter.cpp
    KisNodeDisplayModeAdapter.cpp
    kis_node_model.cpp
    kis_node_filter_proxy_model.cpp
    kis_model_index_converter_base.cpp
    kis_model_index_converter.cpp
    kis_model_index_converter_show_all.cpp
    KisLayerThumbnailCache.cpp
    kis_painting_assistant.cc
    kis_painting_assistants_decoration.cpp
    KisDecorationsManager.cpp
    kis_paintop_box.cc
    kis_paintop_option.cpp
    kis_paintop_options_model.cpp
    kis_paintop_settings_widget.cpp
    kis_popup_palette.cpp
    kis_png_converter.cpp
    kis_preference_set_registry.cpp
    KisResourceServerProvider.cpp
    KisSelectedShapesProxy.cpp
    kis_selection_decoration.cc
    kis_selection_manager.cc
    KisSelectionActionsAdapter.cpp
    kis_statusbar.cc
    kis_zoom_manager.cc
    kis_favorite_resource_manager.cpp
    kis_workspace_resource.cpp
    kis_action.cpp
    kis_action_manager.cpp
    KisActionPlugin.cpp
    kis_canvas_controls_manager.cpp
    kis_multinode_property.cpp
    KisWelcomePageWidget.cpp
    KisUiFont.cpp
    KisIdleTasksManager.cpp
    KisIdleTaskStrokeStrategy.cpp
    KisImageThumbnailStrokeStrategy.cpp
    KisTextPropertiesManager.cpp

    opengl/kis_opengl.cpp
    opengl/kis_opengl_canvas2.cpp
    opengl/kis_opengl_canvas_debugger.cpp
    opengl/kis_opengl_image_textures.cpp
    opengl/kis_texture_tile.cpp
    opengl/kis_opengl_shader_loader.cpp
    opengl/kis_texture_tile_info_pool.cpp
    opengl/KisOpenGLUpdateInfoBuilder.cpp
    opengl/KisOpenGLModeProber.cpp
    opengl/KisScreenInformationAdapter.cpp
    opengl/KisOpenGLBufferCircularStorage.cpp
    opengl/KisOpenGLSync.cpp
    opengl/KisOpenGLBufferCreationGuard.cpp
    opengl/KisOpenGLCanvasRenderer.cpp
    opengl/KisOpenGLContextSwitchLock.cpp
    kis_fps_decoration.cpp

    tool/KisToolChangesTracker.cpp
    tool/KisToolChangesTrackerData.cpp
    tool/kis_selection_tool_helper.cpp
    tool/kis_selection_tool_config_widget_helper.cpp
    tool/kis_rectangle_constraint_widget.cpp
    tool/kis_shape_tool_helper.cpp
    tool/kis_tool.cc
    tool/kis_delegated_tool_policies.cpp
    tool/kis_tool_freehand.cc
    tool/kis_speed_smoother.cpp
    tool/kis_painting_information_builder.cpp
    tool/kis_stabilized_events_sampler.cpp
    tool/kis_tool_freehand_helper.cpp
    tool/kis_tool_multihand_helper.cpp
    tool/kis_figure_painting_tool_helper.cpp
    tool/KisAsynchronousStrokeUpdateHelper.cpp
    tool/kis_tool_paint.cc
    tool/kis_tool_shape.cc
    tool/kis_tool_ellipse_base.cpp
    tool/kis_tool_rectangle_base.cpp
    tool/kis_tool_polyline_base.cpp
    tool/KisToolOutlineBase.cpp
    tool/kis_tool_utils.cpp
    tool/kis_resources_snapshot.cpp
    tool/KisStrokeCompatibilityInfo.cpp
    tool/kis_smoothing_options.cpp
    tool/KisStabilizerDelayedPaintHelper.cpp
    tool/KisStrokeSpeedMonitor.cpp
    tool/strokes/freehand_stroke.cpp
    tool/strokes/KisStrokeEfficiencyMeasurer.cpp
    tool/strokes/kis_painter_based_stroke_strategy.cpp
    tool/strokes/kis_filter_stroke_strategy.cpp
    tool/strokes/kis_color_sampler_stroke_strategy.cpp
    tool/strokes/KisFreehandStrokeInfo.cpp
    tool/strokes/KisMaskedFreehandStrokePainter.cpp
    tool/strokes/KisMaskingBrushRenderer.cpp
    tool/strokes/KisMaskingBrushCompositeOpFactory.cpp
    tool/strokes/move_stroke_strategy.cpp
    tool/strokes/KisNodeSelectionRecipe.cpp
    tool/KisSelectionToolFactoryBase.cpp
    tool/KisToolPaintFactoryBase.cpp
    tool/KisAsyncColorSamplerHelper.cpp

    toolbox/KoToolBox.cpp
    toolbox/KoToolBoxButton.cpp
    toolbox/KoToolBoxDocker.cpp
    toolbox/KoToolBoxFactory.cpp

    widgets/kis_canvas_drop.cpp
    widgets/kis_cmb_composite.cc
    widgets/KisCompositeOpListConnectionHelper.cpp
    widgets/kis_cmb_contour.cpp
    widgets/kis_cmb_gradient.cpp
    widgets/kis_collapsible_button_group.cpp
    widgets/kis_paintop_list_widget.cpp
    widgets/kis_cmb_idlist.cc
    widgets/kis_color_space_selector.cc
    widgets/kis_advanced_color_space_selector.cc
    widgets/kis_cie_tongue_widget.cpp
    widgets/kis_tone_curve_widget.cpp
    widgets/kis_transport_controls.cpp
    widgets/kis_utility_title_bar.cpp
    widgets/kis_utility_title_bar_p.h
    widgets/kis_curve_widget.cpp
    widgets/KisCurveWidgetConnectionHelper.cpp
    widgets/KisCurveWidgetControlsManager.cpp
    widgets/kis_custom_image_widget.cc
    widgets/kis_image_from_clipboard_widget.cpp
    widgets/kis_filter_selector_widget.cc
    widgets/kis_iconwidget.cc
    widgets/kis_mask_widgets.cpp
    widgets/kis_meta_data_merge_strategy_chooser_widget.cc
    widgets/kis_multi_bool_filter_widget.cc
    widgets/kis_multi_double_filter_widget.cc
    widgets/kis_multi_integer_filter_widget.cc
    widgets/kis_paintop_presets_editor.cpp
    widgets/kis_tool_options_popup.cpp
    widgets/kis_paintop_presets_chooser_popup.cpp
    widgets/kis_paintop_preset_icon_library.cpp
    widgets/kis_pattern_chooser.cc

    widgets/kis_preset_chooser.cpp
    widgets/kis_progress_widget.cpp
    widgets/kis_selection_options.cc
    widgets/kis_scratch_pad.cpp
    widgets/kis_scratch_pad_event_filter.cpp
    widgets/KisSelectionPropertySlider.cpp
    widgets/kis_size_group.cpp
    widgets/kis_size_group_p.cpp
    widgets/kis_wdg_generator.cpp
    widgets/kis_workspace_chooser.cpp
    widgets/kis_categorized_list_view.cpp
    widgets/kis_widget_chooser.cpp
    widgets/kis_tool_button.cpp
    widgets/kis_floating_message.cpp
    widgets/kis_lod_availability_widget.cpp
    widgets/KisLodAvailabilityData.cpp
    widgets/KisLodAvailabilityModel.cpp
    widgets/kis_color_filter_combo.cpp
    widgets/kis_color_label_selector_widget.cpp
    widgets/kis_color_label_button.cpp
    widgets/kis_layer_filter_widget.cpp
    widgets/kis_elided_label.cpp
    widgets/kis_preset_live_preview_view.cpp
    widgets/KisScreenColorSampler.cpp
    widgets/KoDualColorButton.cpp
    widgets/KoStrokeConfigWidget.cpp
    widgets/KoFillConfigWidget.cpp
    widgets/KisLayerStyleAngleSelector.cpp
    widgets/KisMemoryReportButton.cpp
    widgets/KisDitherWidget.cpp
    widgets/KisWarningWidget.cpp
    widgets/KisAutoLevelsWidget.cpp
    widgets/KisHistogramPainter.cpp
    widgets/KisHistogramView.cpp
    widgets/kis_zoom_scrollbar.cpp
    widgets/KisDlgListPicker.cpp
    widgets/kis_round_hud_button.cpp
    widgets/KisDockerHud.cpp

    widgets/KisGrabKeyboardFocusRecoveryWorkaround.cpp

    # Gradient related widgets
    widgets/gradient/KisStopGradientEditor.cpp
    widgets/gradient/KisSegmentGradientEditor.cpp
    widgets/gradient/KisStopGradientSlider.cpp
    widgets/gradient/KisSegmentGradientSlider.cpp
    widgets/gradient/KisGradientChooser.cpp
    widgets/gradient/KisGenericGradientEditor.cpp
    widgets/gradient/KisGradientWidgetsUtils.cpp
    widgets/gradient/KisGradientColorEditor.cpp

    KisPaletteEditor.cpp
    dialogs/KisDlgPaletteEditor.cpp

    widgets/KisNewsWidget.cpp
    widgets/KisGamutMaskToolbar.cpp

    utils/kis_document_aware_spin_box_unit_manager.cpp
    utils/KisSpinBoxSplineUnitConverter.cpp
    utils/KisDitherUtil.cpp
    utils/KisFileIconCreator.cpp
    utils/KisRecentDocumentsModelWrapper.cpp
    utils/KisRecentFileIconCache.cpp

    input/kis_input_manager.cpp
    input/kis_input_manager_p.cpp
    input/kis_extended_modifiers_mapper.cpp
    input/KisExtendedModifiersMapperPluginInterface.cpp
    input/kis_abstract_input_action.cpp
    input/kis_tool_invocation_action.cpp
    input/kis_pan_action.cpp
    input/kis_alternate_invocation_action.cpp
    input/kis_rotate_canvas_action.cpp
    input/kis_zoom_action.cpp
    input/kis_change_frame_action.cpp
    input/kis_gamma_exposure_action.cpp
    input/KisPopupWidgetAction.cpp
    input/kis_change_primary_setting_action.cpp
    input/kis_abstract_shortcut.cpp
    input/kis_native_gesture_shortcut.cpp
    input/kis_single_action_shortcut.cpp
    input/kis_stroke_shortcut.cpp
    input/kis_shortcut_matcher.cpp
    input/kis_select_layer_action.cpp
    input/KisQtWidgetsTweaker.cpp
    input/KisInputActionGroup.cpp
    input/kis_zoom_and_rotate_action.cpp
    input/KisCanvasOnlyAction.cpp
    input/KisTouchGestureAction.cpp
    input/KisInputProfileMigrator.cpp

    operations/kis_operation.cpp
    operations/kis_operation_configuration.cpp
    operations/kis_operation_registry.cpp
    operations/kis_operation_ui_factory.cpp
    operations/kis_operation_ui_widget.cpp
    operations/kis_filter_selection_operation.cpp
    actions/kis_selection_action_factories.cpp
    actions/KisPasteActionFactories.cpp
    actions/KisTransformToolActivationCommand.cpp
    animation/KisFFMpegWrapper.cpp
    animation/KisVideoSaver.cpp
    animation/KisAnimationRenderingOptions.cpp
    animation/KisAnimationRender.cpp
    animation/KisDlgAnimationRenderer.cpp
    animation/VideoExportOptionsDialog.cpp
    animation/VideoHDRMetadataOptionsDialog.cpp
    animation/KisHDRMetadataOptions.cpp
    animation/KisDlgImportVideoAnimation.cpp
    animation/KisFrameDisplayProxy.cpp
    input/kis_touch_shortcut.cpp
    kis_document_undo_store.cpp
    kis_gui_context_command.cpp
    kis_gui_context_command_p.cpp
    input/kis_tablet_debugger.cpp
    input/kis_input_profile_manager.cpp
    input/kis_input_profile.cpp
    input/kis_shortcut_configuration.cpp
    input/config/kis_input_configuration_page.cpp
    input/config/kis_edit_profiles_dialog.cpp
    input/config/kis_input_profile_model.cpp
    input/config/kis_input_configuration_page_item.cpp
    input/config/kis_action_shortcuts_model.cpp
    input/config/kis_input_type_delegate.cpp
    input/config/kis_input_mode_delegate.cpp
    input/config/kis_input_button.cpp
    input/config/kis_input_editor_delegate.cpp
    input/config/kis_mouse_input_editor.cpp
    input/config/kis_wheel_input_editor.cpp
    input/config/kis_key_input_editor.cpp
    input/config/KisGestureSelector.cpp
    processing/fill_processing_visitor.cpp
    processing/KisEncloseAndFillProcessingVisitor.cpp
    canvas/kis_mirror_axis.cpp

    imagesize/imagesize.cc
    imagesize/dlg_canvassize.cc
    imagesize/dlg_imagesize.cc
    imagesize/wdg_imagesize.cc
    imagesize/dlg_layersize.cc

    kcanvaspreview.cc
    kis_abstract_perspective_grid.cpp

    KisApplication.cpp
    KisAutoSaveRecoveryDialog.cpp
    KisDetailsPane.cpp
    KisDocument.cpp
    KisCloneDocumentStroke.cpp
    kis_node_view_color_scheme.cpp
    KisImportExportFilter.cpp
    KisImportExportManager.cpp
    KisImportExportUtils.cpp
    KisImportUserFeedbackInterface.cpp
    kis_async_action_feedback.cpp
    KisMainWindow.cpp
    KisOpenPane.cpp
    KisPart.cpp
    KisTemplate.cpp
    KisTemplateCreateDia.cpp
    KisTemplateGroup.cpp
    KisTemplates.cpp
    KisTemplatesPane.cpp
    KisTemplateTree.cpp
    KisUndoActionsUpdateManager.cpp
    KisView.cpp
    KisCanvasWindow.cpp
    KisImportExportErrorCode.cpp
    KisImportExportAdditionalChecks.cpp

    thememanager.cpp

    kis_mainwindow_observer.cpp
    KisViewManager.cpp
    kis_mirror_manager.cpp

    qtsingleapplication/qtlocalpeer.cpp
    qtsingleapplication/qtsingleapplication.cpp

    KisApplicationArguments.cpp

    KisNetworkAccessManager.cpp
    KisRssReader.cpp
    KisMultiFeedRSSModel.cpp
    KisRemoteFileFetcher.cpp

    KisSaveGroupVisitor.cpp
    KisWindowLayoutResource.cpp
    KisWindowLayoutManager.cpp
    KisSessionResource.cpp

    KisReferenceImagesDecoration.cpp
    KisReferenceImage.cpp
    flake/KisReferenceImagesLayer.cpp
    flake/KisReferenceImagesLayer.h
    KisMouseClickEater.cpp

    KisDecorationsWrapperLayer.cpp

    KoDocumentInfoDlg.cpp
    KoDocumentInfo.cpp

    StoryboardItem.cpp

    KisNodeActivationActionCreatorVisitor.cpp
    KisAndroidFileProxy.cpp
)

if (KSeExpr_FOUND)
    set(kritaui_LIB_SRCS ${kritaui_LIB_SRCS}
        widgets/kis_seexpr_script_chooser.cpp
    )
endif()

if (Mlt7_FOUND)
    set(kritaui_LIB_SRCS ${kritaui_LIB_SRCS}
        KisPlaybackEngineMLT.cpp
        KisMLTProducerKrita.cpp
    )
endif()

if(WIN32)
    set(kritaui_LIB_SRCS
        ${kritaui_LIB_SRCS}
        dialogs/KisDlgCustomTabletResolution.cpp
        )
endif()

    set(kritaui_LIB_SRCS
        ${kritaui_LIB_SRCS}
        kis_animation_frame_cache.cpp
        kis_animation_cache_populator.cpp
        KisAsyncAnimationRendererBase.cpp
        KisAsyncAnimationCacheRenderer.cpp
        KisAsyncAnimationFramesSavingRenderer.cpp
        dialogs/KisAsyncAnimationRenderDialogBase.cpp
        dialogs/KisAsyncAnimationCacheRenderDialog.cpp
        dialogs/KisAsyncAnimationFramesSaveDialog.cpp
        canvas/KisCanvasAnimationState.cpp	
        kis_animation_importer.cpp
        KisFrameDataSerializer.cpp
        KisFrameCacheStore.cpp
        KisFrameCacheSwapper.cpp
        KisAbstractFrameCacheSwapper.cpp
        KisInMemoryFrameCacheSwapper.cpp

        input/wintab/drawpile_tablettester/tablettester.cpp
        input/wintab/drawpile_tablettester/tablettest.cpp
    )

if (ENABLE_UPDATERS)
	if (UNIX)
    	set(kritaui_LIB_SRCS
        	${kritaui_LIB_SRCS}
        	utils/KisAppimageUpdater.cpp
        	)
	endif()

	set(kritaui_LIB_SRCS
        ${kritaui_LIB_SRCS}
    	utils/KisUpdaterBase.cpp
    	utils/KisManualUpdater.cpp
    	utils/KisUpdaterStatus.cpp
		)
endif()


if(APPLE)
    set(kritaui_LIB_SRCS
        ${kritaui_LIB_SRCS}
        input/kis_extended_modifiers_mapper_osx.mm
        osx.mm
    )
endif()

ki18n_wrap_ui(kritaui_LIB_SRCS
    widgets/KoFillConfigWidget.ui
    widgets/KoStrokeConfigWidget.ui
    widgets/KisDitherWidget.ui
    widgets/KisAutoLevelsWidget.ui

    forms/wdgaboutapplication.ui
    forms/wdgdlgpngimport.ui
    forms/wdgfullscreensettings.ui
    forms/wdgsegmentgradienteditor.ui
    forms/wdggeneralsettings.ui
    forms/wdgperformancesettings.ui
    forms/wdggenerators.ui
    forms/wdgbookmarkedconfigurationseditor.ui
    forms/wdgapplyprofile.ui
    forms/wdgcustompattern.ui
    forms/wdglayerproperties.ui
    forms/wdgcolorsettings.ui
    forms/wdgtabletsettings.ui
    forms/wdgcolorspaceselector.ui
    forms/wdgcolorspaceselectoradvanced.ui
    forms/wdgdisplaysettings.ui
    forms/kis_matrix_widget.ui
    forms/wdggeometryoptions.ui
    forms/wdgnewimage.ui
    forms/wdgimageproperties.ui
    forms/wdgmaskfromselection.ui
    forms/wdgmasksource.ui
    forms/wdgfilterdialog.ui
    forms/wdgmetadatamergestrategychooser.ui
    forms/wdgpaintoppresets.ui
    forms/wdgpaintopsettings.ui
    forms/wdgdlggeneratorlayer.ui
    forms/wdgdlgfilelayer.ui
    forms/wdgfilterselector.ui
    forms/wdgfilternodecreation.ui
    forms/wdgnodequerypatheditor.ui
    forms/wdgpresetselectorstrip.ui
    forms/wdgsavebrushpreset.ui
    forms/wdgpreseticonlibrary.ui
    forms/wdgrectangleconstraints.ui
    forms/wdgimportimagesequence.ui
    forms/wdgstrokeselectionproperties.ui
    forms/KisDetailsPaneBase.ui
    forms/KisOpenPaneBase.ui
    forms/wdgstopgradienteditor.ui
    forms/wdgsessionmanager.ui
    forms/wdgnewwindowlayout.ui
    forms/KisWelcomePage.ui
    forms/WdgDlgPaletteEditor.ui
    forms/KisNewsPage.ui
    forms/wdgGamutMaskToolbar.ui
    forms/wdgchangeclonesource.ui
    forms/koDocumentInfoAboutWidget.ui
    forms/koDocumentInfoAuthorWidget.ui
    forms/WdgPopupPaletteSettings.ui
    forms/wdgMissingColorProfile.ui
    forms/wdgPasteFormat.ui
    forms/wdgconvertcolorspace.ui


    widgets/KisDlgListPicker.ui
    dialogs/kis_delayed_save_dialog.ui
    dialogs/kis_dlg_hlg_import.ui
    dialogs/KisRecoverNamedAutosaveDialog.ui
    input/config/kis_input_configuration_page.ui
    input/config/kis_edit_profiles_dialog.ui
    input/config/kis_input_configuration_page_item.ui
    input/config/kis_mouse_input_editor.ui
    input/config/kis_wheel_input_editor.ui
    input/config/kis_key_input_editor.ui

    layerstyles/wdgBevelAndEmboss.ui
    layerstyles/wdgblendingoptions.ui
    layerstyles/WdgColorOverlay.ui
    layerstyles/wdgContour.ui
    layerstyles/wdgdropshadow.ui
    layerstyles/WdgGradientOverlay.ui
    layerstyles/wdgInnerGlow.ui
    layerstyles/wdglayerstyles.ui
    layerstyles/WdgPatternOverlay.ui
    layerstyles/WdgSatin.ui
    layerstyles/WdgStroke.ui
    layerstyles/wdgstylesselector.ui
    layerstyles/wdgTexture.ui
    layerstyles/wdgKisLayerStyleAngleSelector.ui

    wdgsplash.ui
    input/wintab/drawpile_tablettester/tablettest.ui

    animation/wdg_animationrenderer.ui
    animation/video_export_options_dialog.ui
    animation/VideoHDRMetadataOptionsDialog.ui
    animation/VideoImportDialog.ui

    imagesize/wdg_imagesize.ui
    imagesize/wdg_layersize.ui
    imagesize/wdg_canvassize.ui
)

if(WIN32)
    ki18n_wrap_ui(kritaui_LIB_SRCS
        dialogs/KisDlgCustomTabletResolution.ui
    )
endif()


kis_add_library(kritaui SHARED ${kritaui_HEADERS_MOC} ${kritaui_LIB_SRCS})

target_sources(kritaui
  PRIVATE
    kis_abstract_resources.h
    kis_abstract_resources.cpp
)
generate_export_header(kritaui BASE_NAME kritaui)

target_link_libraries(kritaui
    PUBLIC
        KF${KF_MAJOR}::CoreAddons
        KF${KF_MAJOR}::Completion
        KF${KF_MAJOR}::I18n
        KF${KF_MAJOR}::ItemViews

        Qt${QT_MAJOR_VERSION}::Network
        Qt${QT_MAJOR_VERSION}::Concurrent

        Eigen3::Eigen
        Boost::boost
        ${PNG_LIBRARIES}

        kritaversion
        kritaimpex
        kritacolor
        kritaimage
        kritalibbrush
        kritawidgets
        kritawidgetutils
        kritaresources
)

if(WIN32)
    # Private headers are needed for:
    #    * KisDlgCustomTabletResolution
    #    * KisScreenInformationAdapter
    #    * dialogs/kis_dlg_preferences.cc
    set_source_files_properties(
        dialogs/KisDlgCustomTabletResolution.cpp
        opengl/KisScreenInformationAdapter.cpp
        dialogs/kis_dlg_preferences.cc # for WinTab switching API
        kis_config.cc # for logging the WinTab API
        PROPERTIES
        INCLUDE_DIRECTORIES "${Qt${QT_MAJOR_VERSION}Gui_PRIVATE_INCLUDE_DIRS}"
        )
endif()

if (QT_MAJOR_VERSION STREQUAL "6")
    target_link_libraries(kritaui
        PRIVATE
            Qt6::OpenGLWidgets
            Qt6::SvgWidgets
            KF6::ColorScheme
    )
endif()

if (Mlt7_FOUND)
    target_link_libraries(kritaui PRIVATE SDL2::SDL2 Mlt7::mlt Mlt7::mlt++)
endif()

if (ANDROID)
    target_link_libraries(kritaui PRIVATE GLESv3)
    target_link_libraries(kritaui PUBLIC Qt${QT_MAJOR_VERSION}::Gui)
    target_link_libraries(kritaui PRIVATE Qt${QT_MAJOR_VERSION}::AndroidExtras)
endif()

if (HAIKU)
  target_link_libraries(kritaui PRIVATE network expat iconv intl)
endif()

if(APPLE)
    target_link_libraries(kritaui
        PUBLIC
        kritamacosutils

        PRIVATE
        ${FOUNDATION_LIBRARY}
        ${APPKIT_LIBRARY}
    )
endif ()

if(OpenEXR_FOUND)
  target_link_libraries(kritaui PUBLIC ${LINK_OPENEXR_LIB})
endif()

target_include_directories(kritaui
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/canvas>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/flake>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/ora>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/tool>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/utils>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/widgets>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/widgets/gradient>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/input/wintab>
)

set_target_properties(kritaui
    PROPERTIES VERSION ${GENERIC_KRITA_LIB_VERSION} SOVERSION ${GENERIC_KRITA_LIB_SOVERSION}
)
install(TARGETS kritaui  ${INSTALL_TARGETS_DEFAULT_ARGS})


if (APPLE)
    install(FILES osx.stylesheet DESTINATION ${KDE_INSTALL_DATADIR}/krita)
endif ()

if (UNIX AND BUILD_TESTING AND ENABLE_UPDATERS)
    install(FILES tests/data/AppImageUpdateDummy
            PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
            DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
endif ()
