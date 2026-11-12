// ViPER for Linux — Qt6 control surface.
// Audio runs in PipeWire's independent filter-chain, not inside this GUI.
#include "viper-control.h"
#include "viper-backend-state.h"
#include "gui-backend-exit.h"
#include "sync/gui-control-channel.h"
#include "viper-eq-presets.h"
#include "widgets/eq-overviews.h"
#include "widgets/dynamic-eq-editor.h"
#include "widgets/graphic-eq-editor.h"
#include "panels/bass-panels.h"
#include "panels/eq-tone-panels.h"
#include "panels/dynamics-panels.h"
#include "panels/correction-panels.h"
#include "panels/output-panels.h"
#include "panels/spatial-panels.h"
#include "widgets/effect-widgets.h"
#include "widgets/irs-combo-box.h"
#include "viper-theme.h"
#include "viper-named-presets.h"
#include "viper-preset-dialog.h"
#include "viper-build-info.h"
#include "gui-single-instance.h"
#include "gui-startup-notification.h"
#include <QApplication>
#include <QCoreApplication>
#include <QAction>
#include <QButtonGroup>
#include <QDialogButtonBox>
#include <QDesktopServices>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QRadioButton>
#include <QSaveFile>
#include <QSystemTrayIcon>
#include <QUrl>
#include <QByteArray>
#include <QGuiApplication>
#include <QScreen>
#include <QCloseEvent>
#include <QEvent>
#include <QWindowStateChangeEvent>
#include <QIcon>
#include <QPixmap>
#include <QStyleHints>
#include <QtGlobal>
#include <QColor>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPalette>
#include <QProcess>
#include <QCheckBox>
#include <QComboBox>
#include <QListWidget>
#include <QUuid>
#include <QAbstractItemView>
#include <QFileDialog>
#include <QFileSystemWatcher>
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QMessageBox>
#include <QDialog>
#include <QInputDialog>
#include <QLineEdit>
#include <QMouseEvent>
#include <QTextBrowser>
#include <QPlainTextEdit>
#include <QTextOption>
#include <QFormLayout>
#include <QFrame>
#include <QFont>
#include <QGroupBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMainWindow>
#include <QStyle>
#include <QPushButton>
#include <QSettings>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSlider>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QTimer>
#include <QTabWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <QTableWidgetItem>
#include <QTabBar>
#include <QToolButton>
#include <QWheelEvent>
#include <QVBoxLayout>
#include <QWidget>
#include <QString>
#include <algorithm>
#include <array>
#include <QStringList>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <functional>
#include <vector>
#include <utility>
#include <cmath>

