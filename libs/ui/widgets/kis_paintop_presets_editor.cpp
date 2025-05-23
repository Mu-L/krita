/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2008 Boudewijn Rempt <boud@valdyas.org>
 * SPDX-FileCopyrightText: 2010 Lukáš Tvrdý <lukast.dev@gmail.com>
 * SPDX-FileCopyrightText: 2011 Silvio Heinrich <plassy@web.de>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "widgets/kis_paintop_presets_editor.h"

#include <QList>
#include <QHBoxLayout>
#include <QToolButton>
#include <QGridLayout>
#include <QFont>
#include <QMenu>
#include <QAction>
#include <QShowEvent>
#include <QFontDatabase>
#include <QWidgetAction>
#include <QScreen>
#include <QSplitter>
#include <QActionGroup>

#include <kconfig.h>
#include <klocalizedstring.h>

#include <KoDockRegistry.h>

#include <kis_icon.h>
#include <brushengine/kis_paintop_preset.h>
#include <brushengine/kis_paintop_config_widget.h>
#include <kis_canvas_resource_provider.h>
#include <widgets/kis_preset_chooser.h>
#include <KisResourceUserOperations.h>
#include <KisResourceItemChooser.h>

#include <ui_wdgpaintopsettings.h>
#include <kis_node.h>
#include "kis_config.h"

#include "KisResourceServerProvider.h"
#include "kis_lod_availability_widget.h"
#include "KisLodAvailabilityModel.h"

#include "kis_signal_auto_connection.h"
#include <kis_paintop_settings.h>
#include <KisPaintOpPresetUpdateProxy.h>

// ones from brush engine selector
#include <brushengine/kis_paintop_factory.h>
#include <kis_preset_live_preview_view.h>

#include <lager/state.hpp>

struct KisPaintOpPresetsEditor::Private
{
    Ui_WdgPaintOpSettings uiWdgPaintOpPresetSettings;
    QGridLayout *layout {0};

    QSplitter* horzSplitter {0};
    QList<int> defaultSplitterSizes;
    int presetPanelWidth {0};
    int scratchPanelWidth {0};

    KisPaintOpConfigWidget *settingsWidget {0};
    QFont smallFont;
    KisCanvasResourceProvider *resourceProvider {0};
    KisFavoriteResourceManager *favoriteResManager {0};

    bool ignoreHideEvents;
    bool isCreatingBrushFromScratch = false;

    KisSignalAutoConnectionsStore widgetConnections;

    lager::state<KisLodAvailabilityData, lager::automatic_tag> lodAvailabilityData;
};