class ViperWindow final : public QMainWindow {
public:
    ViperWindow() : settings_("ViPERforLinux", "ViPERClean") {
        setObjectName("viperWindow");
        setWindowTitle("Viper4Linux");
        // Avoid duplicating the large logo in Cinnamon's native title bar.
        // Keep the blue QApplication, .desktop and system-tray icons intact.
        // Taskbar behavior with a transparent per-window icon needs live testing.
        QPixmap titleBarIcon(1, 1);
        titleBarIcon.fill(Qt::transparent);
        setWindowIcon(QIcon(titleBarIcon));
        setMinimumSize(440, 440);
        resize(740, 800);
        auto* central = new QWidget(this);
        central->setObjectName("viperCentral");
        auto* layout = new QVBoxLayout(central);
        layout->setSpacing(10);

        auto* title = new QLabel("Viper4Linux", central);
        QFont font = title->font();
        font.setPointSize(font.pointSize() + 6);
        font.setBold(true);
        title->setFont(font);
        // The logo fills both header rows, while Audio Output begins beneath
        // the title, not under the logo. The lower text spans all right columns.
        auto* headerGrid = new QGridLayout();
        headerGrid->setHorizontalSpacing(8);
        headerGrid->setVerticalSpacing(2);
        header_icon_ = new QLabel(central);
        header_icon_->setObjectName("viperHeaderLogo");
        header_icon_->setFixedSize(58, 58);
        header_icon_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        header_icon_->setAccessibleName("Viper4Linux logo");
        header_icon_->setAlignment(Qt::AlignCenter);
        headerGrid->addWidget(header_icon_, 0, 0, 2, 1, Qt::AlignVCenter);
        headerGrid->addWidget(title, 0, 1);
        auto* themeLabel = new QLabel("Theme", central);
        headerGrid->addWidget(themeLabel, 0, 2);
        theme_selector_ = new QComboBox(central);
        theme_selector_->setObjectName("viperThemeSelector");
        theme_selector_->setAccessibleName("Interface color scheme");
        theme_selector_->setToolTip("Change only the GUI colors; audio settings are unaffected.");
        theme_selector_->addItem("Modern teal", "modern");
        theme_selector_->addItem("Classic Viper", "classic-v4a");
        theme_selector_->setCurrentIndex(
            settings_.value("ui/theme", "modern").toString() == "classic-v4a" ? 1 : 0);
        // Allow room for the longest label plus the custom arrow at different
        // font scales; the old 140px width clipped "Classic Viper" on Cinnamon.
        theme_selector_->setFixedWidth(qMax(168,
            theme_selector_->fontMetrics().horizontalAdvance("Classic Viper") + 62));
        headerGrid->addWidget(theme_selector_, 0, 3);
        // Single compact menu instead of separate Help and About buttons.
        auto* menuButton = new QToolButton(central);
        menuButton->setObjectName("viperApplicationMenuButton");
        menuButton->setText(QString::fromUtf8("☰"));
        menuButton->setAccessibleName("Application menu");
        menuButton->setToolTip("Restart backend, preferences, library, help, and quit");
        menuButton->setFixedWidth(34);
        menuButton->setPopupMode(QToolButton::InstantPopup);
        menuButton->setMenu(makeApplicationMenu(menuButton));
        headerGrid->addWidget(menuButton, 0, 4);
        // Read-only status from the background monitor. No CLI subprocesses.
        audio_output_label_ = new QLabel("Audio Output: detecting…", central);
        audio_output_label_->setObjectName("viperCurrentAudioOutput");
        audio_output_label_->setStyleSheet("color: palette(mid);");
        audio_output_label_->setToolTip("Current physical Audio Output; enable independent settings under Preferences.");
        headerGrid->addWidget(audio_output_label_, 1, 1, 1, 4);
        headerGrid->setColumnStretch(1, 1);
        layout->addLayout(headerGrid);

        // Master bypass lives outside both tabs, as in the previous working UI.
        auto* masterCard = new QFrame(central);
        masterCard->setObjectName("viperMasterCard");
        const bool darkUi = palette().color(QPalette::Window).lightness() < 130;
        const QString cardBg = darkUi ? "#293a3e" : "#ebf3f5";
        const QString cardBorder = darkUi ? "#405659" : "#d8e7e9";
        masterCard->setStyleSheet(QString("QFrame#viperMasterCard { background:%1; border:1px solid %2; border-radius:16px; }")
                                  .arg(cardBg, cardBorder));
        auto* masterRow = new QHBoxLayout(masterCard);
        masterRow->setContentsMargins(18, 13, 18, 13);
        auto* masterText = new QVBoxLayout();
        auto* masterTitle = new QLabel("Master effects", masterCard);
        QFont masterFont = masterTitle->font();
        masterFont.setBold(true);
        masterFont.setPointSize(masterFont.pointSize() + 1);
        masterTitle->setFont(masterFont);
        auto* masterDetail = new QLabel("Global processing bypass", masterCard);
        masterDetail->setStyleSheet("color: palette(mid);");
        masterText->addWidget(masterTitle);
        masterText->addWidget(masterDetail);
        masterRow->addLayout(masterText, 1);
        global_ = new EffectSwitch(masterCard);
        global_->setAccessibleName("Enable all effects globally");
        global_->setToolTip("Bypass all DSP effects without changing their individual settings or audio routing.");
        masterRow->addWidget(global_);
        layout->addWidget(masterCard);

        // Major categories use native tabs; each tab scrolls independently.
        // Master switch, presets and backend status stay outside the scrolling tabs.
        tabs_ = new QTabWidget(central);
        // Match the enabled effect switches: the active tab is dark teal with
        // white text, while inactive tabs remain quieter.  Presentation only:
        // changing tabs does not touch any DSP control or effect enable flag.
        tabs_->tabBar()->setExpanding(false);
        tabs_->tabBar()->setDrawBase(false);
        tabs_->tabBar()->setUsesScrollButtons(true); // All six categories remain reachable on small windows.
        const QString tabInactiveBg = darkUi ? "#25383c" : "#e4edef";
        const QString tabInactiveText = darkUi ? "#d9e6e8" : "#476268";
        const QString tabInactiveBorder = darkUi ? "#476268" : "#bfcfd2";
        const QString tabHoverBg = darkUi ? "#344d52" : "#d4e5e8";
        tabs_->tabBar()->setStyleSheet(QString(R"(
            QTabBar::tab {
                background-color: %1;
                color: %2;
                border: 1px solid %3;
                border-radius: 10px;
                padding: 9px 13px;
                min-width: 80px;
                margin-right: 7px;
                margin-bottom: 5px;
                font-size: 14px;
                font-weight: 600;
            }
            QTabBar::tab:selected {
                background-color: #236e76;
                color: #ffffff;
                border-color: #236e76;
            }
            QTabBar::tab:!selected:hover {
                background-color: %4;
            }
        )").arg(tabInactiveBg, tabInactiveText, tabInactiveBorder, tabHoverBg));
        // Output is the first category. The saved tab uses a stable name rather
        // than an index so future tab additions cannot silently switch views.
        // Six-tab public proof-of-concept layout. Empty categories are real
        // roadmap placeholders, never simulated DSP controls.
        auto* outputLayout = addEffectsTab("Output");
        auto* bassLayout = addEffectsTab("Bass");
        auto* effectLayout = addEffectsTab("EQ & Tone");
        auto* spatialLayout = addEffectsTab("Spatial");
        auto* dynamicsLayout = addEffectsTab("Dynamics");
        auto* correctionLayout = addEffectsTab("Sound Correction");

        // The EQ and Dynamic EQ controls are composed separately; the
        // editors, preset state, and all signal callbacks remain in this window.
        const auto eqToneEq = viper_panels::createEqAndDynamicPanels(tabs_);
        eq_enabled_ = eqToneEq.eq_enabled;
        eq_mode_ = eqToneEq.eq_mode;
        eq_preset_ = eqToneEq.eq_preset;
        eq_overview_ = eqToneEq.eq_overview;
        deq_enabled_ = eqToneEq.deq_enabled;
        deq_count_ = eqToneEq.deq_count;
        deq_overview_ = eqToneEq.deq_overview;
        eq_delete_button_ = eqToneEq.eqDelete;
        auto* eq_save = eqToneEq.eqSave;
        auto* eq_delete = eqToneEq.eqDelete;
        addEffectPanel(effectLayout, eqToneEq.eqGroup, "equalizer", eq_enabled_);
        addEffectPanel(effectLayout, eqToneEq.deqGroup, "dynamic_eq", deq_enabled_);

        const auto editorWidgets = viper_eq_ui::createDynamicEqEditor(this);
        deq_editor_ = editorWidgets.dialog;
        deq_editor_count_ = editorWidgets.bandCount;
        deq_editor_graph_ = editorWidgets.graph;
        deq_table_ = editorWidgets.table;
        deq_selected_label_ = editorWidgets.selectedLabel;
        deq_band_select_ = editorWidgets.bandSelector;
        deq_frequency_ = editorWidgets.frequency;
        deq_type_ = editorWidgets.filterType;
        deq_q_ = editorWidgets.q;
        deq_gain_ = editorWidgets.gain;
        deq_threshold_ = editorWidgets.threshold;
        deq_attack_ = editorWidgets.attack;
        deq_release_ = editorWidgets.release;
        auto* deq_spread = editorWidgets.spread;
        deq_overview_->selectBand = [this](int index) {
            if (index >= 0 && index < deq_active_count_)
                deq_band_select_->setCurrentIndex(index);
        };
        deq_overview_->openEditor = [this] {
            deq_editor_->show(); deq_editor_->raise(); deq_editor_->activateWindow();
        };
        deq_editor_graph_->selectBand = [this](int index) {
            if (index >= 0 && index < deq_active_count_)
                deq_band_select_->setCurrentIndex(index);
        };
        connect(deq_table_, &QTableWidget::cellClicked, this,
                [this](int row, int) {
            if (row >= 0 && row < deq_active_count_)
                deq_band_select_->setCurrentIndex(row);
        });
        connect(deq_table_, &QTableWidget::currentCellChanged, this,
                [this](int row, int, int, int) {
            if (row >= 0 && row < deq_active_count_)
                deq_band_select_->setCurrentIndex(row);
        });
        connect(deq_table_, &QTableWidget::itemChanged, this,
                [this](QTableWidgetItem* item) { commitDeqTableItem(item); });

        // Retained nonmodal editor; state and callbacks remain with the window.
        const auto graphicEditor = viper_eq_ui::createGraphicEqEditor(this);
        eq_editor_ = graphicEditor.dialog;
        eq_fader_container_ = graphicEditor.faderContainer;
        eq_columns_ = graphicEditor.columns;
        eq_faders_ = graphicEditor.faders;
        eq_values_ = graphicEditor.values;
        eq_frequencies_ = graphicEditor.frequencies;
        auto* eq_reset = graphicEditor.reset;
        eq_overview_->openEditor = [this] {
            eq_editor_->show();
            // The QScrollArea may show its children on the first dialog show.
            // Enforce the active band layout after that transition, too.
            syncGraphicEqEditorColumns();
            eq_editor_->raise();
            eq_editor_->activateWindow();
        };
        // Read the saved preset identifier BEFORE populating the combo box.
        // populateEqPresets() persists selection, so doing this later would
        // silently overwrite the previous launch's choice with "Custom".
        migrateEqPrecision();
        eq_selected_preset_ = settings_.value("eq/selected_preset", "custom").toString();
        populateEqPresets();
        connect(eq_save, &QPushButton::clicked, this, [this] { saveUserEqPreset(); });
        connect(eq_delete, &QPushButton::clicked, this, [this] { deleteUserEqPreset(); });
        connect(eq_preset_, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int index) {
            if (index < 0) return;
            const QString id = eq_preset_->itemData(index).toString();
            eq_selected_preset_ = id;
            if (id != "custom") applyEqPreset(id);
            else { loadEqLayout(); push(); }
            updateEqDeleteButton();
        });

        const auto dynamicsFet = viper_panels::createFetPanel(
            tabs_, [this](QFormLayout* form, const char* label, int min, int max) {
                return addSlider(form, label, min, max);
            });
        fet_enabled_ = dynamicsFet.fet_enabled;
        fet_threshold_ = dynamicsFet.fet_threshold;
        fet_ratio_ = dynamicsFet.fet_ratio;
        fet_knee_ = dynamicsFet.fet_knee;
        fet_gain_ = dynamicsFet.fet_gain;
        fet_attack_ = dynamicsFet.fet_attack;
        fet_release_ = dynamicsFet.fet_release;
        addEffectPanel(dynamicsLayout, dynamicsFet.group, "fet", fet_enabled_);
        const auto dynamicsMb = viper_panels::createMultibandPanel(
            tabs_, [this](QFormLayout* form, const char* label, int min, int max) {
                return addSlider(form, label, min, max);
            }, mb_crossovers_, mb_band_ranges_, mb_bands_);
        mb_enabled_ = dynamicsMb.mb_enabled;
        mb_band_count_ = dynamicsMb.mb_band_count;
        mb_band_tabs_ = dynamicsMb.mb_band_tabs;
        mb_crossover_panel_ = dynamicsMb.mb_crossover_panel;
        auto* mb_crossover_toggle = dynamicsMb.crossoverToggle;
        connect(mb_crossover_toggle, &QToolButton::toggled, this,
                [this, mb_crossover_toggle](bool expanded) {
            mb_crossover_panel_->setVisible(expanded);
            mb_crossover_toggle->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
        });
        mb_band_tabs_->setCurrentIndex(std::clamp(settings_.value("ui/mb_selected_band", 0).toInt(), 0, 4));
        connect(mb_band_tabs_, &QTabWidget::currentChanged, this, [this](int index) {
            if (index >= 0) settings_.setValue("ui/mb_selected_band", index);
        });
        addEffectPanel(dynamicsLayout, dynamicsMb.group, "multiband", mb_enabled_);
        // UI construction extracted; keep library state, watchers and
        // all import/refresh callbacks in the main window.
        const auto correctionDdc = viper_panels::createDdcPanel(tabs_);
        ddc_enabled_ = correctionDdc.ddc_enabled;
        ddc_library_ = correctionDdc.ddc_library;
        ddc_file_name_ = correctionDdc.ddc_file_name;
        ddc_status_ = correctionDdc.ddc_status;
        auto* importDdc = correctionDdc.importDdc;
        auto* refreshDdc = correctionDdc.refreshDdc;
        ddc_library_->beforeOpen = [this] {
            if (ddc_library_dirty_) refreshDdcLibrary();
        };
        addEffectPanel(correctionLayout, correctionDdc.group, "ddc", ddc_enabled_);
        refreshDdcLibrary();
        ddc_watcher_ = new QFileSystemWatcher(this);
        ddc_watcher_->addPath(ddcDirectory());
        connect(ddc_watcher_, &QFileSystemWatcher::directoryChanged, this,
                [this](const QString&) {
            ddc_library_dirty_ = true;
            if (!ddc_library_->popupIsOpen()) refreshDdcLibrary();
        });
        connect(ddc_library_, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int index) {
            if (index >= 0) selectDdcProfile(ddc_library_->currentData().toString());
        });
        connect(refreshDdc, &QPushButton::clicked, this, [this] { refreshDdcLibrary(); });
        connect(importDdc, &QPushButton::clicked, this, [this] {
            const QString selected = QFileDialog::getOpenFileName(this, "Import ViPER-DDC profile",
                ddcDirectory(), "ViPER correction (*.vdc *.VDC)");
            if (selected.isEmpty()) return;
            const QFileInfo file(selected);
            if (!file.isFile() || !file.isReadable() ||
                file.suffix().compare("vdc", Qt::CaseInsensitive) != 0) {
                QMessageBox::warning(this, "VDC import", "Select a readable .vdc profile.");
                return;
            }
            if (!QDir().mkpath(ddcDirectory())) {
                QMessageBox::warning(this, "VDC import", "Could not create VDC library directory.");
                return;
            }
            QString destination = QDir(ddcDirectory()).filePath(file.fileName());
            if (QFileInfo(destination).canonicalFilePath() != file.canonicalFilePath()) {
                unsigned serial = 2;
                while (QFile::exists(destination))
                    destination = QDir(ddcDirectory()).filePath(file.completeBaseName() +
                        " (" + QString::number(serial++) + ")." + file.suffix());
                if (!QFile::copy(selected, destination)) {
                    QMessageBox::warning(this, "VDC import", "Could not copy the selected profile.");
                    return;
                }
            }
            selectDdcProfile(destination);
        });
        const auto correctionSpeaker = viper_panels::createSpeakerCorrectionPanel(tabs_);
        speaker_enabled_ = correctionSpeaker.speaker_enabled;
        addEffectPanel(correctionLayout, correctionSpeaker.group, "speaker_correction", speaker_enabled_);
        const auto correctionConvolver = viper_panels::createConvolverPanel(
            tabs_, [this](QFormLayout* form, const char* label, int min, int max) {
                return addSlider(form, label, min, max);
            });
        convolver_enabled_ = correctionConvolver.convolver_enabled;
        convolver_library_ = correctionConvolver.convolver_library;
        convolver_full_name_ = correctionConvolver.convolver_full_name;
        convolver_cross_ = correctionConvolver.convolver_cross;
        convolver_status_ = correctionConvolver.convolver_status;
        auto* browse = correctionConvolver.browse;
        auto* refresh = correctionConvolver.refresh;
        convolver_library_->beforeOpen = [this] {
            // Refresh only after closing and reopening if the library changed
            // while the dropdown was open. Never reset an open popup's scroll.
            if (irs_library_dirty_) refreshIrsLibrary();
        };
        addEffectPanel(correctionLayout, correctionConvolver.group, "convolver", convolver_enabled_);
        refreshIrsLibrary();
        irs_watcher_ = new QFileSystemWatcher(this);
        irs_watcher_->addPath(irsDirectory());
        connect(irs_watcher_, &QFileSystemWatcher::directoryChanged, this,
                [this](const QString&) {
            // A closed library updates as soon as Qt reports a folder change.
            // An open popup is never rebuilt underneath the user.
            irs_library_dirty_ = true;
            if (!convolver_library_->popupIsOpen()) refreshIrsLibrary();
        });
        connect(convolver_library_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
                if (index < 0) return;
                selectIrPath(convolver_library_->currentData().toString());
            });
        connect(refresh, &QPushButton::clicked, this, [this] { refreshIrsLibrary(); settings_.sync(); requestIrsLoad(); });
        connect(browse, &QPushButton::clicked, this, [this] {
            const QStringList selected = QFileDialog::getOpenFileNames(this, "Import impulse responses",
                irsDirectory(), "Impulse responses (*.irs *.IRS *.wav *.WAV);;All files (*)");
            if (selected.isEmpty()) return;
            if (!QDir().mkpath(irsDirectory())) {
                QMessageBox::warning(this, "IRS import", "Could not create the IRS library directory.");
                return;
            }
            QString firstImported;
            QStringList failures;
            for (const QString& selectedFile : selected) {
                const QFileInfo file(selectedFile);
                if (!file.isFile() || !file.isReadable() ||
                    (file.suffix().compare("irs", Qt::CaseInsensitive) != 0 &&
                     file.suffix().compare("wav", Qt::CaseInsensitive) != 0)) {
                    failures << file.fileName() + ": not a readable .irs or .wav file";
                    continue;
                }
                QString destination = QDir(irsDirectory()).filePath(file.fileName());
                // Browsing a library file is a selection, not an import.
                if (QFileInfo(destination).canonicalFilePath() != file.canonicalFilePath()) {
                    const QString base = file.completeBaseName(), suffix = file.suffix();
                    unsigned serial = 2;
                    while (QFile::exists(destination)) {
                        destination = QDir(irsDirectory()).filePath(base + " (" + QString::number(serial++) + ")" +
                            (suffix.isEmpty() ? QString() : "." + suffix));
                    }
                    if (!QFile::copy(selectedFile, destination)) {
                        failures << file.fileName() + ": could not copy";
                        continue;
                    }
                }
                if (firstImported.isEmpty()) firstImported = destination;
            }
            // Only the first imported file becomes active; others appear in the
            // library for immediate switching. Keep previous choice on failure.
            if (!firstImported.isEmpty()) selectIrPath(firstImported);
            refreshIrsLibrary();
            if (!failures.isEmpty()) {
                QMessageBox::warning(this, "IRS import",
                    "Some files could not be imported:\n" + failures.join("\n"));
            }
        });

        // Bass widget construction moved; card wrappers, persistence,
        // precise DSP values and shared-memory subscriptions remain here.
        const auto bassPanels = viper_panels::createBassPanels(
            tabs_, [this](QFormLayout* form, const char* label, int min, int max) {
                return addSlider(form, label, min, max);
            });
        enabled_ = bassPanels.enabled;
        bass_mode_ = bassPanels.bass_mode;
        strength_ = bassPanels.strength;
        frequency_ = bassPanels.frequency;
        mono_enabled_ = bassPanels.mono_enabled;
        mono_mode_ = bassPanels.mono_mode;
        mono_strength_ = bassPanels.mono_strength;
        mono_frequency_ = bassPanels.mono_frequency;
        psycho_enabled_ = bassPanels.psycho_enabled;
        psycho_cutoff_ = bassPanels.psycho_cutoff;
        psycho_intensity_ = bassPanels.psycho_intensity;
        psycho_order_ = bassPanels.psycho_order;
        psycho_original_ = bassPanels.psycho_original;
        dyn_enabled_ = bassPanels.dyn_enabled;
        dyn_xlo_ = bassPanels.dyn_xlo;
        dyn_xhi_ = bassPanels.dyn_xhi;
        dyn_ylo_ = bassPanels.dyn_ylo;
        dyn_yhi_ = bassPanels.dyn_yhi;
        dyn_side_lo_ = bassPanels.dyn_side_lo;
        dyn_side_hi_ = bassPanels.dyn_side_hi;
        dyn_strength_ = bassPanels.dyn_strength;
        addEffectPanel(bassLayout, bassPanels.bassGroup, "bass", enabled_);
        addEffectPanel(bassLayout, bassPanels.monoGroup, "bass_mono", mono_enabled_);
        addEffectPanel(bassLayout, bassPanels.psychoGroup, "psycho_bass", psycho_enabled_);
        addEffectPanel(bassLayout, bassPanels.dynamicGroup, "dynamic_bass", dyn_enabled_);

        const auto eqToneClarity = viper_panels::createClaritySpectrumPanels(
            central, [this](QFormLayout* form, const char* label, int min, int max) {
                return addSlider(form, label, min, max);
            });
        clarity_enabled_ = eqToneClarity.clarity_enabled;
        clarity_mode_ = eqToneClarity.clarity_mode;
        clarity_gain_ = eqToneClarity.clarity_gain;
        spectrum_enabled_ = eqToneClarity.spectrum_enabled;
        spectrum_frequency_ = eqToneClarity.spectrum_frequency;
        spectrum_exciter_ = eqToneClarity.spectrum_exciter;
        addEffectPanel(effectLayout, eqToneClarity.clarityGroup, "clarity", clarity_enabled_);
        addEffectPanel(effectLayout, eqToneClarity.spectrumGroup, "spectrum", spectrum_enabled_);

        const auto spatialPanels = viper_panels::createSpatialPanels(tabs_,
            [this](QFormLayout* form, const char* label, int min, int max) {
                return addSlider(form, label, min, max);
            });
        crossfeed_enabled_ = spatialPanels.crossfeed_enabled;
        crossfeed_mode_ = spatialPanels.crossfeed_mode;
        headphone_enabled_ = spatialPanels.headphone_enabled;
        headphone_quality_ = spatialPanels.headphone_quality;
        diff_enabled_ = spatialPanels.diff_enabled;
        diff_delay_ = spatialPanels.diff_delay;
        diff_reverse_ = spatialPanels.diff_reverse;
        diff_mix_ = spatialPanels.diff_mix;
        diff_lowpass_ = spatialPanels.diff_lowpass;
        field_enabled_ = spatialPanels.field_enabled;
        field_widen_ = spatialPanels.field_widen;
        field_mid_ = spatialPanels.field_mid;
        field_depth_ = spatialPanels.field_depth;
        reverb_enabled_ = spatialPanels.reverb_enabled;
        reverb_room_ = spatialPanels.reverb_room;
        reverb_width_ = spatialPanels.reverb_width;
        reverb_damp_ = spatialPanels.reverb_damp;
        reverb_wet_ = spatialPanels.reverb_wet;
        reverb_dry_ = spatialPanels.reverb_dry;
        imager_enabled_ = spatialPanels.imager_enabled;
        imager_low_ = spatialPanels.imager_low;
        imager_mid_ = spatialPanels.imager_mid;
        imager_high_ = spatialPanels.imager_high;
        imager_low_xover_ = spatialPanels.imager_low_xover;
        imager_high_xover_ = spatialPanels.imager_high_xover;
        addEffectPanel(spatialLayout, spatialPanels.crossfeedGroup, "crossfeed", crossfeed_enabled_);
        addEffectPanel(spatialLayout, spatialPanels.headphoneGroup, "headphone", headphone_enabled_);
        addEffectPanel(spatialLayout, spatialPanels.diffGroup, "differential", diff_enabled_);
        addEffectPanel(spatialLayout, spatialPanels.fieldGroup, "field", field_enabled_);
        addEffectPanel(spatialLayout, spatialPanels.reverbGroup, "reverb", reverb_enabled_);
        addEffectPanel(spatialLayout, spatialPanels.imagerGroup, "imager", imager_enabled_);

        const auto eqToneTube = viper_panels::createTubeAnalogXPanels(
            tabs_, [this](QFormLayout* form, const char* label, int min, int max) {
                return addSlider(form, label, min, max);
            });
        tube_enabled_ = eqToneTube.tube_enabled;
        tube_model_ = eqToneTube.tube_model;
        tube_drive_ = eqToneTube.tube_drive;
        tube_mix_ = eqToneTube.tube_mix;
        analogx_enabled_ = eqToneTube.analogx_enabled;
        analogx_mode_ = eqToneTube.analogx_mode;
        addEffectPanel(effectLayout, eqToneTube.tubeGroup, "tube", tube_enabled_);
        addEffectPanel(effectLayout, eqToneTube.analogxGroup, "analogx", analogx_enabled_);

        // Audio enters native ViPERDSP at unity: no adapter input trim.

        // Output effect widget construction is separate; persistence and all
        // backend/CLI connections remain in ViperWindow.
        const auto outputPanels = viper_panels::createOutputPanels(
            tabs_, [this](QFormLayout* form, const char* label, int minimum, int maximum) {
                return addSlider(form, label, minimum, maximum);
            });
        limiter_indicator_ = outputPanels.limiter_indicator;
        limiter_threshold_ = outputPanels.limiter_threshold;
        limiter_gain_ = outputPanels.limiter_gain;
        limiter_pan_ = outputPanels.limiter_pan;
        agc_enabled_ = outputPanels.agc_enabled;
        agc_strength_ = outputPanels.agc_strength;
        agc_max_gain_ = outputPanels.agc_max_gain;
        agc_threshold_ = outputPanels.agc_threshold;
        lufs_enabled_ = outputPanels.lufs_enabled;
        lufs_target_ = outputPanels.lufs_target;
        lufs_gain_ = outputPanels.lufs_gain;
        lufs_speed_ = outputPanels.lufs_speed;
        output_warning_ = outputPanels.output_warning;
        addEffectPanel(outputLayout, outputPanels.limiterGroup, "limiter", limiter_indicator_);
        addEffectPanel(outputLayout, outputPanels.agcGroup, "agc", agc_enabled_);
        addEffectPanel(outputLayout, outputPanels.lufsGroup, "lufs", lufs_enabled_);
        outputLayout->insertWidget(outputLayout->count() - 1, output_warning_);


        // Keep the primary interface small; all named-preset operations and
        // the guarded defaults reset live together in one manager dialog.
        // Use the existing actions row: Restart Backend on the left,
        // Presets on the right. No additional footer height is needed.
        auto* actions = new QHBoxLayout();
        audio_restart_button_ = new QPushButton("Restart Backend", central);
        audio_restart_button_->setObjectName("viperRestartAudioButton");
        audio_restart_button_->setToolTip(
            "Restart the ViPER backend and Audio Output monitor without closing the GUI.");
        actions->addWidget(audio_restart_button_);
        auto* presetsButton = new QPushButton("Presets…", central);
        presetsButton->setObjectName("viperNamedPresetsButton");
        presetsButton->setToolTip("Save, load, rename or delete complete effect configurations.");
        actions->addStretch();
        actions->addWidget(presetsButton);

        // The tabs own all remaining vertical space. Keep this faint divider
        // pinned immediately above both footer buttons even on short tabs.
        // Do not alter the main window size or the footer status placement.
        auto* footerDivider = new QFrame(central);
        footerDivider->setObjectName("viperFooterDivider");
        footerDivider->setFrameShape(QFrame::HLine);
        footerDivider->setFrameShadow(QFrame::Plain);
        footerDivider->setLineWidth(1);
        footerDivider->setFixedHeight(1);
        footerDivider->setStyleSheet("QFrame#viperFooterDivider { background-color: #bcc7cf; border: none; }");
        auto* footerControls = new QVBoxLayout();
        footerControls->setContentsMargins(0, 0, 0, 0);
        footerControls->setSpacing(6);
        footerControls->addWidget(footerDivider);
        footerControls->addLayout(actions);
        layout->addWidget(tabs_, 1);
        layout->addLayout(footerControls);
        connect(presetsButton, &QPushButton::clicked, this, [this]{ showPresetManager(); });
        connect(audio_restart_button_, &QPushButton::clicked, this,
                [this] { restartAudioEngine(); });

        // New stable name key deliberately ignores the former numeric index
        // (Enhancements=0, Surround=1, Output=2). On first launch show Output.
        // Preserve the user's last category even though two display names
        // changed. No existing effect, preset or panel key is migrated.
        QString initialTabName = settings_.value("ui/active_tab_name", "Output").toString();
        if (initialTabName == "Enhancements") initialTabName = "EQ & Tone";
        if (initialTabName == "Surround") initialTabName = "Spatial";
        if (initialTabName == "IR & Correction" || initialTabName == "IR_Correction") initialTabName = "Sound Correction";
        if (initialTabName == "EQ_Tone") initialTabName = "EQ & Tone";
        for (int tab = 0; tab < tabs_->count(); ++tab) {
            if (tabs_->tabText(tab) == initialTabName ||
                (tabs_->tabText(tab) == "EQ && Tone" && initialTabName == "EQ & Tone")) {
                tabs_->setCurrentIndex(tab);
                break;
            }
        }
        connect(tabs_, &QTabWidget::currentChanged, this, [this](int index) {
            settings_.setValue("ui/active_tab_name",
                tabs_->tabText(index) == "EQ && Tone" ? "EQ & Tone" : tabs_->tabText(index));
        });
        // Status returns to its own compact line below the actions row.
        status_ = new QLabel("Checking backend...", central);
        status_->setWordWrap(true); // Only long recovery/offline messages wrap.
        layout->addWidget(status_);
        // Both the button and the application-menu action use one restart handler.
        setCentralWidget(central);

        save_timer_ = new QTimer(this);
        save_timer_->setSingleShot(true);
        connect(save_timer_, &QTimer::timeout, this, [this]{ settings_.sync(); });

        loadSavedEffectSettings();
        connectEffectSignals(eq_reset, deq_spread);
        auto* timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, [this]{
            if (audio_restart_process_) return;
            updateAudioOutputDisplay();
            pollBackend();
        });
        timer->start(500);
        setupThemeMonitoring();
        restoreWindowSizes();
        setupTray();
        pollBackend();
    }
    // Public entry used only when a second launch asks us to show this window.
    void activateFromSecondLaunch() { restoreFromTray(); }
    void showAtStartup() {
        const QString choice = settings_.value("ui/startupBehavior", "show").toString();
        if (choice == "tray") {
            if (trayAvailable()) return; // Tray exists, so the window can stay hidden.
            show(); // Never hide without a working tray.
            statusMessage("System tray unavailable; showing the main window instead.");
        } else if (choice == "minimized") {
            // Explicit start-on-taskbar must override the independent
            // minimize-to-tray preference.
            force_taskbar_minimize_ = true;
            showMinimized();
            QTimer::singleShot(0, this, [this]{ force_taskbar_minimize_ = false; });
        } else {
            show();
        }
    }
protected:
    void closeEvent(QCloseEvent* event) override {
        const QString choice = settings_.value("ui/closeBehavior", "quit").toString();
        if (choice == "tray") {
            saveUiGeometry();
            if (!trayAvailable()) {
                event->ignore();
                statusMessage("System tray unavailable; the window stays open.");
                return;
            }
            event->ignore();
            hide();
            return;
        }
        if (choice == "minimized") {
            saveUiGeometry();
            event->ignore();
            force_taskbar_minimize_ = true;
            showMinimized();
            QTimer::singleShot(0, this, [this]{ force_taskbar_minimize_ = false; });
            return;
        }
        // Normal X-to-quit and menu/tray Quit share the same backend choice.
        // Ignore this close event: Cancel must leave the window alive.
        event->ignore();
        quitGui();
    }
    void changeEvent(QEvent* event) override {
        // The window manager has already iconified this native top-level window.
        // On Cinnamon/X11, clearing Qt::WindowMinimized *after* hide() can
        // recreate a black, taskbar-listed ghost window. Leave the minimized
        // state alone while hidden; restoreFromTray() resolves it on demand.
        const bool is_state_change = event->type() == QEvent::WindowStateChange;
        const bool was_maximized = is_state_change &&
            static_cast<QWindowStateChangeEvent*>(event)->oldState().testFlag(Qt::WindowMaximized);
        QMainWindow::changeEvent(event);
        // This handler requires the X11 window-state protocol. Native Wayland
        // cannot notify clients of compositor-side minimization, but an
        // explicitly selected XWayland/xcb GUI can still use this feature.
        if (QGuiApplication::platformName() != QLatin1String("xcb")) return;
        if (!is_state_change || !isMinimized() || force_taskbar_minimize_
            || !settings_.value("ui/minimizeToTray", false).toBool()) return;
        if (!trayAvailable()) return; // Keep native taskbar behavior without a tray.
        // Deferring until after the WM's minimize event prevents reentrant
        // state changes. Only withdraw the window from the WM; don't remap it.
        QTimer::singleShot(0, this, [this, was_maximized] {
            if (!isMinimized() || !isVisible() || !trayAvailable()) return;
            tray_restore_maximized_ = was_maximized;
            hide();
        });
    }
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (event->type() == QEvent::Hide) {
            if (watched == eq_editor_)
                settings_.setValue("ui/graphicEqEditorSize", eq_editor_->size());
            else if (watched == deq_editor_)
                settings_.setValue("ui/dynamicEqEditorSize", deq_editor_->size());
        }
        return QMainWindow::eventFilter(watched, event);
    }