KisPaintOpPresetsEditor::KisPaintOpPresetsEditor(KisCanvasResourceProvider * resourceProvider,
                                                 KisFavoriteResourceManager* favoriteResourceManager,
                                                 KisPresetSaveWidget* savePresetWidget,
                                                 QWidget * parent)
    : QWidget(parent)
    , m_d(new Private())
{
    setObjectName("KisPaintOpPresetsEditor");

    KisConfig cfg(true);

    current_paintOpId = "";

    m_d->resourceProvider = resourceProvider;
    m_d->favoriteResManager = favoriteResourceManager;

    m_d->uiWdgPaintOpPresetSettings.setupUi(this);

    m_d->layout = new QGridLayout(m_d->uiWdgPaintOpPresetSettings.frmOptionWidgetContainer);

    m_d->uiWdgPaintOpPresetSettings.scratchPad->setupScratchPad(resourceProvider, Qt::white);
    m_d->uiWdgPaintOpPresetSettings.scratchPad->setCutoutOverlayRect(QRect(25, 25, 200, 200));

    m_d->uiWdgPaintOpPresetSettings.dirtyPresetIndicatorButton->setToolTip(i18n("The settings for this preset have changed from their default."));

    m_d->uiWdgPaintOpPresetSettings.showPresetsButton->setToolTip(i18n("Toggle showing presets"));

    m_d->uiWdgPaintOpPresetSettings.showScratchpadButton->setToolTip(i18n("Toggle showing scratchpad"));

    m_d->uiWdgPaintOpPresetSettings.reloadPresetButton->setToolTip(i18n("Reload the brush preset"));
    m_d->uiWdgPaintOpPresetSettings.renameBrushPresetButton->setToolTip(i18n("Rename the brush preset"));


    // creating a new preset from scratch. Part of the brush presets area
    // the menu options will get filled up later when we are generating all available paintops
    // in the filter drop-down
    newPresetBrushEnginesMenu = new QMenu();

    // overwrite existing preset and saving a new preset use the same dialog
    saveDialog = savePresetWidget;
    saveDialog->scratchPadSetup(resourceProvider);
    saveDialog->setFavoriteResourceManager(m_d->favoriteResManager); // this is needed when saving the preset
    saveDialog->hide();

    // the area on the brush editor for renaming the brush. make sure edit fields are hidden by default
    toggleBrushRenameUIActive(false);

    // Brush Preset Left Panel
    {
        // Container
        QVBoxLayout* containerLayout = dynamic_cast<QVBoxLayout*>(m_d->uiWdgPaintOpPresetSettings.presetsContainer->layout());
        containerLayout->setAlignment(Qt::AlignmentFlag::AlignTop);  // spacers are not required

        // Show Panel Button
        m_d->uiWdgPaintOpPresetSettings.showPresetsButton->setCheckable(true);
        m_d->uiWdgPaintOpPresetSettings.showPresetsButton->setChecked(cfg.presetStripVisible());

        connect(m_d->uiWdgPaintOpPresetSettings.showPresetsButton, SIGNAL(clicked(bool)), this, SLOT(slotSwitchShowPresets(bool)));

        // Brush Engine Filter
        connect(m_d->uiWdgPaintOpPresetSettings.brushEngineComboBox, SIGNAL(currentIndexChanged(int)),
            this, SLOT(slotUpdatePaintOpFilter()));

        // Brush Presets
        KisPresetChooser* presetChooser = m_d->uiWdgPaintOpPresetSettings.presetWidget;
        presetChooser->showTaggingBar(true);

        // Brush Presets ViewMode Menu
        QMenu* viewModeMenu = new QMenu(this);
        {
            viewModeMenu->setStyleSheet("margin: 6px");

            // View Modes Btns
            viewModeMenu->addSection(i18nc("@title Which elements to display (e.g., thumbnails or details)", "Display"));
            KisPresetChooser::ViewMode mode = (KisPresetChooser::ViewMode)cfg.presetChooserViewMode();
            QActionGroup *actionGroup = new QActionGroup(this);

            QAction* action = viewModeMenu->addAction(KisIconUtils::loadIcon("view-preview"), i18n("Thumbnails"));
            action->setCheckable(true);
            action->setChecked(mode == KisPresetChooser::THUMBNAIL);
            action->setActionGroup(actionGroup);

            connect(action, &QAction::triggered,
                m_d->uiWdgPaintOpPresetSettings.presetWidget, &KisPresetChooser::setViewModeToThumbnail);

            action = viewModeMenu->addAction(KisIconUtils::loadIcon("view-list-details"), i18n("Details"));
            action->setCheckable(true);
            action->setChecked(mode == KisPresetChooser::DETAIL);
            action->setActionGroup(actionGroup);

            connect(action, &QAction::triggered,
                m_d->uiWdgPaintOpPresetSettings.presetWidget, &KisPresetChooser::setViewModeToDetail);

            // Icon Size Slider
            viewModeMenu->addSection(i18n("Icon Size"));

            QSlider* iconSizeSlider = new QSlider(this);
            iconSizeSlider->setOrientation(Qt::Horizontal);
            iconSizeSlider->setRange(30, 80);
            iconSizeSlider->setValue(m_d->uiWdgPaintOpPresetSettings.presetWidget->iconSize());
            iconSizeSlider->setMinimumHeight(20);
            iconSizeSlider->setMinimumWidth(40);
            iconSizeSlider->setTickInterval(10);

            connect(iconSizeSlider, &QSlider::valueChanged,
                    m_d->uiWdgPaintOpPresetSettings.presetWidget, &KisPresetChooser::setIconSize);

            connect(iconSizeSlider, &QSlider::sliderReleased,
                m_d->uiWdgPaintOpPresetSettings.presetWidget, &KisPresetChooser::saveIconSize);

            QWidgetAction *sliderAction= new QWidgetAction(this);
            sliderAction->setDefaultWidget(iconSizeSlider);

            viewModeMenu->addAction(sliderAction);
        }

        KisResourceItemChooser* resourceChooser = presetChooser->itemChooser();
        resourceChooser->viewModeButton()->setPopupWidget(viewModeMenu);

        // Bottom Bar Buttons
        m_d->uiWdgPaintOpPresetSettings.newPresetEngineButton->setPopupMode(QToolButton::InstantPopup);  // loading preset from scratch option
        connect(m_d->uiWdgPaintOpPresetSettings.bnBlacklistPreset,SIGNAL(clicked()),
            this, SLOT(slotBlackListCurrentPreset()));  // TODO: add confirm dialog
    }

    // show/hide buttons
    m_d->uiWdgPaintOpPresetSettings.showScratchpadButton->setCheckable(true);
    m_d->uiWdgPaintOpPresetSettings.showScratchpadButton->setChecked(cfg.scratchpadVisible());

    QMenu *viewOptionsMenu = new QMenu(this);
    QAction *detachBrushEditorAction = viewOptionsMenu->addAction(i18n("Detach Brush Editor"));
    detachBrushEditorAction->setCheckable(true);
    detachBrushEditorAction->setChecked(cfg.paintopPopupDetached());
    m_d->uiWdgPaintOpPresetSettings.viewOptionButton->setMenu(viewOptionsMenu);

    // Connections
    connect(m_d->uiWdgPaintOpPresetSettings.paintPresetIcon, SIGNAL(clicked()),
            m_d->uiWdgPaintOpPresetSettings.scratchPad, SLOT(paintPresetImage()));

    connect(saveDialog, SIGNAL(resourceSelected(KoResourceSP )), this, SLOT(resourceSelected(KoResourceSP )));

    connect (m_d->uiWdgPaintOpPresetSettings.renameBrushPresetButton, SIGNAL(clicked(bool)),
             this, SLOT(slotRenameBrushActivated()));

    connect (m_d->uiWdgPaintOpPresetSettings.cancelBrushNameUpdateButton, SIGNAL(clicked(bool)),
             this, SLOT(slotRenameBrushDeactivated()));

    connect(m_d->uiWdgPaintOpPresetSettings.updateBrushNameButton, SIGNAL(clicked(bool)),
            this, SLOT(slotSaveRenameCurrentBrush()));

    connect(m_d->uiWdgPaintOpPresetSettings.renameBrushNameTextField, SIGNAL(returnPressed()),
            SLOT(slotSaveRenameCurrentBrush()));

    connect(m_d->uiWdgPaintOpPresetSettings.showScratchpadButton, SIGNAL(clicked(bool)),
            this, SLOT(slotSwitchScratchpad(bool)));

    connect(detachBrushEditorAction, SIGNAL(toggled(bool)),
            this, SLOT(slotToggleDetach(bool)));

    connect(m_d->uiWdgPaintOpPresetSettings.eraseScratchPad, SIGNAL(clicked()),
            m_d->uiWdgPaintOpPresetSettings.scratchPad, SLOT(fillDefault()));

    connect(m_d->uiWdgPaintOpPresetSettings.fillLayer, SIGNAL(clicked()),
            m_d->uiWdgPaintOpPresetSettings.scratchPad, SLOT(fillDocument()));

    connect(m_d->uiWdgPaintOpPresetSettings.fillGradient, SIGNAL(clicked()),
            m_d->uiWdgPaintOpPresetSettings.scratchPad, SLOT(fillGradient()));

    connect(m_d->uiWdgPaintOpPresetSettings.fillSolid, SIGNAL(clicked()),
            m_d->uiWdgPaintOpPresetSettings.scratchPad, SLOT(fillBackground()));


    m_d->settingsWidget = 0;

    connect(m_d->uiWdgPaintOpPresetSettings.saveBrushPresetButton, SIGNAL(clicked()),
            this, SLOT(slotSaveBrushPreset()));

    connect(m_d->uiWdgPaintOpPresetSettings.saveNewBrushPresetButton, SIGNAL(clicked()),
            this, SLOT(slotSaveNewBrushPreset()));

    connect(m_d->uiWdgPaintOpPresetSettings.reloadPresetButton, SIGNAL(clicked()),
            this, SIGNAL(reloadPresetClicked()));

    connect(m_d->uiWdgPaintOpPresetSettings.dirtyPresetCheckBox, SIGNAL(toggled(bool)),
            this, SIGNAL(dirtyPresetToggled(bool)));

    connect(m_d->uiWdgPaintOpPresetSettings.eraserBrushSizeCheckBox, SIGNAL(toggled(bool)),
            this, SIGNAL(eraserBrushSizeToggled(bool)));

    connect(m_d->uiWdgPaintOpPresetSettings.eraserBrushOpacityCheckBox, SIGNAL(toggled(bool)),
            this, SIGNAL(eraserBrushOpacityToggled(bool)));


    // preset widget connections
    connect(m_d->uiWdgPaintOpPresetSettings.presetWidget, SIGNAL(resourceSelected(KoResourceSP )),
            this, SIGNAL(signalResourceSelected(KoResourceSP )));

    connect(m_d->uiWdgPaintOpPresetSettings.reloadPresetButton, SIGNAL(clicked()),
            m_d->uiWdgPaintOpPresetSettings.presetWidget, SLOT(updateViewSettings()));

    connect(m_d->uiWdgPaintOpPresetSettings.reloadPresetButton, SIGNAL(clicked()), SLOT(slotUpdatePresetSettings()));

    m_d->ignoreHideEvents = false;

    m_d->uiWdgPaintOpPresetSettings.dirtyPresetCheckBox->setChecked(cfg.useDirtyPresets());
    m_d->uiWdgPaintOpPresetSettings.eraserBrushSizeCheckBox->setChecked(cfg.useEraserBrushSize());
    m_d->uiWdgPaintOpPresetSettings.eraserBrushOpacityCheckBox->setChecked(cfg.useEraserBrushOpacity());

    connect(m_d->uiWdgPaintOpPresetSettings.brushEngineComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(slotUpdatePaintOpFilter()));
    connect(m_d->uiWdgPaintOpPresetSettings.bnBlacklistPreset, SIGNAL(clicked()), this, SLOT(slotBlackListCurrentPreset()));

    updateThemedIcons();

    // setup things like the scene construct images, layers, etc that is a one-time thing
    m_d->uiWdgPaintOpPresetSettings.liveBrushPreviewView->setup(resourceProvider->resourceManager());

    // Responsive Layout
    m_d->horzSplitter = new QSplitter(this);  // you can't add QSplitter to a UI file
    m_d->horzSplitter->setChildrenCollapsible(false);

    m_d->horzSplitter->addWidget(m_d->uiWdgPaintOpPresetSettings.presetsContainer);
    m_d->horzSplitter->setStretchFactor(0, 0);
    m_d->horzSplitter->addWidget(m_d->uiWdgPaintOpPresetSettings.brushEditorSettingsControls);
    m_d->horzSplitter->setStretchFactor(1, 2);
    m_d->horzSplitter->addWidget(m_d->uiWdgPaintOpPresetSettings.scratchpadControls);
    m_d->horzSplitter->setStretchFactor(2, 0);

    m_d->uiWdgPaintOpPresetSettings.gridLayout->addWidget(m_d->horzSplitter, 3, 0, 1, 3);

    // The side panels' sizeHints must be retrieved while they are visible.
    // The center widget's sizeHint must be retrieved later.
    m_d->defaultSplitterSizes << m_d->horzSplitter->widget(0)->sizeHint().width()
                              << m_d->horzSplitter->widget(1)->sizeHint().width()
                              << m_d->horzSplitter->widget(2)->sizeHint().width();

    // Default Configuration
    slotSwitchShowPresets(cfg.presetStripVisible());
    slotSwitchScratchpad(cfg.scratchpadVisible());
}