private:
#include "window/window-presets.inc"
#include "window/window-layout.inc"
#include "window/window-correction.inc"
#include "window/window-shell.inc"
#include "window/window-graphic-eq.inc"
#include "window/window-initialization.inc"
    // Always check the named control-channel object, not just the owner PID.
    // If an older object was unlinked while its owner still lived, a cached
    // pointer could silently accept edits that the current backend never saw.
    void showOffline() {
        audio_activity_.reset();
        if (viperbass::detected_backend_pid() > 0) {
            status_->setText(pending_edit_
                ? "○ Ready to process audio — changes queued until DSP activates"
                : "○ Ready to process audio — backend started; DSP not active yet");
        } else {
            status_->setText(pending_edit_
                ? "○ Offline — changes queued until ViPER reconnects"
                : "○ Offline — start viper4linux");
        }
        status_->setStyleSheet(QString("color:%1;").arg(theme_muted_color_));
    }
#include "window/window-dynamics.inc"
    void push() {
        persistCurrentSettings();
        if (audio_restart_process_) {
            pending_edit_ = true; // Send edits to the new backend, never the dying one.
            return;
        }
        publishToBackend();
    }
#include "window/window-effect-actions.inc"
    // Compare the widgets before/after a backend read. Signal blockers are
    // intentional: reflecting CLI changes must not republish a stale GUI
    // snapshot over them. We then persist the resulting live settings without
    // sending a second change to the DSP. This is GUI-thread-only code.