void KisPaintOpPresetsEditor::slotBlackListCurrentPreset()
{
    KisPaintOpPresetResourceServer *rServer = KisResourceServerProvider::instance()->paintOpPresetServer();
    KisPaintOpPresetSP curPreset = m_d->resourceProvider->currentPreset();
    rServer->removeResourceFromServer(curPreset);
}

void KisPaintOpPresetsEditor::slotRenameBrushActivated()
{
    toggleBrushRenameUIActive(true);
}

void KisPaintOpPresetsEditor::slotRenameBrushDeactivated()
{
    toggleBrushRenameUIActive(false);
}

void KisPaintOpPresetsEditor::toggleBrushRenameUIActive(bool isRenaming)
{
    // This function doesn't really do anything except get the UI in a state to rename a brush preset
    m_d->uiWdgPaintOpPresetSettings.renameBrushNameTextField->setVisible(isRenaming);
    m_d->uiWdgPaintOpPresetSettings.updateBrushNameButton->setVisible(isRenaming);
    m_d->uiWdgPaintOpPresetSettings.cancelBrushNameUpdateButton->setVisible(isRenaming);


    // hide these below areas while renaming
    m_d->uiWdgPaintOpPresetSettings.currentBrushNameLabel->setVisible(!isRenaming);
    m_d->uiWdgPaintOpPresetSettings.renameBrushPresetButton->setVisible(!isRenaming);
    m_d->uiWdgPaintOpPresetSettings.saveBrushPresetButton->setEnabled(!isRenaming);
    m_d->uiWdgPaintOpPresetSettings.saveBrushPresetButton->setVisible(!isRenaming);
    m_d->uiWdgPaintOpPresetSettings.saveNewBrushPresetButton->setEnabled(!isRenaming);
    m_d->uiWdgPaintOpPresetSettings.saveNewBrushPresetButton->setVisible(!isRenaming);

    // if the presets area is shown, only then can you show/hide the load default brush
    // need to think about weird state when you are in the middle of renaming a brush
    // what happens if you try to change presets. maybe we should auto-hide (or disable)
    // the presets area in this case
    if (m_d->uiWdgPaintOpPresetSettings.presetWidget->isVisible()) {
        m_d->uiWdgPaintOpPresetSettings.newPresetEngineButton->setVisible(!isRenaming);
        m_d->uiWdgPaintOpPresetSettings.bnBlacklistPreset->setVisible(!isRenaming);
    }

}

void KisPaintOpPresetsEditor::slotSaveRenameCurrentBrush()
{
    // if you are renaming a brush, that is different than updating the settings
    // make sure we are in a clean state before renaming. This logic might change,
    // but that is what we are going with for now
    KisPaintOpSettingsSP prevSettings = m_d->resourceProvider->currentPreset()->settings()->clone();
    bool isDirty = m_d->resourceProvider->currentPreset()->isDirty();

    // this returns the UI to its original state after saving
    toggleBrushRenameUIActive(false);
    slotUpdatePresetSettings(); // update visibility of dirty preset and icon

    // get a reference to the existing (and new) file name and path that we are working with
    KisPaintOpPresetSP curPreset = m_d->resourceProvider->currentPreset();
    // in case the preset is dirty, we need an id to get the actual non-dirty preset to save just the name change
    // into the database
    int currentPresetResourceId = curPreset->resourceId();

    QString renamedPresetName = m_d->uiWdgPaintOpPresetSettings.renameBrushNameTextField->text();

    // If the id < 0, this is a new preset that hasn't been added to the storage and the database yet.
    if (currentPresetResourceId < 0) {
        curPreset->setName(renamedPresetName);
        slotUpdatePresetSettings(); // update visibility of dirty preset and icon
        return;
    }

    Q_EMIT reloadPresetClicked();

    // create a new brush preset with the name specified and add to resource provider
    KisResourceModel model(ResourceType::PaintOpPresets);
    KoResourceSP properCleanResource = model.resourceForId(currentPresetResourceId);
    const bool success = KisResourceUserOperations::renameResourceWithUserInput(this, properCleanResource, renamedPresetName);

    if (isDirty) {
        properCleanResource.dynamicCast<KisPaintOpPreset>()->setSettings(prevSettings);
        properCleanResource.dynamicCast<KisPaintOpPreset>()->setDirty(isDirty);
    }

    // refresh and select our freshly renamed resource
    if (success) resourceSelected(properCleanResource);

    m_d->favoriteResManager->updateFavoritePresets();

    slotUpdatePresetSettings(); // update visibility of dirty preset and icon
}

KisPaintOpPresetsEditor::~KisPaintOpPresetsEditor()
{
    if (m_d->settingsWidget) {
        m_d->layout->removeWidget(m_d->settingsWidget);
        m_d->settingsWidget->hide();
        m_d->settingsWidget->setParent(0);
        m_d->settingsWidget = 0;
    }
    delete m_d;
    delete newPresetBrushEnginesMenu;
}

void KisPaintOpPresetsEditor::setPaintOpSettingsWidget(QWidget * widget)
{
    if (m_d->settingsWidget) {
        m_d->layout->removeWidget(m_d->settingsWidget);
        m_d->uiWdgPaintOpPresetSettings.frmOptionWidgetContainer->updateGeometry();
    }
    m_d->layout->update();
    updateGeometry();

    m_d->widgetConnections.clear();
    m_d->settingsWidget = 0;

    if (widget) {

        m_d->settingsWidget = dynamic_cast<KisPaintOpConfigWidget*>(widget);
        KIS_ASSERT_RECOVER_RETURN(m_d->settingsWidget);

        KisConfig cfg(true);
        if (m_d->settingsWidget->supportScratchBox() && cfg.scratchpadVisible()) {
            slotSwitchScratchpad(true);
        } else {
            slotSwitchScratchpad(false);
        }

        slotSwitchShowPresets(cfg.presetStripVisible());

        KisLodAvailabilityModel *model =
            new KisLodAvailabilityModel(m_d->lodAvailabilityData,
                                        m_d->settingsWidget->effectiveBrushSize(),
                                        m_d->settingsWidget->lodLimitationsReader());
        m_d->uiWdgPaintOpPresetSettings.wdgLodAvailability->setLodAvailabilityModel(model);

        widget->setFont(m_d->smallFont);

        m_d->layout->addWidget(widget);

        // hook up connections that will monitor if our preset is dirty or not. Show a notification if it is
        if (m_d->resourceProvider && m_d->resourceProvider->currentPreset() ) {

            KisPaintOpPresetSP preset = m_d->resourceProvider->currentPreset();
            m_d->widgetConnections.addConnection(preset->updateProxy(), SIGNAL(sigSettingsChanged()),
                                                 this, SLOT(slotUpdatePresetSettings()));
        }

        m_d->widgetConnections.addConnection(model, SIGNAL(effectiveLodAvailableChanged(bool)),
                                             this, SLOT(slotUpdateEffectiveLodAvailable(bool)));
        slotUpdateEffectiveLodAvailable(model->effectiveLodAvailable());

        m_d->widgetConnections.addConnection(model, SIGNAL(sigConfigurationItemChanged()),
                                             widget, SIGNAL(sigConfigurationItemChanged()));

        m_d->layout->update();
        widget->show();

    }
}

QImage KisPaintOpPresetsEditor::cutOutOverlay()
{
    return m_d->uiWdgPaintOpPresetSettings.scratchPad->cutoutOverlay();
}

void KisPaintOpPresetsEditor::contextMenuEvent(QContextMenuEvent *e)
{
    Q_UNUSED(e);
}

void KisPaintOpPresetsEditor::setCreatingBrushFromScratch(bool enabled)
{
    m_d->isCreatingBrushFromScratch = enabled;
}

void KisPaintOpPresetsEditor::readOptionSetting(const KisPropertiesConfigurationSP setting)
{
    KIS_SAFE_ASSERT_RECOVER_RETURN(m_d->settingsWidget);
    m_d->settingsWidget->setConfigurationSafe(setting);

    KisLodAvailabilityData data = m_d->lodAvailabilityData.get();
    data.read(setting.data());
    m_d->lodAvailabilityData.set(data);
}