#include "sync/gui-settings-bridge.inc"
#include "sync/gui-backend-bridge.inc"
    QSettings settings_;
    viperbass::AudioActivityTracker audio_activity_;
    QSystemTrayIcon* tray_icon_{};
    QAction* tray_processing_action_{};
    QAction* audio_restart_action_{};
    QPushButton* audio_restart_button_{};
    QProcess* audio_restart_process_{};
    bool force_taskbar_minimize_ = false;
    bool tray_restore_maximized_ = false;
    QString theme_muted_color_ = "#536a74";
    QProcess* cinnamon_theme_query_{};
    bool cinnamon_theme_known_ = false;
    bool cinnamon_theme_dark_ = false;
    QLabel* header_icon_{};
    QLabel* audio_output_label_{};
    QString last_output_memory_revision_;
    QComboBox* theme_selector_{};
    QCheckBox* eq_enabled_{};
    QComboBox* eq_mode_{};
    QComboBox* eq_preset_{};
    QPushButton* eq_delete_button_{};
    EqOverview* eq_overview_{};
    QDialog* eq_editor_{};
    QString eq_selected_preset_ = "custom";
    int eq_active_bands_ = 10;
    QWidget* eq_fader_container_{};
    std::array<QWidget*, 31> eq_columns_{};
    std::array<QSlider*, 31> eq_faders_{};
    std::array<QLabel*, 31> eq_values_{};
    std::array<QLabel*, 31> eq_frequencies_{};
    QCheckBox* crossfeed_enabled_{};
    QComboBox* crossfeed_mode_{};
    QCheckBox* mb_enabled_{};
    QComboBox* mb_band_count_{};
    std::array<SliderRow,4> mb_crossovers_{};
    QTabWidget* mb_band_tabs_{};
    QWidget* mb_crossover_panel_{};
    std::array<QLabel*,5> mb_band_ranges_{};
    std::array<MbBandRows,5> mb_bands_{};
    QCheckBox* fet_enabled_{};
    SliderRow fet_threshold_{}, fet_ratio_{}, fet_knee_{}, fet_gain_{}, fet_attack_{}, fet_release_{};
    QCheckBox* imager_enabled_{};
    SliderRow imager_low_{}, imager_mid_{}, imager_high_{};
    SliderRow imager_low_xover_{}, imager_high_xover_{};
    QCheckBox* deq_enabled_{};
    DynamicEqOverview* deq_overview_{};
    DynamicEqOverview* deq_editor_graph_{};
    QTableWidget* deq_table_{};
    QLabel* deq_selected_label_{};
    QDialog* deq_editor_{};
    QSpinBox* deq_count_{};
    QSpinBox* deq_editor_count_{};
    QComboBox* deq_band_select_{};
    QSpinBox* deq_frequency_{};
    QComboBox* deq_type_{};
    QDoubleSpinBox* deq_q_{};
    QDoubleSpinBox* deq_gain_{};
    QDoubleSpinBox* deq_threshold_{};
    QSpinBox* deq_attack_{};
    QSpinBox* deq_release_{};
    std::array<DeqBand,10> deq_bands_{};
    std::array<bool,10> deq_user_modified_{};
    int deq_active_count_=3;
    QCheckBox* ddc_enabled_{};
    IrsComboBox* ddc_library_{};
    QFileSystemWatcher* ddc_watcher_{};
    bool ddc_library_dirty_ = false;
    QLabel* ddc_file_name_{};
    QLabel* ddc_status_{};
    QCheckBox* speaker_enabled_{};
    QCheckBox* convolver_enabled_{};
    IrsComboBox* convolver_library_{};
    QFileSystemWatcher* irs_watcher_{};
    bool irs_library_dirty_ = false;
    QPlainTextEdit* convolver_full_name_ = nullptr;
    QLabel* convolver_status_{};
    SliderRow convolver_cross_{};
    QTabWidget* tabs_{};
    QTimer* save_timer_{};
    viperbass::Mapping mapping_;
    bool pending_edit_ = false;
    bool replace_precision_on_push_ = false;
    // Native Low X can be 0–119 via CLI while the GUI displays 120.
    // Track the true value separately and publish it only on deliberate edits.
    int dyn_xlo_native_ = 100;
    bool dyn_xlo_explicit_ = false;
    QCheckBox* limiter_indicator_{};
    SliderRow limiter_threshold_{};
    SliderRow limiter_gain_{};
    SliderRow limiter_pan_{};
    QCheckBox* agc_enabled_{};
    SliderRow agc_strength_{};
    SliderRow agc_max_gain_{};
    SliderRow agc_threshold_{};
    QCheckBox* lufs_enabled_{};
    SliderRow lufs_target_{};
    SliderRow lufs_gain_{};
    QComboBox* lufs_speed_{};
    QLabel* output_warning_{};
    QCheckBox* global_{};
    QCheckBox* enabled_{};
    QComboBox* bass_mode_{};
    QCheckBox* mono_enabled_{};
    QComboBox* mono_mode_{};
    SliderRow mono_strength_{};
    SliderRow mono_frequency_{};
    QCheckBox* psycho_enabled_{};
    SliderRow psycho_cutoff_{};
    SliderRow psycho_intensity_{};
    QComboBox* psycho_order_{};
    SliderRow psycho_original_{};
    QCheckBox* dyn_enabled_{};
    SliderRow dyn_xlo_{};
    SliderRow dyn_xhi_{};
    SliderRow dyn_ylo_{};
    SliderRow dyn_yhi_{};
    SliderRow dyn_side_lo_{};
    SliderRow dyn_side_hi_{};
    SliderRow dyn_strength_{};
    QCheckBox* clarity_enabled_{};
    QComboBox* clarity_mode_{};
    SliderRow clarity_gain_{};
    QCheckBox* headphone_enabled_{};
    QComboBox* headphone_quality_{};

    QCheckBox* field_enabled_{};
    SliderRow field_widen_{};
    SliderRow field_mid_{};
    SliderRow field_depth_{};
    QCheckBox* reverb_enabled_{};
    SliderRow reverb_room_{};
    SliderRow reverb_width_{};
    SliderRow reverb_damp_{};
    SliderRow reverb_wet_{};
    SliderRow reverb_dry_{};
    QCheckBox* diff_enabled_{};
    SliderRow diff_delay_{};
    QCheckBox* diff_reverse_{};
    SliderRow diff_mix_{};
    SliderRow diff_lowpass_{};
    QCheckBox* tube_enabled_{};
    QComboBox* tube_model_{};
    SliderRow tube_drive_{};
    SliderRow tube_mix_{};
    QCheckBox* analogx_enabled_{};
    QComboBox* analogx_mode_{};
    QCheckBox* spectrum_enabled_{};
    SliderRow spectrum_frequency_{};
    SliderRow spectrum_exciter_{};
    SliderRow strength_{};
    SliderRow frequency_{};
    QLabel* status_{};
};