void KisPaintOpPresetsEditor::writeOptionSetting(KisPropertiesConfigurationSP setting) const
{
    KIS_SAFE_ASSERT_RECOVER_RETURN(m_d->settingsWidget);

    m_d->settingsWidget->writeConfigurationSafe(setting);
    m_d->lodAvailabilityData->write(setting.data());
}

void KisPaintOpPresetsEditor::resourceSelected(KoResourceSP resource)
{
    // this gets called every time the brush editor window is opened
    // TODO: this gets called multiple times whenever the preset is changed in the presets area
    // the connections probably need to be thought about with this a bit more to keep things in sync

    m_d->uiWdgPaintOpPresetSettings.presetWidget->setCurrentResource(resource);

    // find the display name of the brush engine and append it to the selected preset display
    QString currentBrushEngineName;
    QPixmap currentBrushEngineIcon = QPixmap(26, 26);
    currentBrushEngineIcon.fill(Qt::transparent);
    for(int i=0; i < sortedBrushEnginesList.length(); i++) {
        if (sortedBrushEnginesList.at(i).id == currentPaintOpId() ) {
            currentBrushEngineName = sortedBrushEnginesList.at(i).name;
            currentBrushEngineIcon = sortedBrushEnginesList.at(i).icon.pixmap(26, 26);
        }
    }

    // brush names have underscores as part of the file name (to help with building). We don't really need underscores
    // when viewing the names, so replace them with spaces
    QString formattedBrushName = resource->name().replace("_", " ");

    m_d->uiWdgPaintOpPresetSettings.currentBrushNameLabel->setToolTip(formattedBrushName);
    formattedBrushName = this->fontMetrics().elidedText(formattedBrushName, Qt::ElideRight, m_d->uiWdgPaintOpPresetSettings.currentBrushNameLabel->width());
    m_d->uiWdgPaintOpPresetSettings.currentBrushNameLabel->setText(formattedBrushName);
    m_d->uiWdgPaintOpPresetSettings.currentBrushEngineLabel->setText(i18nc("%1 is the name of a brush engine", "%1 Engine", currentBrushEngineName));
    m_d->uiWdgPaintOpPresetSettings.currentBrushEngineIcon->setPixmap(currentBrushEngineIcon);
    m_d->uiWdgPaintOpPresetSettings.renameBrushNameTextField->setText(resource->name());

    // get the preset image and pop it into the thumbnail area on the top of the brush editor
    QSize thumbSize = QSize(55, 55)*devicePixelRatioF();
    QImage thumbImage = resource->image();

    m_d->uiWdgPaintOpPresetSettings.scratchPad->setPresetImage(thumbImage);

    QPixmap thumbnail;
    if (!thumbImage.isNull()) {
        thumbnail = QPixmap::fromImage(thumbImage.scaled(thumbSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        thumbnail = QPixmap();
    }

    thumbnail.setDevicePixelRatio(devicePixelRatioF());
    m_d->uiWdgPaintOpPresetSettings.presetThumbnailicon->setPixmap(thumbnail);

    toggleBrushRenameUIActive(false); // reset the UI state of renaming a brush if we are changing brush presets
    slotUpdatePresetSettings(); // check to see if the dirty preset icon needs to be shown
}

bool variantLessThan(const KisPaintOpInfo v1, const KisPaintOpInfo v2)
{
    return v1.priority < v2.priority;
}

void KisPaintOpPresetsEditor::setPaintOpList(const QList< KisPaintOpFactory* >& list)
{
    m_d->uiWdgPaintOpPresetSettings.brushEngineComboBox->clear(); // reset combobox list just in case


    // create a new list so we can sort it and populate the brush engine combo box
    sortedBrushEnginesList.clear(); // just in case this function is called again, don't keep adding to the list

    for(int i=0; i < list.length(); i++) {
        KisPaintOpInfo paintOpInfo;
        paintOpInfo.id = list.at(i)->id();
        paintOpInfo.name = list.at(i)->name();
        paintOpInfo.icon = list.at(i)->icon();
        paintOpInfo.priority = list.at(i)->priority();

        sortedBrushEnginesList.append(paintOpInfo);
    }

    std::stable_sort(sortedBrushEnginesList.begin(), sortedBrushEnginesList.end(), variantLessThan );

    // add an "All" option at the front to show all presets
    QPixmap emptyPixmap = QPixmap(22,22);
    emptyPixmap.fill(Qt::transparent);

    // if we create a new brush from scratch, we need a full list of paintops to choose from
    // we don't want "All", so populate the list before that is added
    newPresetBrushEnginesMenu->actions().clear(); // clean out list in case we run this again
    newBrushEngineOptions.clear();

    for (int j = 0; j < sortedBrushEnginesList.length(); j++) {
        auto * newEngineAction = newPresetBrushEnginesMenu->addAction(sortedBrushEnginesList[j].name);
        newEngineAction->setObjectName(sortedBrushEnginesList[j].id); // we need the ID for changing the paintop when action triggered
        newEngineAction->setIcon(sortedBrushEnginesList[j].icon);
        newBrushEngineOptions.append(newEngineAction);
        connect(newEngineAction, SIGNAL(triggered()), this, SLOT(slotCreateNewBrushPresetEngine()));
    }
    m_d->uiWdgPaintOpPresetSettings.newPresetEngineButton->setMenu(newPresetBrushEnginesMenu);

    // fill the list into the brush combo box
    sortedBrushEnginesList.push_front(KisPaintOpInfo(QString("all_options"), i18n("All"), QString(""), QIcon(emptyPixmap), 0 ));
    for (int m = 0; m < sortedBrushEnginesList.length(); m++) {
        m_d->uiWdgPaintOpPresetSettings.brushEngineComboBox->addItem(sortedBrushEnginesList[m].icon, sortedBrushEnginesList[m].name, QVariant(sortedBrushEnginesList[m].id));
    }
}


void KisPaintOpPresetsEditor::setCurrentPaintOpId(const QString& paintOpId)
{
    current_paintOpId = paintOpId;
}


QString KisPaintOpPresetsEditor::currentPaintOpId() {
    return current_paintOpId;
}

void KisPaintOpPresetsEditor::hideEvent(QHideEvent *event)
{
    if (m_d->ignoreHideEvents) {
        return;
    }

    KisConfig cfg(false);

    QList<int> splitterSizes = m_d->horzSplitter->sizes();
    if (!cfg.presetStripVisible()) {
        splitterSizes[0] = m_d->presetPanelWidth;
    }

    if (!cfg.scratchpadVisible()) {
        splitterSizes[2] = m_d->scratchPanelWidth;
    }

    cfg.writeList<int>("brushEditorSplitterSizes", splitterSizes);

    QWidget *frame = this->parentWidget();
    cfg.writeEntry("brushEditorWindowGeometry", frame->saveGeometry());

    QWidget::hideEvent(event);
}

void KisPaintOpPresetsEditor::showEvent(QShowEvent *)
{
    KisConfig cfg(false);

    // The center widget's size will be cut off if not re-retrieved.
    m_d->defaultSplitterSizes[1] = m_d->horzSplitter->widget(1)->sizeHint().width();
    QList<int> splitterSizes = cfg.readList<int>("brushEditorSplitterSizes", m_d->defaultSplitterSizes);

    m_d->presetPanelWidth = splitterSizes[0];
    m_d->scratchPanelWidth = splitterSizes[2];

    if (!cfg.presetStripVisible()) {
        splitterSizes[0] = 0;
    }
    if (!cfg.scratchpadVisible()) {
        splitterSizes[2] = 0;
    }

    QWidget *frame = this->parentWidget();
    QByteArray frameGeometry = cfg.readEntry("brushEditorWindowGeometry", QByteArray());
    if (!frameGeometry.isEmpty()) {
        frame->restoreGeometry(frameGeometry);
    }
    else {
        int presetPanelWidth = splitterSizes[0] != 0 ? splitterSizes[0] : m_d->uiWdgPaintOpPresetSettings.showPresetsButton->width();
        int scratchPanelWidth =
            splitterSizes[2] != 0 ? splitterSizes[2] : m_d->uiWdgPaintOpPresetSettings.showScratchpadButton->width();
        const QMargins margins = m_d->layout->contentsMargins();
        int width = presetPanelWidth + splitterSizes[1] + scratchPanelWidth +
            margins.bottom() + margins.left() + margins.top() + margins.right();
        QRect defaultGeometry = QRect(frame->geometry().x(), frame->geometry().y(), width, frame->geometry().height());
        frame->setGeometry(defaultGeometry);
    }
    m_d->horzSplitter->setSizes(splitterSizes);

    Q_EMIT brushEditorShown();
}

void KisPaintOpPresetsEditor::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (parentWidget()) {
        // Make sure resizing doesn't push this widget out of the screen
        QRect screenRect = this->screen()->availableGeometry();
        QRect newPositionRect = kisEnsureInRect(parentWidget()->geometry(), screenRect);
        parentWidget()->setGeometry(newPositionRect);
    }
}

void KisPaintOpPresetsEditor::slotSwitchScratchpad(bool visible)
{
    bool sameVisibility = m_d->uiWdgPaintOpPresetSettings.scratchPad->isVisible() == visible;

    // hide all the internal controls except the toggle button
    m_d->uiWdgPaintOpPresetSettings.scratchPad->setVisible(visible);
    m_d->uiWdgPaintOpPresetSettings.paintPresetIcon->setVisible(visible);
    m_d->uiWdgPaintOpPresetSettings.fillGradient->setVisible(visible);
    m_d->uiWdgPaintOpPresetSettings.fillLayer->setVisible(visible);
    m_d->uiWdgPaintOpPresetSettings.fillSolid->setVisible(visible);
    m_d->uiWdgPaintOpPresetSettings.eraseScratchPad->setVisible(visible);
    m_d->uiWdgPaintOpPresetSettings.scratchpadSidebarLabel->setVisible(visible);

    if (sameVisibility) {
        return;
    }

    QPushButton* showBtn = m_d->uiWdgPaintOpPresetSettings.showScratchpadButton;
    QGroupBox *container = m_d->uiWdgPaintOpPresetSettings.scratchpadControls;

    const QMargins margins = m_d->layout->contentsMargins();
    int emptyContainerWidth = showBtn->width() + margins.left() + margins.right();

    if (visible) {
        showBtn->setIcon(KisIconUtils::loadIcon("arrow-left"));

        container->setMinimumWidth(scratchPadPanelMinWidth);
        container->setMaximumWidth(0xFF'FFFF);

        QList<int> splitterSizes = m_d->horzSplitter->sizes();
        splitterSizes[2] = m_d->scratchPanelWidth > 0 ? m_d->scratchPanelWidth : scratchPadPanelInitWidth;
        QWidget *frame = this->parentWidget();
        QRect currentGeometry = frame->geometry();
        currentGeometry.setRight(currentGeometry.right() + (splitterSizes[2] - emptyContainerWidth));
        frame->setGeometry(currentGeometry);
        m_d->horzSplitter->setSizes(splitterSizes);
    } else {
        showBtn->setIcon(KisIconUtils::loadIcon("arrow-right"));

        container->setMinimumWidth(emptyContainerWidth);
        container->setMaximumWidth(emptyContainerWidth);

        QList<int> splitterSizes = m_d->horzSplitter->sizes();
        m_d->scratchPanelWidth = m_d->scratchPanelWidth > 0 ? splitterSizes[2] : scratchPadPanelInitWidth;
        QWidget *frame = this->parentWidget();
        QRect currentGeometry = frame->geometry();
        currentGeometry.setRight(currentGeometry.right() - (m_d->scratchPanelWidth - emptyContainerWidth));
        frame->setGeometry(currentGeometry);

        splitterSizes[1] = 0xFF'FFFF;
        splitterSizes[2] = emptyContainerWidth;
        m_d->horzSplitter->setSizes(splitterSizes);
    }

    KisConfig cfg(false);
    cfg.setScratchpadVisible(visible);
}

void KisPaintOpPresetsEditor::slotSwitchShowEditor(bool visible) {
    m_d->uiWdgPaintOpPresetSettings.brushEditorSettingsControls->setVisible(visible);
}

void KisPaintOpPresetsEditor::slotSwitchShowPresets(bool visible)
{
    bool sameVisibility = m_d->uiWdgPaintOpPresetSettings.presetsSidebarLabel->isVisible() == visible;

    m_d->uiWdgPaintOpPresetSettings.presetsSidebarLabel->setVisible(visible);
    m_d->uiWdgPaintOpPresetSettings.engineFilterLabel->setVisible(visible);
    m_d->uiWdgPaintOpPresetSettings.brushEngineComboBox->setVisible(visible);
    m_d->uiWdgPaintOpPresetSettings.presetWidget->setVisible(visible);
    m_d->uiWdgPaintOpPresetSettings.newPresetEngineButton->setVisible(visible);
    m_d->uiWdgPaintOpPresetSettings.bnBlacklistPreset->setVisible(visible);

    if (sameVisibility) {
        return;
    }

    QPushButton* showBtn = m_d->uiWdgPaintOpPresetSettings.showPresetsButton;
    QGroupBox *container = m_d->uiWdgPaintOpPresetSettings.presetsContainer;

    const QMargins margins = m_d->layout->contentsMargins();
    int emptyContainerWidth = showBtn->width() + margins.left() + margins.right();

    if (visible) {
        showBtn->setIcon(KisIconUtils::loadIcon("arrow-right"));

        container->setMinimumWidth(brushPresetsPanelMinWidth);
        container->setMaximumWidth(0xFF'FFFF);

        QList<int> splitterSizes = m_d->horzSplitter->sizes();
        splitterSizes[0] = m_d->presetPanelWidth;
        QWidget *frame = this->parentWidget();
        QRect currentGeometry = frame->geometry();
        currentGeometry.setLeft(currentGeometry.left() - (splitterSizes[0] - emptyContainerWidth));
        frame->setGeometry(currentGeometry);
        m_d->horzSplitter->setSizes(splitterSizes);
    } else {
        showBtn->setIcon(KisIconUtils::loadIcon("arrow-left"));

        container->setMinimumWidth(emptyContainerWidth);
        container->setMaximumWidth(emptyContainerWidth);

        QList<int> splitterSizes = m_d->horzSplitter->sizes();
        m_d->presetPanelWidth = m_d->presetPanelWidth > 0 ? splitterSizes[0] : brushPresetsPanelInitWidth;
        QWidget *frame = this->parentWidget();
        QRect currentGeometry = frame->geometry();
        currentGeometry.setLeft(currentGeometry.left() + (m_d->presetPanelWidth - emptyContainerWidth));
        frame->setGeometry(currentGeometry);

        splitterSizes[0] = emptyContainerWidth;
        splitterSizes[1] = 0xFF'FFFF;
        m_d->horzSplitter->setSizes(splitterSizes);
    }

    KisConfig cfg(false);
    cfg.setPresetStripVisible(visible);
}

void KisPaintOpPresetsEditor::slotUpdatePaintOpFilter() {
    QVariant userData = m_d->uiWdgPaintOpPresetSettings.brushEngineComboBox->currentData(); // grab paintOpID from data
    QString filterPaintOpId = userData.toString();

    if (filterPaintOpId == "all_options") {
        filterPaintOpId = "";
    }
    m_d->uiWdgPaintOpPresetSettings.presetWidget->setPresetFilter(filterPaintOpId);
}

void KisPaintOpPresetsEditor::slotSaveBrushPreset() {
    // here we are assuming that people want to keep their existing preset icon. We will just update the
    // settings and save a new copy with the same name.
    // there is a dialog with save options, but we don't need to show it in this situation

    saveDialog->useNewBrushDialog(false); // this mostly just makes sure we keep the existing brush preset name when saving
    const QImage thumbImage = m_d->resourceProvider->currentPreset() ? m_d->resourceProvider->currentPreset()->image() : QImage();
    saveDialog->brushPresetThumbnailWidget->setPresetImage(thumbImage);
    saveDialog->saveScratchPadThumbnailArea(m_d->uiWdgPaintOpPresetSettings.scratchPad->cutoutOverlay());
    saveDialog->loadExistingThumbnail(); // This makes sure we use the existing preset icon when updating the existing brush preset
    saveDialog->showDialog();

    // refresh the view settings so the brush doesn't appear dirty
    // tiar 2021: I'm not sure if it's needed anymore; seems to work without it...
    slotUpdatePresetSettings();
}

void KisPaintOpPresetsEditor::slotSaveNewBrushPreset() {
    saveDialog->useNewBrushDialog(true);
    const QImage thumbImage = m_d->resourceProvider->currentPreset() ? m_d->resourceProvider->currentPreset()->image() : QImage();
    saveDialog->brushPresetThumbnailWidget->setPresetImage(thumbImage);
    saveDialog->saveScratchPadThumbnailArea(m_d->uiWdgPaintOpPresetSettings.scratchPad->cutoutOverlay());
    saveDialog->showDialog();
}

void KisPaintOpPresetsEditor::slotToggleDetach(bool detach)
{
    Q_EMIT toggleDetachState(detach);
    KisConfig cfg(false);
    cfg.setPaintopPopupDetached(detach);
}

void KisPaintOpPresetsEditor::slotUpdateEffectiveLodAvailable(bool value)
{
    if (!m_d->resourceProvider) return;
    m_d->resourceProvider->resourceManager()->setResource(KoCanvasResource::EffectiveLodAvailability, value);
}

void KisPaintOpPresetsEditor::slotCreateNewBrushPresetEngine()
{
    Q_EMIT createPresetFromScratch(sender()->objectName());
}

void KisPaintOpPresetsEditor::updateViewSettings()
{
    m_d->uiWdgPaintOpPresetSettings.presetWidget->updateViewSettings();
}

void KisPaintOpPresetsEditor::currentPresetChanged(KisPaintOpPresetSP preset)
{
    if (preset) {
        m_d->uiWdgPaintOpPresetSettings.presetWidget->setCurrentResource(preset);
        setCurrentPaintOpId(preset->paintOp().id());
    }
}

void KisPaintOpPresetsEditor::updateThemedIcons()
{
    m_d->uiWdgPaintOpPresetSettings.viewOptionButton->setIcon(KisIconUtils::loadIcon("view-choose"));

    m_d->uiWdgPaintOpPresetSettings.paintPresetIcon->setIcon(KisIconUtils::loadIcon("krita_tool_freehand"));
    m_d->uiWdgPaintOpPresetSettings.fillLayer->setIcon(KisIconUtils::loadIcon("document-new"));
    m_d->uiWdgPaintOpPresetSettings.fillLayer->hide();
    m_d->uiWdgPaintOpPresetSettings.fillGradient->setIcon(KisIconUtils::loadIcon("krita_tool_gradient"));
    m_d->uiWdgPaintOpPresetSettings.fillSolid->setIcon(KisIconUtils::loadIcon("krita_tool_color_fill"));
    m_d->uiWdgPaintOpPresetSettings.eraseScratchPad->setIcon(KisIconUtils::loadIcon("edit-delete"));

    m_d->uiWdgPaintOpPresetSettings.newPresetEngineButton->setIcon(KisIconUtils::loadIcon("list-add"));
    m_d->uiWdgPaintOpPresetSettings.bnBlacklistPreset->setIcon(KisIconUtils::loadIcon("deletelayer"));
    m_d->uiWdgPaintOpPresetSettings.reloadPresetButton->setIcon(KisIconUtils::loadIcon("reload-preset-16"));
    m_d->uiWdgPaintOpPresetSettings.renameBrushPresetButton->setIcon(KisIconUtils::loadIcon("document-edit"));
    m_d->uiWdgPaintOpPresetSettings.dirtyPresetIndicatorButton->setIcon(KisIconUtils::loadIcon("warning"));
    m_d->uiWdgPaintOpPresetSettings.brokenPresetIndicatorButton->setIcon(KisIconUtils::loadIcon("broken-preset"));

    m_d->uiWdgPaintOpPresetSettings.newPresetEngineButton->setIcon(KisIconUtils::loadIcon("list-add"));
    m_d->uiWdgPaintOpPresetSettings.bnBlacklistPreset->setIcon(KisIconUtils::loadIcon("deletelayer"));
    //m_d->uiWdgPaintOpPresetSettings.presetChangeViewToolButton->setIcon(KisIconUtils::loadIcon("view-choose"));

    // store if the scratchpad or brush presets are visible in the config
    KisConfig cfg(true);
    if (cfg.presetStripVisible()) {
        //m_d->uiWdgPaintOpPresetSettings.presetsSpacer->changeSize(0,0, QSizePolicy::Ignored,QSizePolicy::Ignored);
        m_d->uiWdgPaintOpPresetSettings.showPresetsButton->setIcon(KisIconUtils::loadIcon("arrow-right"));
    } else {
        //m_d->uiWdgPaintOpPresetSettings.presetsSpacer->changeSize(0,0, QSizePolicy::MinimumExpanding,QSizePolicy::MinimumExpanding);
        m_d->uiWdgPaintOpPresetSettings.showPresetsButton->setIcon(KisIconUtils::loadIcon("arrow-left"));
    }

    if (cfg.scratchpadVisible()) {
        m_d->uiWdgPaintOpPresetSettings.showScratchpadButton->setIcon(KisIconUtils::loadIcon("arrow-left"));
    } else {
        m_d->uiWdgPaintOpPresetSettings.showScratchpadButton->setIcon(KisIconUtils::loadIcon("arrow-right"));
    }
}

void KisPaintOpPresetsEditor::slotUpdatePresetSettings()
{
    if (!m_d->resourceProvider) {
        return;
    }

    if (!m_d->resourceProvider->currentPreset()) {
        return;
    }

    // hide options on UI if we are creating a brush preset from scratch to prevent confusion
    if (m_d->isCreatingBrushFromScratch) {
        m_d->uiWdgPaintOpPresetSettings.dirtyPresetIndicatorButton->setVisible(false);
        m_d->uiWdgPaintOpPresetSettings.brokenPresetIndicatorButton->setVisible(false);
        m_d->uiWdgPaintOpPresetSettings.reloadPresetButton->setVisible(false);
        m_d->uiWdgPaintOpPresetSettings.saveBrushPresetButton->setVisible(false);
        m_d->uiWdgPaintOpPresetSettings.renameBrushPresetButton->setVisible(false);
    } else {
        const bool isPresetDirty = m_d->resourceProvider->currentPreset()->isDirty();

        // don't need to reload or overwrite a clean preset
        m_d->uiWdgPaintOpPresetSettings.dirtyPresetIndicatorButton->setVisible(isPresetDirty);

        {
            bool isBroken = false;
            QString brokenReason;
            const int resourceId = m_d->resourceProvider->currentPreset()->resourceId();
            if (resourceId >= 0) {
                KisResourceModel model(ResourceType::PaintOpPresets);
                QModelIndex index = model.indexForResourceId(resourceId);
                if (index.isValid()) {
                    isBroken = index.data(Qt::UserRole + KisAbstractResourceModel::BrokenStatus).toBool();
                    brokenReason =
                        QString(
                            "<html><body style=\"margin: 20px;\"><h3>%1</h3>"
                            "%2"
                            "</body></html>")
                            .arg(i18n("Resource is broken!"),
                                 index.data(Qt::UserRole + KisAbstractResourceModel::BrokenStatusMessage).toString());
                }
            }

            m_d->uiWdgPaintOpPresetSettings.brokenPresetIndicatorButton->setVisible(isBroken);
            m_d->uiWdgPaintOpPresetSettings.brokenPresetIndicatorButton->setToolTip(brokenReason);
        }
        
        m_d->uiWdgPaintOpPresetSettings.reloadPresetButton->setVisible(isPresetDirty);
        m_d->uiWdgPaintOpPresetSettings.saveBrushPresetButton->setEnabled(isPresetDirty);
        m_d->uiWdgPaintOpPresetSettings.renameBrushPresetButton->setVisible(true);
    }

    // update live preview area in here...
    // don't update the live preview if the widget is not visible.
    if (m_d->uiWdgPaintOpPresetSettings.liveBrushPreviewView->isVisible()) {
        m_d->uiWdgPaintOpPresetSettings.liveBrushPreviewView->setCurrentPreset(m_d->resourceProvider->currentPreset());
        m_d->uiWdgPaintOpPresetSettings.liveBrushPreviewView->requestUpdateStroke();
    }
}