int main(int argc, char* argv[]) {
    // Qt may consume startup identifiers while constructing QApplication.
    const QByteArray x11StartupId = qgetenv("DESKTOP_STARTUP_ID");
    const QByteArray waylandActivationToken = qgetenv("XDG_ACTIVATION_TOKEN");
    QApplication app(argc, argv);
    app.setApplicationName("Viper4Linux");
    app.setDesktopFileName("viper4linux");
    app.setWindowIcon(QIcon(":/viper4linux/blue.png"));
    // A tray-only launch has no visible top-level window. Explicit Quit GUI
    // and closeEvent remain the only reasons for the GUI to terminate.
    app.setQuitOnLastWindowClosed(false);
    // Claim this user's GUI before creating a second settings-owning window.
    ViperGuiInstance guiInstance;
    const auto instance = guiInstance.claim(waylandActivationToken);
    if (instance == ViperGuiInstance::StartResult::Forwarded) {
        // The existing window cannot complete this new X11 startup on its own.
        viper_startup::completeX11Startup(x11StartupId);
        return 0;
    }
    if (instance == ViperGuiInstance::StartResult::Error) return 1;
    ViperWindow window;
    guiInstance.onActivation(&window, [&window](const QByteArray& token) {
        // Qt's Wayland QWindow::requestActivate consumes this environment
        // variable for xdg-activation-v1; never let it leak to subprocesses.
        if (!token.isEmpty() && QGuiApplication::platformName() == QLatin1String("wayland"))
            qputenv("XDG_ACTIVATION_TOKEN", token);
        window.activateFromSecondLaunch();
        qunsetenv("XDG_ACTIVATION_TOKEN");
    });
    window.showAtStartup();
    // A successful start-in-tray does not map a regular top-level window.
    // On X11 Qt's automatic startup-notification completion normally occurs
    // when showing a window, so explicitly complete this *hidden* cold start.
    // If the tray is unavailable, showAtStartup() falls back to a visible
    // window, leaving Qt's normal completion path untouched. Never send
    // X11 messages on a native Wayland session.
    if (!window.isVisible() &&
        QGuiApplication::platformName() == QLatin1String("xcb")) {
        QTimer::singleShot(0, &window, [x11StartupId] {
            viper_startup::completeX11Startup(x11StartupId);
        });
    }
    return app.exec();
}
