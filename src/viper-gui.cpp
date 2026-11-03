// ViPER for Linux — Qt6 control surface.
// Audio runs in PipeWire's independent filter-chain, not inside this GUI.
#include "viper-control.h"
#include "viper-backend-state.h"
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
#include <QApplication>
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
        // Keep the blue application/taskbar symbol stable across GUI themes.
        // Set the X11 WM icon from Qt, not by a .desktop file workaround.
        setWindowIcon(QIcon(":/viper4linux/blue.png"));
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
        auto* titleRow = new QHBoxLayout();
        header_icon_ = new QLabel(central);
        header_icon_->setObjectName("viperHeaderLogo");
        header_icon_->setFixedSize(38, 38);
        header_icon_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        header_icon_->setAccessibleName("Viper4Linux logo");
        header_icon_->setAlignment(Qt::AlignCenter);
        titleRow->addWidget(header_icon_);
        titleRow->addWidget(title, 1);
        auto* themeLabel = new QLabel("Theme", central);
        titleRow->addWidget(themeLabel);
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
        titleRow->addWidget(theme_selector_);
        // Single compact menu instead of separate Help and About buttons.
        auto* menuButton = new QToolButton(central);
        menuButton->setObjectName("viperApplicationMenuButton");
        menuButton->setText(QString::fromUtf8("☰"));
        menuButton->setAccessibleName("Application menu");
        menuButton->setToolTip("Preferences, library, help, and quit");
        menuButton->setFixedWidth(34);
        menuButton->setPopupMode(QToolButton::InstantPopup);
        menuButton->setMenu(makeApplicationMenu(menuButton));
        titleRow->addWidget(menuButton);
        layout->addLayout(titleRow);
        // Read-only status from the background monitor. No CLI subprocesses.
        audio_output_label_ = new QLabel("Audio Output: detecting…", central);
        audio_output_label_->setObjectName("viperCurrentAudioOutput");
        audio_output_label_->setStyleSheet("color: palette(mid);");
        audio_output_label_->setToolTip("Current physical Audio Output; enable independent settings under Preferences.");
        layout->addWidget(audio_output_label_);

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
        auto* actions = new QHBoxLayout();
        auto* presetsButton = new QPushButton("Presets…", central);
        presetsButton->setObjectName("viperNamedPresetsButton");
        presetsButton->setToolTip("Save, load, rename or delete complete effect configurations.");
        actions->addStretch();
        actions->addWidget(presetsButton);
        layout->addWidget(tabs_, 1);
        layout->addLayout(actions);
        connect(presetsButton, &QPushButton::clicked, this, [this]{ showPresetManager(); });

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
        status_ = new QLabel("Checking backend...", central);
        auto* footer = new QHBoxLayout();
        footer->setContentsMargins(0, 0, 0, 0);
        footer->addWidget(status_, 1);
        // About is available from the application menu; the footer now only
        // displays live backend connection status.
        layout->addLayout(footer);
        setCentralWidget(central);

        save_timer_ = new QTimer(this);
        save_timer_->setSingleShot(true);
        connect(save_timer_, &QTimer::timeout, this, [this]{ settings_.sync(); });

        // The original Stage 5 GUI saved these keys on every adjustment.
        // Keep the format and keys compatible so old settings are preserved.
        global_->setChecked(settings_.value("master_enabled", true).toBool());
        enabled_->setChecked(settings_.value("enabled", true).toBool());
        bass_mode_->setCurrentIndex(std::clamp(settings_.value("bass_mode", 0).toInt(), 0, 2));
        strength_.slider->setValue(settings_.value("strength_tenths", 100).toInt());
        frequency_.slider->setValue(settings_.value("frequency", 65).toInt());
        // Remove obsolete settings so no hidden input attenuation survives.
        settings_.remove("input_percent");
        settings_.remove("preset/input_percent");
        eq_enabled_->setChecked(settings_.value("eq_enabled", false).toBool());
        const int savedEqMode = validEqBands(settings_.value("eq_band_count", 10).toInt());
        eq_active_bands_ = savedEqMode;
        eq_mode_->setCurrentIndex(eq_mode_->findData(savedEqMode));
        loadEqLayout();
        eq_selected_preset_ = settings_.value("eq/selected_preset", "custom").toString();
        selectEqPreset(eq_selected_preset_);
        ddc_enabled_->setChecked(settings_.value("ddc_enabled", false).toBool());
        updateDdcFileName(settings_.value("ddc_profile").toString());
        speaker_enabled_->setChecked(settings_.value("speaker_enabled", false).toBool());
        convolver_enabled_->setChecked(settings_.value("convolver_enabled", false).toBool());
        convolver_cross_.slider->setValue(std::clamp(settings_.value("convolver_cross_percent", 0).toInt(), 0, 100));
        refreshIrsLibrary();
        mono_enabled_->setChecked(settings_.value("bass_mono_enabled", false).toBool());
        mono_mode_->setCurrentIndex(std::clamp(settings_.value("bass_mono_mode", 0).toInt(), 0, 2));
        mono_strength_.slider->setValue(std::clamp(settings_.value("bass_mono_strength_centi", 200).toInt()/10, 5, 100));
        mono_frequency_.slider->setValue(std::clamp(settings_.value("bass_mono_frequency_hz", 65).toInt(), 15, 150));
        psycho_enabled_->setChecked(settings_.value("psycho_enabled", false).toBool());
        psycho_cutoff_.slider->setValue(std::clamp(settings_.value("psycho_cutoff_hz", 80).toInt(), 60, 150));
        psycho_intensity_.slider->setValue(std::clamp(settings_.value("psycho_intensity_percent", 30).toInt(), 0, 100));
        psycho_order_->setCurrentIndex(std::clamp(settings_.value("psycho_order", 3).toInt()-2, 0, 3));
        psycho_original_.slider->setValue(std::clamp(settings_.value("psycho_original_percent", 100).toInt(), 0, 100));
        dyn_enabled_->setChecked(settings_.value("dynamic_enabled", false).toBool());
        dyn_xlo_.slider->setValue(std::clamp(settings_.value("dynamic_x_low", 100).toInt(), 0, 2400));
        dyn_xhi_.slider->setValue(std::clamp(settings_.value("dynamic_x_high", 6000).toInt(), 0, 12000));
        dyn_ylo_.slider->setValue(std::clamp(settings_.value("dynamic_y_low", 100).toInt(), 0, 200));
        dyn_yhi_.slider->setValue(std::clamp(settings_.value("dynamic_y_high", 150).toInt(), 0, 300));
        dyn_side_lo_.slider->setValue(std::clamp(settings_.value("dynamic_side_low_percent", 50).toInt(), 0, 100));
        dyn_side_hi_.slider->setValue(std::clamp(settings_.value("dynamic_side_high_percent", 50).toInt(), 0, 100));
        dyn_strength_.slider->setValue(std::clamp(settings_.value("dynamic_strength_percent", 0).toInt(), 0, 100));
        clarity_enabled_->setChecked(settings_.value("clarity_enabled", false).toBool());
        clarity_mode_->setCurrentIndex(std::clamp(settings_.value("clarity_mode", 0).toInt(), 0, 2));
        clarity_gain_.slider->setValue(settings_.value("clarity_gain_tenths", 15).toInt());
        spectrum_enabled_->setChecked(settings_.value("spectrum_enabled", false).toBool());
        spectrum_frequency_.slider->setValue(settings_.value("spectrum_frequency_hundreds", 76).toInt());
        spectrum_exciter_.slider->setValue(settings_.value("spectrum_exciter_tenths", 10).toInt());
        tube_enabled_->setChecked(settings_.value("tube_enabled", false).toBool());
        tube_model_->setCurrentIndex(std::clamp(settings_.value("tube_model", 0).toInt(), 0, 2));
        tube_drive_.slider->setValue(std::clamp(settings_.value("tube_drive_percent", 50).toInt(), 0, 100));
        tube_mix_.slider->setValue(std::clamp(settings_.value("tube_mix_percent", 30).toInt(), 0, 100));
        analogx_enabled_->setChecked(settings_.value("analogx_enabled", false).toBool());
        analogx_mode_->setCurrentIndex(std::clamp(settings_.value("analogx_mode", 0).toInt(), 0, 2));
        crossfeed_enabled_->setChecked(settings_.value("crossfeed_enabled", false).toBool());
        crossfeed_mode_->setCurrentIndex(std::clamp(settings_.value("crossfeed_preset", 0).toInt(), 0, 2));
        loadMultibandSettings("");
        fet_enabled_->setChecked(settings_.value("fet_enabled", false).toBool());
        fet_threshold_.slider->setValue(std::clamp(settings_.value("fet_threshold_db", -18).toInt(), -48, 0));
        fet_ratio_.slider->setValue(std::clamp(settings_.value("fet_ratio_tenths", 20).toInt(), 10, 100));
        fet_knee_.slider->setValue(std::clamp(settings_.value("fet_knee_db", 6).toInt(), 0, 12));
        fet_gain_.slider->setValue(std::clamp(settings_.value("fet_makeup_db", 0).toInt(), 0, 24));
        fet_attack_.slider->setValue(std::clamp(settings_.value("fet_attack_ms", 10).toInt(), 1, 100));
        fet_release_.slider->setValue(std::clamp(settings_.value("fet_release_ms", 100).toInt(), 5, 500));
        imager_enabled_->setChecked(settings_.value("imager_enabled", false).toBool());
        imager_low_.slider->setValue(std::clamp(settings_.value("imager_low_width_percent", 100).toInt(), 0, 200));
        imager_mid_.slider->setValue(std::clamp(settings_.value("imager_mid_width_percent", 100).toInt(), 0, 200));
        imager_high_.slider->setValue(std::clamp(settings_.value("imager_high_width_percent", 100).toInt(), 0, 200));
        imager_low_xover_.slider->setValue(std::clamp(settings_.value("imager_low_crossover_hz", 200).toInt(), 80, 400));
        imager_high_xover_.slider->setValue(std::clamp(settings_.value("imager_high_crossover_hz", 4000).toInt()/100, 20, 80));
        loadDeqSettings();
        headphone_enabled_->setChecked(settings_.value("headphone_enabled", false).toBool());
        headphone_quality_->setCurrentIndex(std::clamp(settings_.value("headphone_quality", 0).toInt(), 0, 4));
        diff_enabled_->setChecked(settings_.value("diff_enabled", false).toBool());
        diff_delay_.slider->setValue(std::clamp(settings_.value("diff_delay_ms", 8).toInt(), 1, 20));
        diff_reverse_->setChecked(settings_.value("diff_reverse", false).toBool());
        diff_mix_.slider->setValue(std::clamp(settings_.value("diff_mix_percent", 50).toInt(), 0, 100));
        diff_lowpass_.slider->setValue(std::clamp(settings_.value("diff_lp_hundreds", 0).toInt(), 0, 200));
        field_enabled_->setChecked(settings_.value("field_enabled", false).toBool());
        field_widen_.slider->setValue(std::clamp(settings_.value("field_widen_tenths", 10).toInt(), 0, 80));
        field_mid_.slider->setValue(std::clamp(settings_.value("field_mid_hundredths", 100).toInt(), 100, 200));
        field_depth_.slider->setValue(std::clamp(settings_.value("field_depth_step", 3).toInt(), 0, 16));
        reverb_enabled_->setChecked(settings_.value("reverb_enabled", false).toBool());
        reverb_room_.slider->setValue(std::clamp(settings_.value("reverb_room_percent", 50).toInt(), 0, 100));
        reverb_width_.slider->setValue(std::clamp(settings_.value("reverb_width_percent", 50).toInt(), 0, 100));
        reverb_damp_.slider->setValue(std::clamp(settings_.value("reverb_damp_percent", 50).toInt(), 0, 100));
        reverb_wet_.slider->setValue(std::clamp(settings_.value("reverb_wet_percent", 20).toInt(), 0, 100));
        reverb_dry_.slider->setValue(std::clamp(settings_.value("reverb_dry_percent", 50).toInt(), 0, 100));
        limiter_threshold_.slider->setValue(std::clamp(settings_.value("limiter_threshold_permille", 500).toInt(), 300, 1000));
        limiter_gain_.slider->setValue(std::clamp(settings_.value("limiter_output_percent", 100).toInt(), 1, 200));
        limiter_pan_.slider->setValue(std::clamp(settings_.value("limiter_pan_percent", 0).toInt(), -100, 100));
        agc_enabled_->setChecked(settings_.value("agc_enabled", false).toBool());
        agc_strength_.slider->setValue(std::clamp(settings_.value("agc_strength_centi", 100).toInt(), 50, 300));
        agc_max_gain_.slider->setValue(std::clamp(settings_.value("agc_max_gain_centi", 300).toInt(), 100, 1000));
        agc_threshold_.slider->setValue(std::clamp(settings_.value("agc_threshold_permille", 900).toInt(), 300, 1000));
        lufs_enabled_->setChecked(settings_.value("lufs_enabled", false).toBool());
        lufs_target_.slider->setValue(std::clamp(settings_.value("lufs_target_tenths", -140).toInt(), -240, -80));
        lufs_gain_.slider->setValue(std::clamp(settings_.value("lufs_max_gain_tenths", 60).toInt(), 0, 120));
        lufs_speed_->setCurrentIndex(std::clamp(settings_.value("lufs_speed", 1).toInt(), 0, 2));
        updateLabels();

        connect(global_, &QCheckBox::toggled, this, [this]{ push(); });
        connect(enabled_, &QCheckBox::toggled, this, [this]{ push(); });
        connect(bass_mode_, &QComboBox::currentIndexChanged, this, [this]{ push(); });
        connect(strength_.slider, &QSlider::valueChanged, this, [this]{ updateLabels(); push(); });
        connect(frequency_.slider, &QSlider::valueChanged, this, [this]{ updateLabels(); push(); });
        connect(eq_enabled_, &QCheckBox::toggled, this, [this]{ push(); });
        connect(eq_mode_, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int index) {
            if (index < 0) return;
            saveEqLayout();
            eq_active_bands_ = validEqBands(eq_mode_->itemData(index).toInt());
            if (eq_selected_preset_ != "custom") applyEqPreset(eq_selected_preset_);
            else { loadEqLayout(); push(); }
            // A visible nonmodal editor must change its columns immediately.
            // applyEqPreset() updates levels, not column visibility.
            syncGraphicEqEditorColumns();
        });
        for (int i = 0; i < 31; ++i) {
            connect(eq_faders_[i], &QSlider::valueChanged, this, [this, i](int value) {
                eq_values_[i]->setText(QString::number(value / 100.0, 'f', 2));
                eq_faders_[i]->setToolTip(eq_frequencies_[i]->text() + ": " +
                    QString::number(value / 100.0, 'f', 2) + " dB");
                saveEqLayout();
                selectEqPreset("custom");
                updateEqOverview();
                push();
            });
        }
        connect(eq_reset, &QPushButton::clicked, this, [this] {
            applyEqPreset("builtin:Flat");
            selectEqPreset("builtin:Flat");
        });
        connect(ddc_enabled_, &QCheckBox::toggled, this, [this]{ push(); });
        connect(speaker_enabled_, &QCheckBox::toggled, this, [this]{ push(); });
        connect(convolver_enabled_, &QCheckBox::toggled, this, [this]{ push(); });
        connect(convolver_cross_.slider, &QSlider::valueChanged, this, [this]{ updateLabels(); push(); });
        connect(mono_enabled_, &QCheckBox::toggled, this, [this]{ push(); });
        connect(mono_mode_, &QComboBox::currentIndexChanged, this, [this]{ push(); });
        connect(psycho_enabled_, &QCheckBox::toggled, this, [this]{ push(); });
        connect(psycho_order_, &QComboBox::currentIndexChanged, this, [this]{ push(); });
        connect(dyn_enabled_, &QCheckBox::toggled, this, [this]{ push(); });
        for (auto* slider : {mono_strength_.slider, mono_frequency_.slider,
                             psycho_cutoff_.slider, psycho_intensity_.slider, psycho_original_.slider,
                             dyn_xlo_.slider, dyn_xhi_.slider, dyn_ylo_.slider, dyn_yhi_.slider,
                             dyn_side_lo_.slider, dyn_side_hi_.slider, dyn_strength_.slider}) {
            connect(slider, &QSlider::valueChanged, this, [this]{ updateLabels(); push(); });
        }
        connect(clarity_enabled_, &QCheckBox::toggled, this, [this]{ push(); });
        connect(clarity_mode_, &QComboBox::currentIndexChanged, this, [this]{ push(); });
        connect(clarity_gain_.slider, &QSlider::valueChanged, this, [this]{ updateLabels(); push(); });
        connect(spectrum_enabled_, &QCheckBox::toggled, this, [this]{ push(); });
        connect(spectrum_frequency_.slider, &QSlider::valueChanged, this, [this]{ updateLabels(); push(); });
        connect(spectrum_exciter_.slider, &QSlider::valueChanged, this, [this]{ updateLabels(); push(); });
        connect(tube_enabled_, &QCheckBox::toggled, this, [this]{ push(); });
        connect(tube_model_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]{ push(); });
        connect(tube_drive_.slider, &QSlider::valueChanged, this, [this]{ updateLabels(); push(); });
        connect(tube_mix_.slider, &QSlider::valueChanged, this, [this]{ updateLabels(); push(); });
        connect(analogx_enabled_, &QCheckBox::toggled, this, [this]{ push(); });
        connect(analogx_mode_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]{ push(); });
        connect(deq_enabled_, &QCheckBox::toggled, this, [this]{
            updateDeqOverview(); push();
        });
        connect(deq_spread, &QPushButton::clicked, this,
                [this]{ spreadDeqFrequencies(); });
        connect(deq_count_, QOverload<int>::of(&QSpinBox::valueChanged),
                this, [this](int n){ changeDeqBandCount(n); });
        connect(deq_editor_count_, QOverload<int>::of(&QSpinBox::valueChanged),
                this, [this](int n){ changeDeqBandCount(n); });
        connect(deq_band_select_, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int index){ if (index >= 0) loadDeqEditorBand(index); });
        for (QSpinBox* s : {deq_frequency_, deq_attack_, deq_release_})
            connect(s, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int){ commitDeqEditorBand(); });
        for (QDoubleSpinBox* s : {deq_q_, deq_gain_, deq_threshold_})
            connect(s, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double){ commitDeqEditorBand(); });
        connect(deq_type_, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int){ commitDeqEditorBand(); });
        connect(crossfeed_enabled_, &QCheckBox::toggled, this, [this]{ push(); });
        connect(crossfeed_mode_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]{ push(); });
        connect(mb_enabled_, &QCheckBox::toggled, this, [this]{ push(); });
        connect(mb_band_count_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index){
            if (index < 0) return;
            const std::array<int,4> defaults = index == 0
                ? std::array<int,4>{200, 4000, 5000, 8000}
                : std::array<int,4>{120, 500, 4000, 8000};
            for (int i=0; i<4; ++i) {
                const QSignalBlocker block(mb_crossovers_[i].slider);
                mb_crossovers_[i].slider->setValue(defaults[i]);
            }
            updateMultibandVisibility(); updateLabels(); push();
        });
        for (int i=0; i<4; ++i)
            connect(mb_crossovers_[i].slider, &QSlider::valueChanged, this, [this]{
                updateMultibandFrequencyLabels(); updateLabels(); push();
            });
        for (auto& band : mb_bands_) {
            connect(band.enabled, &QCheckBox::toggled, this, [this]{ push(); });
            for (const auto& r : {band.threshold, band.ratio, band.knee, band.gain, band.attack, band.release})
                connect(r.slider, &QSlider::valueChanged, this, [this]{ updateLabels(); push(); });
        }
        connect(fet_enabled_, &QCheckBox::toggled, this, [this]{ push(); });
        for (const auto& row : {fet_threshold_, fet_ratio_, fet_knee_, fet_gain_, fet_attack_, fet_release_})
            connect(row.slider, &QSlider::valueChanged, this, [this]{ updateLabels(); push(); });
        connect(imager_enabled_, &QCheckBox::toggled, this, [this]{ push(); });
        for (const auto& r : {imager_low_, imager_mid_, imager_high_, imager_low_xover_, imager_high_xover_})
            connect(r.slider, &QSlider::valueChanged, this, [this]{ updateLabels(); push(); });
        connect(headphone_enabled_, &QCheckBox::toggled, this, [this]{ push(); });
        connect(headphone_quality_, &QComboBox::currentIndexChanged, this, [this]{ push(); });
        connect(diff_enabled_, &QCheckBox::toggled, this, [this]{ push(); });
        connect(diff_delay_.slider, &QSlider::valueChanged, this, [this]{ updateLabels(); push(); });
        connect(diff_reverse_, &QCheckBox::toggled, this, [this]{ push(); });
        connect(diff_mix_.slider, &QSlider::valueChanged, this, [this]{ updateLabels(); push(); });
        connect(diff_lowpass_.slider, &QSlider::valueChanged, this, [this]{ updateLabels(); push(); });
        connect(field_enabled_, &QCheckBox::toggled, this, [this]{ push(); });
        connect(field_widen_.slider, &QSlider::valueChanged, this, [this]{ updateLabels(); push(); });
        connect(field_mid_.slider, &QSlider::valueChanged, this, [this]{ updateLabels(); push(); });
        connect(field_depth_.slider, &QSlider::valueChanged, this, [this]{ updateLabels(); push(); });
        connect(reverb_enabled_, &QCheckBox::toggled, this, [this]{ push(); });
        connect(reverb_room_.slider, &QSlider::valueChanged, this, [this]{ updateLabels(); push(); });
        connect(reverb_width_.slider, &QSlider::valueChanged, this, [this]{ updateLabels(); push(); });
        connect(reverb_damp_.slider, &QSlider::valueChanged, this, [this]{ updateLabels(); push(); });
        connect(reverb_wet_.slider, &QSlider::valueChanged, this, [this]{ updateLabels(); push(); });
        connect(reverb_dry_.slider, &QSlider::valueChanged, this, [this]{ updateLabels(); push(); });
        for (auto* slider : {limiter_threshold_.slider, limiter_gain_.slider, limiter_pan_.slider,
                              agc_strength_.slider, agc_max_gain_.slider, agc_threshold_.slider,
                              lufs_target_.slider, lufs_gain_.slider}) {
            connect(slider, &QSlider::valueChanged, this, [this]{ updateLabels(); push(); });
        }
        connect(agc_enabled_, &QCheckBox::toggled, this, [this]{ updateLabels(); push(); });
        connect(lufs_enabled_, &QCheckBox::toggled, this, [this]{ updateLabels(); push(); });
        connect(lufs_speed_, &QComboBox::currentIndexChanged, this, [this]{ push(); });
        auto* timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, [this]{
            updateAudioOutputDisplay();
            pollBackend();
        });
        timer->start(500);
        // Theme selection changes only the GUI, never DSP settings.
        connect(theme_selector_, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int index) {
            settings_.setValue("ui/theme", index == 1 ? "classic-v4a" : "modern");
            applyTheme(index == 1);
        });
        // On Cinnamon/X11 Qt can report a dark colorScheme even while GTK
        // desktop windows use a light theme. The authoritative choice there is
        // Cinnamon's GTK theme setting, read asynchronously (no UI blocking).
        // Until discovered, the required fallback is LIGHT.
        if (runningCinnamon()) {
            cinnamon_theme_query_ = new QProcess(this);
            connect(cinnamon_theme_query_, &QProcess::finished,
                    this, [this](int code, QProcess::ExitStatus exitStatus) {
                if (exitStatus != QProcess::NormalExit || code != 0) return;
                const QString value = QString::fromUtf8(cinnamon_theme_query_->readAllStandardOutput()).trimmed();
                if (value.isEmpty() || !value.contains('\'')) return;
                const bool isDark = value.contains("dark", Qt::CaseInsensitive);
                if (!cinnamon_theme_known_ || cinnamon_theme_dark_ != isDark) {
                    cinnamon_theme_known_ = true;
                    cinnamon_theme_dark_ = isDark;
                    applyTheme(theme_selector_->currentIndex() == 1);
                }
            });
            auto* desktopThemeTimer = new QTimer(this);
            connect(desktopThemeTimer, &QTimer::timeout, this, [this] {
                if (cinnamon_theme_query_->state() == QProcess::NotRunning) {
                    cinnamon_theme_query_->start("gsettings",
                        {"get", "org.cinnamon.desktop.interface", "gtk-theme"});
                }
            });
            desktopThemeTimer->start(5000);
            // First query happens right away, without waiting five seconds.
            cinnamon_theme_query_->start("gsettings",
                {"get", "org.cinnamon.desktop.interface", "gtk-theme"});
        }
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
        connect(qApp->styleHints(), &QStyleHints::colorSchemeChanged,
                this, [this](Qt::ColorScheme) {
            QTimer::singleShot(0, this, [this] {
                applyTheme(theme_selector_->currentIndex() == 1);
            });
        });
#endif
        applyTheme(theme_selector_->currentIndex() == 1);
        restoreWindowSizes();
        setupTray();
        pollBackend();
    }
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
        saveUiGeometry();
        const QString choice = settings_.value("ui/closeBehavior", "quit").toString();
        if (choice == "tray") {
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
            event->ignore();
            force_taskbar_minimize_ = true;
            showMinimized();
            QTimer::singleShot(0, this, [this]{ force_taskbar_minimize_ = false; });
            return;
        }
        event->accept();
        // quitOnLastWindowClosed is false to support tray-only startup;
        // a genuine close/quit must therefore explicitly terminate the GUI.
        QTimer::singleShot(0, qApp, [] { qApp->quit(); });
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
    // Stable per-user location for portable, named effect snapshots. We keep
    // QSettings's existing org/app config store intact for preferences and
    // automatic session state, including older one-slot presets.
    QString presetLibraryDirectory() const {
        return viper_presets::presetLibraryDirectory();
    }
    using NamedPreset = viper_presets::NamedPreset;
    QList<NamedPreset> namedPresets() const {
        return viper_presets::namedPresets();
    }
    static bool acceptablePresetName(const QString& name) {
        return viper_presets::acceptablePresetName(name);
    }
    bool writeNamedPreset(const QString& name, const QString& path, QString* error) {
        return viper_presets::writeNamedPreset(settings_, name, path, error);
    }
    bool migrateLegacyPreset(QString* error) {
        return viper_presets::migrateLegacyPreset(settings_, error);
    }
    bool stageNamedPreset(const QString& path, QString* error) {
        return viper_presets::stageNamedPreset(settings_, path, error);
    }
    void snapshotToPresetStaging() {
            settings_.setValue("preset/valid", true);
            settings_.setValue("preset/master_enabled", global_->isChecked());
            settings_.setValue("preset/enabled", enabled_->isChecked());
            settings_.setValue("preset/bass_mode", bass_mode_->currentIndex());
            settings_.setValue("preset/strength_tenths", strength_.slider->value());
            settings_.setValue("preset/frequency", frequency_.slider->value());
            saveEqLayout();
            settings_.setValue("preset/eq_enabled", eq_enabled_->isChecked());
            settings_.setValue("preset/eq_selected_preset", eq_selected_preset_);
            settings_.setValue("preset/eq_band_count", eq_active_bands_);
            for (int n : {10, 15, 25, 31})
                settings_.setValue("preset/eq_levels_" + QString::number(n),
                    settings_.value("eq_levels_" + QString::number(n), flatEqCsv(n)));
            settings_.setValue("preset/ddc_enabled", ddc_enabled_->isChecked());
            settings_.setValue("preset/ddc_profile", settings_.value("ddc_profile", ""));
            settings_.setValue("preset/speaker_enabled", speaker_enabled_->isChecked());
            settings_.setValue("preset/convolver_enabled", convolver_enabled_->isChecked());
            settings_.setValue("preset/convolver_cross_percent", convolver_cross_.slider->value());
            settings_.setValue("preset/convolver_kernel", settings_.value("convolver_kernel"));
            saveBassPreset();
            settings_.setValue("preset/clarity_enabled", clarity_enabled_->isChecked());
            settings_.setValue("preset/clarity_mode", clarity_mode_->currentIndex());
            settings_.setValue("preset/clarity_gain_tenths", clarity_gain_.slider->value());
            settings_.setValue("preset/tube_enabled", tube_enabled_->isChecked());
            settings_.setValue("preset/tube_model", tube_model_->currentIndex());
            settings_.setValue("preset/tube_drive_percent", tube_drive_.slider->value());
            settings_.setValue("preset/tube_mix_percent", tube_mix_.slider->value());
            settings_.setValue("preset/analogx_enabled", analogx_enabled_->isChecked());
            settings_.setValue("preset/analogx_mode", analogx_mode_->currentIndex());
            settings_.setValue("preset/crossfeed_enabled", crossfeed_enabled_->isChecked());
            settings_.setValue("preset/crossfeed_preset", crossfeed_mode_->currentIndex());
            saveMultibandSettings("preset/");
            settings_.setValue("preset/fet_enabled", fet_enabled_->isChecked());
            settings_.setValue("preset/fet_threshold_db", fet_threshold_.slider->value());
            settings_.setValue("preset/fet_ratio_tenths", fet_ratio_.slider->value());
            settings_.setValue("preset/fet_knee_db", fet_knee_.slider->value());
            settings_.setValue("preset/fet_makeup_db", fet_gain_.slider->value());
            settings_.setValue("preset/fet_attack_ms", fet_attack_.slider->value());
            settings_.setValue("preset/fet_release_ms", fet_release_.slider->value());
            settings_.setValue("preset/imager_enabled", imager_enabled_->isChecked());
            settings_.setValue("preset/imager_low_width_percent", imager_low_.slider->value());
            settings_.setValue("preset/imager_mid_width_percent", imager_mid_.slider->value());
            settings_.setValue("preset/imager_high_width_percent", imager_high_.slider->value());
            settings_.setValue("preset/imager_low_crossover_hz", imager_low_xover_.slider->value());
            settings_.setValue("preset/imager_high_crossover_hundreds", imager_high_xover_.slider->value());
            saveDeqPreset();
            settings_.setValue("preset/spectrum_enabled", spectrum_enabled_->isChecked());
            settings_.setValue("preset/spectrum_frequency_hundreds", spectrum_frequency_.slider->value());
            settings_.setValue("preset/spectrum_exciter_tenths", spectrum_exciter_.slider->value());
            settings_.setValue("preset/headphone_enabled", headphone_enabled_->isChecked());
            settings_.setValue("preset/headphone_quality", headphone_quality_->currentIndex());
            settings_.setValue("preset/diff_enabled", diff_enabled_->isChecked());
            settings_.setValue("preset/diff_delay_ms", diff_delay_.slider->value());
            settings_.setValue("preset/diff_reverse", diff_reverse_->isChecked());
            settings_.setValue("preset/diff_mix_percent", diff_mix_.slider->value());
            settings_.setValue("preset/diff_lp_hundreds", diff_lowpass_.slider->value());
            settings_.setValue("preset/field_enabled", field_enabled_->isChecked());
            settings_.setValue("preset/field_widen_tenths", field_widen_.slider->value());
            settings_.setValue("preset/field_mid_hundredths", field_mid_.slider->value());
            settings_.setValue("preset/field_depth_step", field_depth_.slider->value());
            settings_.setValue("preset/reverb_enabled", reverb_enabled_->isChecked());
            settings_.setValue("preset/reverb_room_percent", reverb_room_.slider->value());
            settings_.setValue("preset/reverb_width_percent", reverb_width_.slider->value());
            settings_.setValue("preset/reverb_damp_percent", reverb_damp_.slider->value());
            settings_.setValue("preset/reverb_wet_percent", reverb_wet_.slider->value());
            settings_.setValue("preset/reverb_dry_percent", reverb_dry_.slider->value());
            saveOutputPreset();
            settings_.sync();
    }
    void restoreFromPresetStaging() {
            if (!settings_.value("preset/valid", false).toBool()) return;
            applySettings(settings_.value("preset/master_enabled", true).toBool(),
                          settings_.value("preset/enabled", true).toBool(),
                          settings_.value("preset/bass_mode", 0).toInt(),
                          settings_.value("preset/strength_tenths", 100).toInt(),
                          settings_.value("preset/frequency", 65).toInt(),
                          settings_.value("preset/clarity_enabled", false).toBool(),
                          settings_.value("preset/clarity_mode", 0).toInt(),
                          settings_.value("preset/clarity_gain_tenths", 15).toInt(),
                          settings_.value("preset/spectrum_enabled", false).toBool(),
                          settings_.value("preset/spectrum_frequency_hundreds", 76).toInt(),
                          settings_.value("preset/spectrum_exciter_tenths", 10).toInt(),
                          settings_.value("preset/headphone_enabled", false).toBool(),
                          settings_.value("preset/headphone_quality", 0).toInt(),
                          settings_.value("preset/diff_enabled", false).toBool(),
                          settings_.value("preset/diff_delay_ms", 8).toInt(),
                          settings_.value("preset/diff_reverse", false).toBool(),
                          settings_.value("preset/diff_mix_percent", 50).toInt(),
                          settings_.value("preset/diff_lp_hundreds", 0).toInt(),
                          settings_.value("preset/field_enabled", false).toBool(),
                          settings_.value("preset/field_widen_tenths", 10).toInt(),
                          settings_.value("preset/field_mid_hundredths", 100).toInt(),
                          settings_.value("preset/field_depth_step", 3).toInt(),
                          settings_.value("preset/reverb_enabled", false).toBool(),
                          settings_.value("preset/reverb_room_percent", 50).toInt(),
                          settings_.value("preset/reverb_width_percent", 50).toInt(),
                          settings_.value("preset/reverb_damp_percent", 50).toInt(),
                          settings_.value("preset/reverb_wet_percent", 20).toInt(),
                          settings_.value("preset/reverb_dry_percent", 50).toInt());
            for (int n : {10, 15, 25, 31})
                settings_.setValue("eq_levels_" + QString::number(n),
                    settings_.value("preset/eq_levels_" + QString::number(n),
                        flatEqCsv(n)));
            {
                const QSignalBlocker blockEq(eq_enabled_), blockMode(eq_mode_);
                eq_enabled_->setChecked(settings_.value("preset/eq_enabled", false).toBool());
                eq_active_bands_ = validEqBands(settings_.value("preset/eq_band_count", 10).toInt());
                eq_mode_->setCurrentIndex(eq_mode_->findData(eq_active_bands_));
                loadEqLayout();
                selectEqPreset(settings_.value("preset/eq_selected_preset", "custom").toString());
            }
            restoreOutputPreset();
            restoreBassPreset();
            applyTubeSettings(settings_.value("preset/tube_enabled", false).toBool(),
                settings_.value("preset/tube_model", 0).toInt(),
                settings_.value("preset/tube_drive_percent", 50).toInt(),
                settings_.value("preset/tube_mix_percent", 30).toInt());
            applyAnalogXSettings(settings_.value("preset/analogx_enabled", false).toBool(),
                settings_.value("preset/analogx_mode", 0).toInt());
            {
                const QSignalBlocker a(crossfeed_enabled_), b(crossfeed_mode_);
                crossfeed_enabled_->setChecked(settings_.value("preset/crossfeed_enabled", false).toBool());
                crossfeed_mode_->setCurrentIndex(std::clamp(settings_.value("preset/crossfeed_preset", 0).toInt(), 0, 2));
            }
            {
                const QSignalBlocker a(imager_enabled_);
                imager_enabled_->setChecked(settings_.value("preset/imager_enabled", false).toBool());
                for (const auto& item : std::initializer_list<std::pair<SliderRow, int>>{
                    {imager_low_, settings_.value("preset/imager_low_width_percent", 100).toInt()},
                    {imager_mid_, settings_.value("preset/imager_mid_width_percent", 100).toInt()},
                    {imager_high_, settings_.value("preset/imager_high_width_percent", 100).toInt()},
                    {imager_low_xover_, settings_.value("preset/imager_low_crossover_hz", 200).toInt()},
                    {imager_high_xover_, settings_.value("preset/imager_high_crossover_hundreds", 40).toInt()}}) {
                    const QSignalBlocker block(item.first.slider);
                    item.first.slider->setValue(item.second);
                }
                updateLabels();
            }
            {
                const QSignalBlocker b(ddc_enabled_);
                ddc_enabled_->setChecked(settings_.value("preset/ddc_enabled", false).toBool());
                settings_.setValue("ddc_profile", settings_.value("preset/ddc_profile", ""));
                updateDdcFileName(settings_.value("ddc_profile", "").toString());
                settings_.sync();
                requestDdcReload();
            }
            {
                const QSignalBlocker b(speaker_enabled_);
                speaker_enabled_->setChecked(settings_.value("preset/speaker_enabled", false).toBool());
            }
            restoreFetSettings("preset/");
            loadMultibandSettings("preset/");
            restoreDeqPreset();
            {
                const QSignalBlocker b1(convolver_enabled_), b2(convolver_cross_.slider);
                convolver_enabled_->setChecked(settings_.value("preset/convolver_enabled", false).toBool());
                convolver_cross_.slider->setValue(settings_.value("preset/convolver_cross_percent", 0).toInt());
                settings_.setValue("convolver_kernel", settings_.value("preset/convolver_kernel", ""));
                settings_.sync();
                refreshIrsLibrary();
                requestIrsLoad();
            }
            push();
    }
    void resetEffectsWithConfirmation() {
            QMessageBox confirm(this);
            confirm.setIcon(QMessageBox::Warning);
            confirm.setWindowTitle("Reset Effect Settings");
            confirm.setText("Reset all effect settings?");
            confirm.setInformativeText(
                "This will replace your current effect settings with the application's defaults "
                "(master processing and limiter at defaults; all optional effects off).\n\n"
                "Your named presets will not be deleted.");
            auto* resetButton = confirm.addButton("Reset Effects", QMessageBox::DestructiveRole);
            auto* cancelButton = confirm.addButton(QMessageBox::Cancel);
            confirm.setDefaultButton(qobject_cast<QPushButton*>(cancelButton));
            confirm.exec();
            if (confirm.clickedButton() != resetButton) return;

            applySettings(true, false, 0, 100, 65, false, 0, 15, false, 76, 10,
                          false, 0, false, 8, false, 50, 0,
                          false, 10, 100, 3,
                          false, 50, 50, 50, 20, 50);
            resetOutputControls();
            resetNewBassControls();
            applyTubeSettings(false, 0, 50, 30);
            applyAnalogXSettings(false, 0);
            {
                const QSignalBlocker a(crossfeed_enabled_), b(crossfeed_mode_);
                crossfeed_enabled_->setChecked(false);
                crossfeed_mode_->setCurrentIndex(0);
            }
            {
                const QSignalBlocker a(imager_enabled_);
                imager_enabled_->setChecked(false);
                for (const auto& item : std::initializer_list<std::pair<SliderRow, int>>{
                    {imager_low_, 100}, {imager_mid_, 100}, {imager_high_, 100},
                    {imager_low_xover_, 200}, {imager_high_xover_, 40}}) {
                    const QSignalBlocker block(item.first.slider);
                    item.first.slider->setValue(item.second);
                }
                updateLabels();
            }
            {
                const QSignalBlocker b(ddc_enabled_);
                ddc_enabled_->setChecked(false);
            }
            {
                const QSignalBlocker b(speaker_enabled_);
                speaker_enabled_->setChecked(false);
            }
            resetFetSettings();
            resetMultibandSettings();
            resetDeq();
            {
                const QSignalBlocker blockEq(eq_enabled_), blockMode(eq_mode_);
                eq_enabled_->setChecked(false);
                for (int n : {10, 15, 25, 31})
                    settings_.setValue("eq_levels_" + QString::number(n),
                        flatEqCsv(n));
                eq_active_bands_ = 10;
                eq_mode_->setCurrentIndex(eq_mode_->findData(10));
                loadEqLayout();
            }

            {
                const QSignalBlocker b1(convolver_enabled_), b2(convolver_cross_.slider);
                convolver_enabled_->setChecked(false);
                convolver_cross_.slider->setValue(0);
            }
            push();
            statusMessage("Effect settings reset to defaults; named presets preserved.");
    }
    void showPresetManager() {
        viper_presets_ui::showPresetManager(this, {
            [this](QString* error) -> bool { return migrateLegacyPreset(error); },
            [this]() -> QList<NamedPreset> { return namedPresets(); },
            [this](const QString& name) -> bool { return acceptablePresetName(name); },
            [this]() -> QString { return presetLibraryDirectory(); },
            [this]() { snapshotToPresetStaging(); },
            [this](const QString& name, const QString& path, QString* error) -> bool { return writeNamedPreset(name, path, error); },
            [this](const QString& message) { statusMessage(message); },
            [this]() -> QString { return outputMemoryDirectory(); },
            [this]() { updateAudioOutputDisplay(); },
            [this](const QString& path, QString* error) -> bool { return stageNamedPreset(path, error); },
            [this]() { restoreFromPresetStaging(); },
            [this]() { resetEffectsWithConfirmation(); }
        });
    }
    QSize savedWindowSize(const QString& key, const QSize& fallback, const QSize& minimum) const {
        QSize requested = settings_.value(key, fallback).toSize();
        if (!requested.isValid() || requested.isEmpty()) requested = fallback;
        QScreen* target = QGuiApplication::primaryScreen();
        if (!target) return requested.expandedTo(minimum);
        const QSize available = target->availableGeometry().size();
        return QSize(std::clamp(requested.width(), minimum.width(),
                              std::max(minimum.width(), available.width() - 24)),
                     std::clamp(requested.height(), minimum.height(),
                              std::max(minimum.height(), available.height() - 48)));
    }
    void restoreWindowSizes() {
        resize(savedWindowSize("ui/mainWindowSize", QSize(740, 800), minimumSize()));
        eq_editor_->resize(savedWindowSize("ui/graphicEqEditorSize", QSize(720, 355),
                                          eq_editor_->minimumSize()));
        deq_editor_->resize(savedWindowSize("ui/dynamicEqEditorSize", QSize(670, 735),
                                           deq_editor_->minimumSize()));
        eq_editor_->installEventFilter(this);
        deq_editor_->installEventFilter(this);
        if (settings_.value("ui/mainWindowMaximized", false).toBool())
            QTimer::singleShot(0, this, [this]{ showMaximized(); });
    }
    static bool runningCinnamon() {
        return qEnvironmentVariable("XDG_CURRENT_DESKTOP").contains("cinnamon", Qt::CaseInsensitive)
            || qEnvironmentVariable("XDG_SESSION_DESKTOP").contains("cinnamon", Qt::CaseInsensitive);
    }
    bool systemPrefersDark() const {
        if (runningCinnamon()) return cinnamon_theme_known_ && cinnamon_theme_dark_;
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
        return qApp->styleHints()->colorScheme() == Qt::ColorScheme::Dark;
#else
        return false; // On Qt < 6.5 Unknown falls back to LIGHT.
#endif
    }
    void applyTheme(bool classic) {
        theme_muted_color_ = viper_theme::apply(
            this, classic, systemPrefersDark(), deq_editor_, eq_editor_,
            tabs_, header_icon_, status_);
    }
    QVBoxLayout* addEffectsTab(const QString& tabName) {
        auto* scroll = new QScrollArea(tabs_);
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        auto* content = new QWidget(scroll);
        content->setObjectName("viperEffectsContent");
        auto* column = new QVBoxLayout(content);
        column->setSpacing(10);
        column->setContentsMargins(8, 10, 8, 10);
        column->addStretch(); // stretches after panels are added (moved below)
        scroll->setWidget(content);
        // Qt treats a single '&' as a mnemonic and draws an underscore.
        // Double it for the user-facing EQ & Tone tab label.
        tabs_->addTab(scroll, tabName == "EQ & Tone" ? "EQ && Tone" : tabName);
        return column;
    }
    void addTabNotice(QVBoxLayout* column, const QString& heading, const QString& detail) {
        // Informational only: deliberately no switches, mappings or DSP writes.
        auto* panel = new QFrame(tabs_);
        panel->setObjectName("viperRoadmapCard");
        const bool dark = palette().color(QPalette::Window).lightness() < 130;
        panel->setStyleSheet(QString(
            "QFrame#viperRoadmapCard { background:%1; border:1px dashed %2; border-radius:13px; }")
            .arg(dark ? "#273638" : "#f0f4f5", dark ? "#52666a" : "#bbcfd2"));
        auto* stack = new QVBoxLayout(panel);
        stack->setContentsMargins(18, 15, 18, 15);
        auto* headingLabel = new QLabel(heading, panel);
        QFont headingFont = headingLabel->font();
        headingFont.setBold(true);
        headingLabel->setFont(headingFont);
        stack->addWidget(headingLabel);
        auto* detailLabel = new QLabel(detail, panel);
        detailLabel->setWordWrap(true);
        detailLabel->setStyleSheet("color: palette(mid);");
        stack->addWidget(detailLabel);
        column->insertWidget(column->count() - 1, panel);
    }
    void addEffectPanel(QVBoxLayout* column, QGroupBox* group, const QString& key,
                        QCheckBox* enable = nullptr) {
        // Each effect is a rounded card: its header and independent toggle
        // remain visible even when the settings body is collapsed.
        auto* panel = new QFrame(tabs_);
        panel->setObjectName("viperEffectCard");
        const bool dark = palette().color(QPalette::Window).lightness() < 130;
        panel->setStyleSheet(QString(
            "QFrame#viperEffectCard { background:%1; border:1px solid %2; border-radius:16px; }")
            .arg(dark ? "#293a3e" : "#ebf3f5", dark ? "#405659" : "#d8e7e9"));
        auto* stack = new QVBoxLayout(panel);
        stack->setContentsMargins(16, 12, 16, 12);
        stack->setSpacing(4);

        const QString title = group->title();
        group->setTitle(QString());
        group->setFlat(true);
        group->setParent(panel);
        group->setStyleSheet("QGroupBox { background: transparent; border: none; }");
        if (auto* form = qobject_cast<QFormLayout*>(group->layout())) {
            form->setContentsMargins(5, 6, 5, 7);
            form->setHorizontalSpacing(16);
            form->setVerticalSpacing(8);
            form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        }
        auto* header = new QHBoxLayout();
        header->setContentsMargins(0, 0, 0, 0);
        header->setSpacing(10);
        auto* glyph = new EffectGlyph(key, panel);
        glyph->setColor(QColor(dark ? "#8fd2d7" : "#536c70"));
        header->addWidget(glyph);
        auto* expand = new QToolButton(panel);
        expand->setText(title);
        expand->setAccessibleName("Expand or collapse " + title + " settings");
        expand->setCheckable(true);
        expand->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        expand->setAutoRaise(true);
        expand->setCursor(Qt::PointingHandCursor);
        expand->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        expand->setMinimumHeight(43);
        expand->setStyleSheet("QToolButton { font-weight: 600; font-size: 14px; border: none; padding: 7px 3px; text-align:left; }");
        const bool open = settings_.value("ui/expanded/" + key, key == "bass").toBool();
        expand->setChecked(open);
        expand->setArrowType(open ? Qt::DownArrow : Qt::RightArrow);
        header->addWidget(expand, 1);
        if (enable) {
            // Detach the original checkbox from its QFormLayout and move the
            // exact same QCheckBox into the permanent card header. No replacement
            // control is created, so persistence and DSP connections are intact.
            auto* form = qobject_cast<QFormLayout*>(group->layout());
            if (form) {
                for (int i = 0; i < form->rowCount(); ++i) {
                    const auto* item = form->itemAt(i, QFormLayout::FieldRole);
                    if (item && item->widget() == enable) {
                        const auto taken = form->takeRow(i);
                        if (taken.labelItem) {
                            delete taken.labelItem->widget();
                            delete taken.labelItem;
                        }
                        delete taken.fieldItem;
                        break;
                    }
                }
            }
            enable->setParent(panel);
            enable->setAccessibleName("Enable " + title);
            if (enable->isEnabled())
                enable->setToolTip("Turn " + title + " processing on or off independently of the expanded settings panel.");
            header->addWidget(enable);
        }
        stack->addLayout(header);
        stack->addWidget(group);
        group->setVisible(open);
        connect(expand, &QToolButton::toggled, this, [this, group, expand, key](bool expanded) {
            // Visual state only. NEVER call push(), setChecked(enable), or write
            // the effect's processing flags from this handler.
            group->setVisible(expanded);
            expand->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
            settings_.setValue("ui/expanded/" + key, expanded);
        });
        column->insertWidget(column->count() - 1, panel);
    }
    struct SliderRow { QSlider* slider; QLabel* value; };
    struct MbBandRows {
        QCheckBox* enabled{};
        SliderRow threshold{}, ratio{}, knee{}, gain{}, attack{}, release{};
    };
    SliderRow addSlider(QFormLayout* form, const char* label, int min, int max) {
        // Android-inspired parameter presentation: label and live value share
        // a row, with a full-width slider underneath.  This is visual only.
        auto* container = new QWidget(this);
        auto* vertical = new QVBoxLayout(container);
        vertical->setContentsMargins(0, 5, 0, 8);
        vertical->setSpacing(8);
        auto* heading = new QHBoxLayout();
        heading->setContentsMargins(0, 0, 0, 0);
        auto* caption = new QLabel(QString::fromUtf8(label), container);
        auto* value = new QLabel(container);
        value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        value->setProperty("viperSliderValue", true);
        value->setStyleSheet("color: #23818b; font-weight: 600;");
        heading->addWidget(caption, 1);
        heading->addWidget(value);
        vertical->addLayout(heading);
        auto* slider = new SafeSlider(Qt::Horizontal, container);
        slider->setRange(min, max);
        slider->setSingleStep(1);
        slider->setMinimumHeight(28);
        const bool dark = palette().color(QPalette::Window).lightness() < 130;
        const QString remaining = dark ? "#52666a" : "#cde4e8";
        slider->setStyleSheet(QString(R"(
            QSlider::groove:horizontal { height: 10px; border-radius: 5px; background: %1; }
            QSlider::sub-page:horizontal { border-radius: 5px; background: #236e76; }
            QSlider::add-page:horizontal { border-radius: 5px; background: %1; }
            QSlider::handle:horizontal { background: #236e76; border: 1px solid #d4e8ea;
                                         width: 9px; margin: -8px 0; border-radius: 4px; }
        )").arg(remaining));
        vertical->addWidget(slider);
        form->addRow(container);
        return {slider, value};
    }
    void updateLabels() {
        strength_.value->setText(QString::number(strength_.slider->value() / 10.0, 'f', 1) + " / 10");
        frequency_.value->setText(QString::number(frequency_.slider->value()) + " Hz");
        mono_strength_.value->setText(QString::number(mono_strength_.slider->value()/10.0, 'f', 1) + " / 10");
        mono_frequency_.value->setText(QString::number(mono_frequency_.slider->value()) + " Hz");
        psycho_cutoff_.value->setText(QString::number(psycho_cutoff_.slider->value()) + " Hz");
        psycho_intensity_.value->setText(QString::number(psycho_intensity_.slider->value()) + "%");
        psycho_original_.value->setText(QString::number(psycho_original_.slider->value()) + "%");
        for (const auto& row : {dyn_xlo_, dyn_xhi_, dyn_ylo_, dyn_yhi_})
            row.value->setText(QString::number(row.slider->value()) + " Hz");
        for (const auto& row : {dyn_side_lo_, dyn_side_hi_, dyn_strength_})
            row.value->setText(QString::number(row.slider->value()) + "%");
        convolver_cross_.value->setText(QString::number(convolver_cross_.slider->value()) + "%");
        clarity_gain_.value->setText(QString::number(clarity_gain_.slider->value() / 10.0, 'f', 1) + " / 4.5");
        spectrum_frequency_.value->setText(QString::number(spectrum_frequency_.slider->value() * 100) + " Hz");
        spectrum_exciter_.value->setText(QString::number(spectrum_exciter_.slider->value() / 10.0, 'f', 1) + " / 6.0");
        tube_drive_.value->setText(QString::number(tube_drive_.slider->value()) + "%");
        tube_mix_.value->setText(QString::number(tube_mix_.slider->value()) + "%");
        reverb_room_.value->setText(QString::number(reverb_room_.slider->value()) + "%");
        reverb_width_.value->setText(QString::number(reverb_width_.slider->value()) + "%");
        reverb_damp_.value->setText(QString::number(reverb_damp_.slider->value()) + "%");
        reverb_wet_.value->setText(QString::number(reverb_wet_.slider->value()) + "%");
        reverb_dry_.value->setText(QString::number(reverb_dry_.slider->value()) + "%");
        diff_delay_.value->setText(QString::number(diff_delay_.slider->value()) + " ms");
        diff_mix_.value->setText(QString::number(diff_mix_.slider->value()) + "%");
        field_widen_.value->setText(QString::number(field_widen_.slider->value() / 10.0, 'f', 1));
        field_mid_.value->setText(QString::number(field_mid_.slider->value() / 100.0, 'f', 2));
        for (int i=0; i<4; ++i)
            mb_crossovers_[i].value->setText(QString::number(mb_crossovers_[i].slider->value()) + " Hz");
        updateMultibandFrequencyLabels();
        for (auto& band : mb_bands_) {
            band.threshold.value->setText(QString::number(band.threshold.slider->value()) + " dBFS");
            band.ratio.value->setText(QString::number(band.ratio.slider->value()/10.0, 'f', 1) + ":1");
            band.knee.value->setText(QString::number(band.knee.slider->value()) + " dB");
            band.gain.value->setText(QString::number(band.gain.slider->value()) + " dB");
            band.attack.value->setText(QString::number(band.attack.slider->value()) + " ms");
            band.release.value->setText(QString::number(band.release.slider->value()) + " ms");
        }
        fet_threshold_.value->setText(QString::number(fet_threshold_.slider->value()) + " dBFS");
        fet_ratio_.value->setText(QString::number(fet_ratio_.slider->value() / 10.0, 'f', 1) + ":1");
        fet_knee_.value->setText(QString::number(fet_knee_.slider->value()) + " dB");
        fet_gain_.value->setText(QString::number(fet_gain_.slider->value()) + " dB");
        fet_attack_.value->setText(QString::number(fet_attack_.slider->value()) + " ms");
        fet_release_.value->setText(QString::number(fet_release_.slider->value()) + " ms");
        for (const auto& row : {imager_low_, imager_mid_, imager_high_})
            row.value->setText(QString::number(row.slider->value()) + "%");
        imager_low_xover_.value->setText(QString::number(imager_low_xover_.slider->value()) + " Hz");
        imager_high_xover_.value->setText(QString::number(imager_high_xover_.slider->value() * 100) + " Hz");
        field_depth_.value->setText(field_depth_.slider->value() == 0 ? "Off" :
            QString::number(150 + field_depth_.slider->value() * 50));
        limiter_threshold_.value->setText(QString::number(20.0 * std::log10(limiter_threshold_.slider->value() / 1000.0), 'f', 1) + " dBFS");
        limiter_gain_.value->setText(QString::number(limiter_gain_.slider->value()) + "%");
        limiter_pan_.value->setText(limiter_pan_.slider->value() == 0 ? "Center" :
            QString::number(std::abs(limiter_pan_.slider->value())) + "% " +
            (limiter_pan_.slider->value() < 0 ? "Left" : "Right"));
        agc_strength_.value->setText(QString::number(agc_strength_.slider->value() / 100.0, 'f', 2));
        agc_max_gain_.value->setText(QString::number(agc_max_gain_.slider->value() / 100.0, 'f', 2) + "×");
        agc_threshold_.value->setText(QString::number(agc_threshold_.slider->value() / 1000.0, 'f', 3));
        lufs_target_.value->setText(QString::number(lufs_target_.slider->value() / 10.0, 'f', 1) + " LUFS");
        lufs_gain_.value->setText(QString::number(lufs_gain_.slider->value() / 10.0, 'f', 1) + " dB");
        if (output_warning_) output_warning_->setVisible(agc_enabled_->isChecked() && lufs_enabled_->isChecked());
        diff_lowpass_.value->setText(diff_lowpass_.slider->value() == 0
            ? "Off" : QString::number(diff_lowpass_.slider->value() * 100) + " Hz");
    }
    QString ddcDirectory() const {
        return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/viper4linux/VDC";
    }
    void refreshDdcLibrary() {
        if (!ddc_library_ || ddc_library_->popupIsOpen()) {
            ddc_library_dirty_ = true;
            return;
        }
        QDir().mkpath(ddcDirectory());
        if (ddc_watcher_ && !ddc_watcher_->directories().contains(ddcDirectory()))
            ddc_watcher_->addPath(ddcDirectory());
        const QString selected = settings_.value("ddc_profile", "").toString();
        const QDir dir(ddcDirectory());
        QList<QPair<QString, QString>> entries;
        entries.append({"None", QString()});
        for (const QString& name : dir.entryList({"*.vdc", "*.VDC"},
                QDir::Files | QDir::Readable, QDir::Name | QDir::IgnoreCase))
            entries.append({name, dir.absoluteFilePath(name)});
        bool selectedFound = selected.isEmpty();
        for (const auto& entry : entries) {
            if (entry.second == selected) { selectedFound = true; break; }
        }
        // Preserve older settings pointing outside the VDC directory.
        if (!selectedFound)
            entries.append({"External: " + QFileInfo(selected).fileName(), selected});
        bool identical = entries.size() == ddc_library_->count();
        if (identical) {
            for (int i = 0; i < entries.size(); ++i) {
                if (ddc_library_->itemText(i) != entries[i].first ||
                    ddc_library_->itemData(i).toString() != entries[i].second) {
                    identical = false;
                    break;
                }
            }
        }
        const QSignalBlocker block(ddc_library_);
        if (!identical) {
            ddc_library_->clear();
            for (const auto& entry : entries) {
                const int index = ddc_library_->count();
                ddc_library_->addItem(entry.first, entry.second);
                ddc_library_->setItemData(index, entry.first, Qt::ToolTipRole);
            }
        }
        ddc_library_->setCurrentIndex(std::max(0, ddc_library_->findData(selected)));
        updateDdcFileName(selected);
        ddc_library_dirty_ = false;
    }
    QString irsDirectory() const {
        return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/viper4linux/IRS";
    }
    void refreshIrsLibrary() {
        if (convolver_library_->popupIsOpen()) {
            irs_library_dirty_ = true;
            return;
        }
        QDir().mkpath(irsDirectory());
        if (irs_watcher_ && !irs_watcher_->directories().contains(irsDirectory()))
            irs_watcher_->addPath(irsDirectory());
        const QString selected = settings_.value("convolver_kernel").toString();
        const QDir dir(irsDirectory());
        QList<QPair<QString, QString>> entries;
        entries.append({"None", QString()});
        for (const QString& name : dir.entryList({"*.irs", "*.IRS", "*.wav", "*.WAV"}, QDir::Files | QDir::Readable, QDir::Name | QDir::IgnoreCase))
            entries.append({name, dir.absoluteFilePath(name)});
        bool selectedFound = selected.isEmpty();
        for (const auto& entry : entries) {
            if (entry.second == selected) { selectedFound = true; break; }
        }
        if (!selectedFound)
            entries.append({"External: " + QFileInfo(selected).fileName(), selected});

        bool identical = entries.size() == convolver_library_->count();
        if (identical) {
            for (int i = 0; i < entries.size(); ++i) {
                if (convolver_library_->itemText(i) != entries[i].first ||
                    convolver_library_->itemData(i).toString() != entries[i].second) {
                    identical = false;
                    break;
                }
            }
        }
        const QSignalBlocker block(convolver_library_);
        if (!identical) {
            convolver_library_->clear();
            for (const auto& entry : entries)
            {
                const int index = convolver_library_->count();
                convolver_library_->addItem(entry.first, entry.second);
                // The popup may elide a long entry, but its tooltip is complete.
                convolver_library_->setItemData(index, entry.first, Qt::ToolTipRole);
            }
        }
        // Preserve selection without an accidental DSP reload or a popup jump.
        convolver_library_->setCurrentIndex(std::max(0, convolver_library_->findData(selected)));
        updateIrsName(selected);
        irs_library_dirty_ = false;
    }
    void updateIrsName(const QString& path) {
        const QString name = path.isEmpty() ? QString("None") : QFileInfo(path).fileName();
        convolver_full_name_->setPlainText("Selected IRS: " + name);
        convolver_full_name_->setToolTip(path.isEmpty() ? QString("No impulse response") : path);
        // The native dropdown's text may be elided. Tooltips and this
        // wrapping, selectable line always retain the full original name.
        convolver_library_->setToolTip(path.isEmpty() ? QString("No impulse response") : path);
    }
    void updateDdcFileName(const QString& path) {
        ddc_file_name_->setText(path.isEmpty() ? "Selected profile: None" :
            "Selected profile: " + QFileInfo(path).fileName());
        ddc_file_name_->setToolTip(path);
    }
    void requestDdcReload() {
        if (ensureBackend()) {
            mapping_.get()->ddc_request_serial.fetch_add(1, std::memory_order_release);
            ddc_status_->setText("Loading DDC profile…");
        } else {
            ddc_status_->setText("DDC selection saved; will load at backend startup.");
        }
    }
    void selectDdcProfile(const QString& path) {
        if (path.isEmpty()) settings_.remove("ddc_profile");
        else settings_.setValue("ddc_profile", path);
        settings_.sync();
        updateDdcFileName(path);
        requestDdcReload();
        refreshDdcLibrary();
    }
    void requestIrsLoad() {
        if (ensureBackend()) {
            auto* c = mapping_.get();
            c->convolver_request_serial.fetch_add(1, std::memory_order_release);
            convolver_status_->setText("Loading IRS into running backend…");
        } else {
            convolver_status_->setText("Selection saved; will load when backend starts.");
        }
    }
    void selectIrPath(const QString& path) {
        if (path.isEmpty()) settings_.remove("convolver_kernel");
        else settings_.setValue("convolver_kernel", path);
        settings_.sync(); // worker reads this file after receiving the IPC request
        requestIrsLoad();
        updateIrsName(path); // even if the popup is still open and frozen
        refreshIrsLibrary();
    }
    // Keep all window/tray policy separate from DSP parameter persistence.
    void saveUiGeometry() {
        if (isVisible() && !isMinimized()) {
            settings_.setValue("ui/mainWindowSize", isMaximized() ? normalGeometry().size() : size());
            settings_.setValue("ui/mainWindowMaximized", isMaximized());
        }
        settings_.setValue("ui/graphicEqEditorSize", eq_editor_->size());
        settings_.setValue("ui/dynamicEqEditorSize", deq_editor_->size());
        settings_.sync();
    }
    bool trayAvailable() const {
        return tray_icon_ && QSystemTrayIcon::isSystemTrayAvailable();
    }
    void restoreFromTray() {
        // Restore the real GUI *before* asking Qt to clear its minimized state.
        // When minimized-to-tray, we only hid the native window; we did not
        // mutate its window state while hidden (which confused Cinnamon/X11).
        if (isMinimized()) {
            if (tray_restore_maximized_) showMaximized();
            else showNormal();
        } else if (!isVisible()) {
            show();
        }
        tray_restore_maximized_ = false;
        raise();
        activateWindow();
    }
    void quitGui() {
        saveUiGeometry();
        qApp->quit(); // Does not touch the separate PipeWire backend.
    }
    void setupTray() {
        tray_icon_ = new QSystemTrayIcon(QIcon(":/viper4linux/blue.png"), this);
        tray_icon_->setToolTip("Viper4Linux — PipeWire effects");
        auto* menu = new QMenu(this);
        auto* showAction = menu->addAction("Show / Hide Window");
        connect(showAction, &QAction::triggered, this, [this] {
            if (isVisible() && !isMinimized()) { saveUiGeometry(); hide(); }
            else restoreFromTray();
        });
        tray_processing_action_ = menu->addAction("Enable Processing");
        tray_processing_action_->setCheckable(true);
        connect(tray_processing_action_, &QAction::triggered, this, [this](bool enabled) {
            global_->setChecked(enabled); // Existing master toggle handles DSP publishing.
        });
        connect(global_, &QCheckBox::toggled, this, [this](bool enabled) {
            if (tray_processing_action_) tray_processing_action_->setChecked(enabled);
        });
        connect(menu, &QMenu::aboutToShow, this, [this] {
            tray_processing_action_->setChecked(global_->isChecked());
        });
        menu->addSeparator();
        auto* prefs = menu->addAction("Preferences…");
        connect(prefs, &QAction::triggered, this, [this] { showPreferences(); });
        menu->addSeparator();
        auto* quit = menu->addAction("Quit GUI");
        connect(quit, &QAction::triggered, this, [this] { quitGui(); });
        tray_icon_->setContextMenu(menu);
        connect(tray_icon_, &QSystemTrayIcon::activated, this,
                [this](QSystemTrayIcon::ActivationReason reason) {
            if (reason != QSystemTrayIcon::Trigger && reason != QSystemTrayIcon::DoubleClick)
                return;
            if (isVisible() && !isMinimized()) { saveUiGeometry(); hide(); }
            else restoreFromTray();
        });
        tray_icon_->show();
    }
    QString autostartFile() const {
        const QString config = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
        return config + "/autostart/viper4linux-re.desktop";
    }
    bool setAutostart(bool enabled) {
        const QString target = autostartFile();
        if (!enabled) {
            if (!QFile::exists(target)) return true;
            return QFile::remove(target);
        }
        if (!QDir().mkpath(QFileInfo(target).absolutePath())) return false;
        QSaveFile file(target);
        if (!file.open(QIODevice::WriteOnly)) return false;
        const QByteArray desktop =
            "[Desktop Entry]\nType=Application\nName=Viper4Linux\n"
            "Comment=Start Viper4Linux at login\nExec=viper4linux\n"
            "TryExec=viper4linux\nIcon=viper4linux\n"
            "Terminal=false\nX-GNOME-Autostart-enabled=true\n";
        if (file.write(desktop) != desktop.size()) { file.cancelWriting(); return false; }
        return file.commit();
    }
    QString outputMemoryDirectory() const {
        return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
               + "/viper4linux/outputs";
    }
    bool outputMemoryEnabled() const {
        QFile file(outputMemoryDirectory() + "/options.json");
        if (!file.open(QIODevice::ReadOnly)) return false;
        const auto doc = QJsonDocument::fromJson(file.readAll());
        return doc.isObject() && doc.object().value("format").toInt() == 1 &&
               doc.object().value("remember_outputs").toBool(false);
    }
    bool setOutputMemoryEnabled(bool enabled, QString* error) {
        const QString directory = outputMemoryDirectory();
        if (!QDir().mkpath(directory)) {
            *error = "Could not create Audio Output settings directory.";
            return false;
        }
        QSaveFile file(directory + "/options.json");
        if (!file.open(QIODevice::WriteOnly)) {
            *error = file.errorString();
            return false;
        }
        QJsonObject options;
        options.insert("format", 1);
        options.insert("remember_outputs", enabled);
        const QByteArray content = QJsonDocument(options).toJson(QJsonDocument::Indented);
        if (file.write(content) != content.size() || !file.commit()) {
            *error = "Could not save Audio Output preferences.";
            return false;
        }
        return true;
    }
    void updateAudioOutputDisplay() {
        const QString runtime = qEnvironmentVariable("XDG_RUNTIME_DIR");
        if (runtime.isEmpty()) {
            audio_output_label_->setText("Audio Output: unavailable");
            return;
        }
        QFile file(QDir(runtime).filePath("viper4linux-output-status.json"));
        if (!file.open(QIODevice::ReadOnly)) {
            audio_output_label_->setText("Audio Output: detecting…");
            return;
        }
        const QJsonDocument status = QJsonDocument::fromJson(file.readAll());
        if (!status.isObject() || status.object().value("format").toInt() != 1 ||
            std::abs(status.object().value("timestamp").toDouble() -
                     double(QDateTime::currentSecsSinceEpoch())) > 5.0) {
            audio_output_label_->setText("Audio Output: unavailable");
            return;
        }
        const QJsonObject active = status.object().value("active").toObject();
        const QString description = active.value("description").toString();
        const QString identity = active.value("key").toString();
        if (description.isEmpty() || identity.size() != 64) {
            audio_output_label_->setText("Audio Output: detecting…");
            return;
        }
        audio_output_label_->setText("Audio Output: " + description);
        const bool memoryEnabled = status.object().value("memory_enabled").toBool(false);
        audio_output_label_->setToolTip("Audio Output identity: " + identity.left(12) +
            (memoryEnabled ? "\nAutomatic per-output effect restoration enabled."
                           : "\nAutomatic per-output effect restoration disabled."));
        const int revision = status.object().value("memory_revision").toInt();
        const QString session = QString::number(status.object().value("manager_pid").toInt());
        const QString token = session + ":" + QString::number(revision);
        if (revision > 0 && token != last_output_memory_revision_) {
            last_output_memory_revision_ = token;
            settings_.sync();
            // These controls have their own editor-side arrays. Basic sliders
            // and switches already follow the live IPC through readBackend().
            loadDeqSettings();
            loadMultibandSettings("");
            updateDdcFileName(settings_.value("ddc_profile").toString());
            refreshDdcLibrary();
            refreshIrsLibrary();
        }
    }

    void showPreferences() {
        QDialog dialog(this);
        dialog.setWindowTitle("Viper4Linux — Preferences");
        dialog.setMinimumWidth(395);
        auto* body = new QVBoxLayout(&dialog);
        auto* startup = new QGroupBox("When Viper4Linux starts", &dialog);
        auto* startLayout = new QVBoxLayout(startup);
        auto* startGroup = new QButtonGroup(&dialog);
        auto* startNormal = new QRadioButton("Show main window", startup);
        auto* startMin = new QRadioButton("Start minimized to taskbar", startup);
        auto* startTray = new QRadioButton("Start hidden in system tray", startup);
        startGroup->addButton(startNormal, 0);
        startGroup->addButton(startMin, 1);
        startGroup->addButton(startTray, 2);
        for (auto* item : {startNormal, startMin, startTray}) startLayout->addWidget(item);
        const QString initial = settings_.value("ui/startupBehavior", "show").toString();
        if (initial == "minimized") startMin->setChecked(true);
        else if (initial == "tray") startTray->setChecked(true);
        else startNormal->setChecked(true);
        body->addWidget(startup);

        auto* minimize = new QCheckBox("Minimize to tray instead of taskbar", &dialog);
        minimize->setChecked(settings_.value("ui/minimizeToTray", false).toBool());
        body->addWidget(minimize);

        auto* closeBox = new QGroupBox("When the close (X) button is clicked", &dialog);
        auto* closeLayout = new QVBoxLayout(closeBox);
        auto* closeGroup = new QButtonGroup(&dialog);
        auto* closeQuit = new QRadioButton("Quit GUI (keep audio backend running)", closeBox);
        auto* closeMin = new QRadioButton("Minimize to taskbar", closeBox);
        auto* closeTray = new QRadioButton("Hide to system tray", closeBox);
        closeGroup->addButton(closeQuit, 0);
        closeGroup->addButton(closeMin, 1);
        closeGroup->addButton(closeTray, 2);
        for (auto* item : {closeQuit, closeMin, closeTray}) closeLayout->addWidget(item);
        const QString ending = settings_.value("ui/closeBehavior", "quit").toString();
        if (ending == "minimized") closeMin->setChecked(true);
        else if (ending == "tray") closeTray->setChecked(true);
        else closeQuit->setChecked(true);
        body->addWidget(closeBox);

        auto* autostart = new QCheckBox("Launch automatically when I log in", &dialog);
        autostart->setChecked(QFile::exists(autostartFile()));
        body->addWidget(autostart);
        auto* memoryGroup = new QGroupBox("Audio Output", &dialog);
        auto* memoryLayout = new QVBoxLayout(memoryGroup);
        auto* rememberOutputs = new QCheckBox("Remember effects separately for each Audio Output",
                                             memoryGroup);
        rememberOutputs->setChecked(outputMemoryEnabled());
        memoryLayout->addWidget(rememberOutputs);
        auto* memoryExplanation = new QLabel(
            "When enabled, changes are saved for each physical output and restored "
            "when you switch back. Works while the GUI is closed. "
            "The current sound is retained when enabling; named presets are independent.",
            memoryGroup);
        memoryExplanation->setWordWrap(true);
        memoryLayout->addWidget(memoryExplanation);
        body->addWidget(memoryGroup);
        auto* notice = new QLabel(
            "If the desktop tray is unavailable, Viper4Linux stays accessible\n"
            "as an ordinary window or taskbar application.", &dialog);
        notice->setWordWrap(true);
        body->addWidget(notice);
        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        body->addWidget(buttons);
        if (dialog.exec() != QDialog::Accepted) return;
        settings_.setValue("ui/startupBehavior",
            startGroup->checkedId() == 2 ? "tray" : startGroup->checkedId() == 1 ? "minimized" : "show");
        settings_.setValue("ui/minimizeToTray", minimize->isChecked());
        settings_.setValue("ui/closeBehavior",
            closeGroup->checkedId() == 2 ? "tray" : closeGroup->checkedId() == 1 ? "minimized" : "quit");
        settings_.sync();
        if (!setAutostart(autostart->isChecked())) {
            QMessageBox::warning(this, "Autostart",
                "Could not update the XDG autostart entry:\n" + autostartFile());
        }
        if (rememberOutputs->isChecked() != outputMemoryEnabled()) {
            // Enabling while the native channel is unavailable cannot capture
            // the current sound. Never claim to remember an unknown state.
            if (rememberOutputs->isChecked() && !ensureBackend()) {
                QMessageBox::warning(this, "Audio Output Settings",
                    "Audio processing must be ready before enabling per-output memory. "
                    "Check that the ViPER backend and native DSP channel are running, then try again.");
            } else {
                QString error;
                if (!setOutputMemoryEnabled(rememberOutputs->isChecked(), &error))
                    QMessageBox::warning(this, "Audio Output Settings", error);
            }
        }
    }
    QMenu* makeApplicationMenu(QWidget* parent) {
        auto* menu = new QMenu(parent);
        auto* preferences = menu->addAction("Preferences…");
        connect(preferences, &QAction::triggered, this, [this] { showPreferences(); });
        auto* presets = menu->addAction("Presets…");
        connect(presets, &QAction::triggered, this, [this] { showPresetManager(); });
        auto* libraries = menu->addMenu("Open Library");
        auto openFolder = [this](const QString& path) {
            if (!QDir().mkpath(path) ||
                !QDesktopServices::openUrl(QUrl::fromLocalFile(path)))
                QMessageBox::warning(this, "Open Library", "Could not open: " + path);
        };
        auto* irs = libraries->addAction("IRS impulse responses");
        connect(irs, &QAction::triggered, this, [openFolder] {
            openFolder(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
                       + "/viper4linux/IRS");
        });
        auto* vdc = libraries->addAction("VDC correction profiles");
        connect(vdc, &QAction::triggered, this, [openFolder] {
            openFolder(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
                       + "/viper4linux/VDC");
        });
        auto* logAction = menu->addAction("View Backend Log");
        connect(logAction, &QAction::triggered, this, [this] {
            const QString state = qEnvironmentVariable("XDG_STATE_HOME", QDir::homePath() + "/.local/state");
            const QString path = state + "/viper4linux/backend.log";
            QFile log(path);
            if (!log.open(QIODevice::ReadOnly)) {
                QMessageBox::information(this, "Backend Log", "No backend log found at:\n" + path);
                return;
            }
            const qint64 start = std::max<qint64>(0, log.size() - 128 * 1024);
            log.seek(start);
            QDialog viewer(this);
            viewer.setWindowTitle("Viper4Linux — Backend Log");
            viewer.resize(750, 480);
            auto* layout = new QVBoxLayout(&viewer);
            auto* text = new QPlainTextEdit(&viewer);
            text->setReadOnly(true);
            text->setPlainText(QString::fromUtf8(log.readAll()));
            layout->addWidget(text);
            auto* close = new QDialogButtonBox(QDialogButtonBox::Close, &viewer);
            connect(close, &QDialogButtonBox::rejected, &viewer, &QDialog::reject);
            layout->addWidget(close);
            viewer.exec();
        });
        menu->addSeparator();
        auto* guide = menu->addAction("Effect Guide");
        connect(guide, &QAction::triggered, this, [this] { showHelp(); });
        auto* about = menu->addAction("About Viper4Linux");
        connect(about, &QAction::triggered, this, [this] { showAbout(); });
        menu->addSeparator();
        auto* quit = menu->addAction("Quit GUI");
        connect(quit, &QAction::triggered, this, [this] { quitGui(); });
        return menu;
    }
    void showAbout() {
        // A minimal, non-intrusive About dialog, kept independent of the
        // effect help. The development revision comes from the source commit.
        QMessageBox about(this);
        about.setWindowTitle("About Viper4Linux");
        about.setWindowIcon(QIcon(":/viper4linux/blue.png"));
        about.setIconPixmap(QPixmap(":/viper4linux/blue.png").scaled(
            48, 48, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        about.setText("<b>Viper4Linux</b>");
        about.setInformativeText(
            QString("Version: <b>%1</b><br>"
                    "Application commit: <code>%2</code><br>"
                    "ViPERDSP upstream: <code>%3</code><br><br>"
                    "Native ViPERDSP processing via PipeWire; Qt6 control interface.")
                .arg(VIPER4LINUX_DEV_VERSION,
                     VIPER4LINUX_DEV_REVISION,
                     VIPER4LINUX_VIPERDSP_REVISION));
        about.setStandardButtons(QMessageBox::Ok);
        about.exec();
    }
    void showHelp() {
        QDialog help(this);
        help.setWindowTitle("Viper4Linux — Effect Guide");
        help.resize(650, 590);
        auto* layout = new QVBoxLayout(&help);
        auto* text = new QTextBrowser(&help);
        text->setOpenExternalLinks(true);
        text->setHtml(R"HTML(<h2>Viper4Linux — Effects</h2>
<p>These descriptions explain the available audio controls and their behavior.</p>
<h3>Output</h3><p><b>Master Limiter:</b> Keeps output peaks under the selected threshold; remains active during bypass.</p>
<p><b>Playback Gain Control (AGC):</b> Automatically adjusts changing playback levels.</p>
<p><b>LUFS Targeting:</b> Adjusts perceived loudness toward a target over time; avoid combining with AGC without testing.</p>
<h3>Bass</h3><p><b>ViPER Bass:</b> Low-frequency enhancement, with Natural, Pure Bass+ and Subwoofer modes.</p>
<p><b>Mono Bass:</b> Uses a mono-derived bass component for both channels.</p>
<p><b>Psychoacoustic Bass:</b> Generates harmonics to make low frequencies more perceptible on smaller drivers.</p>
<p><b>Dynamic System:</b> Shapes bass and dynamic frequency response using crossover and gain parameters.</p>
<h3>EQ &amp; Tone</h3><p><b>Graphic Equalizer:</b> Native 10/15/25/31-band minimum-phase IIR EQ. Select a curve preset or click the read-only response graph to open vertical faders. Save your own named curves; zero dB is flat.</p><p><b>Clarity:</b> Enhances high-frequency detail in Natural, Ozone or XHiFi modes.</p>
<p><b>Spectrum Extension:</b> Adds upper-frequency harmonics above a reference frequency.</p>
<p><b>Tube Simulator:</b> Native ViPERDSP tube-coloration effect with 12AX7, 12AU7 and 6N1P models, drive and wet/dry mix.</p>
<p><b>AnalogX:</b> Native ViPERDSP harmonic coloration with Mild, Medium and Strong modes. The underlying effect briefly resets its filters when enabled or switched.</p>
<h3>Spatial</h3><p><b>Crossfeed (ViPER Cure):</b> Filters a small portion of each stereo channel into the opposite ear (Mild/Medium/Strong), for more speaker-like headphone listening.</p><p><b>Headphone Surround+:</b> Headphone-oriented spatial processing.</p>
<p><b>Differential Surround:</b> Uses interchannel delay and filtering to alter stereo width.</p>
<p><b>Field Surround:</b> Adjusts perceived width, center image and depth.</p>
<p><b>Reverberation:</b> Adds simulated room reflections and ambience.</p>
<p><b>Stereo Imager:</b> Native three-band stereo width processing. Low, mid and high widths each range from 0 to 200%; 100% is neutral. Separate low/mid (80–400 Hz) and mid/high (2–8 kHz) crossovers. It is off by default, and excessive widening may reduce mono compatibility.</p>
<h3>Sound Correction</h3><p><b>Convolver:</b> Applies a WAV-encoded .irs or .wav impulse response. Select from the local IRS library, or use Import to copy a new file into it. Kernel changes apply live.</p>
<p><b>IRS library:</b> ~/.local/share/viper4linux/IRS/ (or the XDG data home equivalent). Import multiple files at once; the dropdown updates when the directory changes, never while open. Different-rate IRs are resampled in the backend without restarting audio.</p>
<h3>Device correction</h3><p>Native ViPER-DDC reads standard .vdc correction profiles at 44.1/48 kHz; native Speaker Correction uses built-in fixed filters for loudspeakers.</p>)HTML");
        layout->addWidget(text);
        auto* close = new QPushButton("Close", &help);
        connect(close, &QPushButton::clicked, &help, &QDialog::accept);
        layout->addWidget(close, 0, Qt::AlignRight);
        help.exec();
    }
    void statusMessage(const QString& message) {
        status_->setText(message);
    }
    void migrateEqPrecision() {
        // Older masters saved 0.1 dB integers; new native EQ IPC uses 0.01 dB.
        // Convert settings exactly once, including saved custom EQ presets.
        if (settings_.value("eq_precision_hundredths", false).toBool()) return;
        auto convert = [this](const QString& key) {
            if (!settings_.contains(key)) return;
            const auto fields = settings_.value(key).toString().split(',');
            QStringList values;
            for (const auto& field : fields) {
                bool ok = false;
                const int prior = field.trimmed().toInt(&ok);
                if (!ok) return; // leave malformed user data untouched
                values << QString::number(std::clamp(prior * 10, -1200, 1200));
            }
            settings_.setValue(key, values.join(','));
        };
        for (int count : {10, 15, 25, 31}) {
            convert("eq_levels_" + QString::number(count));
            convert("preset/eq_levels_" + QString::number(count));
        }
        settings_.beginGroup("eq/user_presets");
        const auto names = settings_.childGroups();
        settings_.endGroup();
        for (const auto& name : names)
            convert("eq/user_presets/" + name + "/levels");
        settings_.setValue("eq_precision_hundredths", true);
        settings_.sync();
    }
    static QString flatEqCsv(int n) {
        QStringList values;
        for (int i = 0; i < n; ++i) values << "0";
        return values.join(",");
    }
    static int validEqBands(int n) {
        return (n == 10 || n == 15 || n == 25 || n == 31) ? n : 10;
    }
    static const std::array<float, 31>& eqFrequencies(int bands) {
        return viper_eq::frequencies(bands);
    }
    static const std::array<viper_eq::NamedCurve, 12>& builtInEq() {
        return viper_eq::builtIns();
    }
    static std::vector<int> interpolateEq(const std::vector<float>& srcHz,
                                           const std::vector<int>& srcLevels, int bands) {
        return viper_eq::interpolate(srcHz, srcLevels, bands);
    }
    void populateEqPresets() {
        const QString selected = eq_selected_preset_;
        const QSignalBlocker blocker(eq_preset_);
        eq_preset_->clear();
        for (const auto& item : builtInEq())
            eq_preset_->addItem(QString::fromLatin1(item.name),
                                "builtin:" + QString::fromLatin1(item.name));
        eq_preset_->insertSeparator(eq_preset_->count());
        eq_preset_->addItem("Custom", "custom");
        settings_.beginGroup("eq/user_presets");
        const QStringList names = settings_.childGroups();
        settings_.endGroup();
        if (!names.isEmpty()) eq_preset_->insertSeparator(eq_preset_->count());
        for (const QString& name : names)
            eq_preset_->addItem(name, "user:" + name);
        selectEqPreset(selected.isEmpty() ? "custom" : selected);
    }
    void selectEqPreset(const QString& requested) {
        const QSignalBlocker blocker(eq_preset_);
        int index = eq_preset_->findData(requested);
        if (index < 0) index = eq_preset_->findData("custom");
        if (index < 0) return;
        eq_preset_->setCurrentIndex(index);
        eq_selected_preset_ = eq_preset_->itemData(index).toString();
        settings_.setValue("eq/selected_preset", eq_selected_preset_);
        updateEqDeleteButton();
    }
    void updateEqDeleteButton() {
        if (eq_delete_button_)
            eq_delete_button_->setEnabled(eq_selected_preset_.startsWith("user:"));
    }
    void applyEqPreset(const QString& preset) {
        std::vector<float> freq;
        std::vector<int> vals;
        if (preset.startsWith("builtin:")) {
            const QString name = preset.mid(8);
            for (const auto& item : builtInEq()) {
                if (name != QString::fromLatin1(item.name)) continue;
                vals = item.levels(eq_active_bands_); // exact upstream layout, no interpolation
                break;
            }
        } else if (preset.startsWith("user:")) {
            const QString path = "eq/user_presets/" + preset.mid(5) + "/";
            const int savedBands = validEqBands(settings_.value(path + "bands", 10).toInt());
            const auto saved = settings_.value(path + "levels", "").toString().split(',');
            if (saved.size() == savedBands) {
                const auto& f = eqFrequencies(savedBands);
                for (int i = 0; i < savedBands; ++i) {
                    bool ok = false;
                    const int value = saved[i].toInt(&ok);
                    if (!ok) { freq.clear(); vals.clear(); break; }
                    freq.push_back(f[i]);
                    vals.push_back(std::clamp(value, -1200, 1200));
                }
            }
        }
        if (vals.empty()) { selectEqPreset("custom"); return; }
        const auto mapped = preset.startsWith("builtin:") ? vals :
            interpolateEq(freq, vals, eq_active_bands_);
        for (int i = 0; i < eq_active_bands_; ++i) {
            const QSignalBlocker blocker(eq_faders_[i]);
            eq_faders_[i]->setValue(mapped[i]);
            eq_values_[i]->setText(QString::number(mapped[i] / 100.0, 'f', 2));
        }
        saveEqLayout();
        updateEqOverview();
        selectEqPreset(preset);
        push();
    }
    void saveUserEqPreset() {
        bool accepted = false;
        QString name = QInputDialog::getText(this, "Save EQ preset", "Preset name:",
                                             QLineEdit::Normal, "", &accepted).trimmed();
        if (!accepted || name.isEmpty()) return;
        if (name.contains('/') || name.contains('\\') || name.size() > 70) {
            QMessageBox::warning(this, "Invalid preset name",
                "Use a name of at most 70 characters without slashes.");
            return;
        }
        settings_.beginGroup("eq/user_presets");
        const auto existing = settings_.childGroups();
        settings_.endGroup();
        for (const auto& other : existing) {
            if (other.compare(name, Qt::CaseInsensitive) == 0) {
                QMessageBox::warning(this, "Preset already exists",
                    "A saved EQ preset already has that name. Nothing was overwritten.");
                return;
            }
        }
        QStringList vals;
        for (int i = 0; i < eq_active_bands_; ++i)
            vals << QString::number(eq_faders_[i]->value());
        const QString path = "eq/user_presets/" + name + "/";
        settings_.setValue(path + "bands", eq_active_bands_);
        settings_.setValue(path + "levels", vals.join(','));
        settings_.sync();
        eq_selected_preset_ = "user:" + name;
        populateEqPresets();
    }
    void deleteUserEqPreset() {
        if (!eq_selected_preset_.startsWith("user:")) return;
        const QString name = eq_selected_preset_.mid(5);
        if (QMessageBox::question(this, "Delete EQ preset",
                "Delete saved preset '" + name + "'? The current EQ curve will be retained.")
                != QMessageBox::Yes) return;
        settings_.remove("eq/user_presets/" + name);
        eq_selected_preset_ = "custom";
        populateEqPresets();
    }
    void updateEqOverview() {
        if (!eq_overview_) return;
        std::vector<int> levels;
        levels.reserve(eq_active_bands_);
        for (int i = 0; i < eq_active_bands_; ++i)
            levels.push_back(eq_faders_[i]->value());
        eq_overview_->setLevels(levels);
    }

    void saveEqLayout() {
        QStringList levels;
        for (int i = 0; i < eq_active_bands_; ++i)
            levels << QString::number(eq_faders_[i]->value());
        settings_.setValue("eq_levels_" + QString::number(eq_active_bands_), levels.join(","));
    }
    void loadEqLayout() {
        const int n = validEqBands(eq_active_bands_);
        const QStringList stored = settings_.value("eq_levels_" + QString::number(n), "").toString().split(',');
        const auto& freqs = eqFrequencies(n);
        for (int i = 0; i < 31; ++i) {
            const QSignalBlocker blocker(eq_faders_[i]);
            if (i < n) {
                int value = 0;
                if (i < stored.size()) {
                    bool ok = false;
                    const int parsed = stored[i].toInt(&ok);
                    if (ok) value = std::clamp(parsed, -1200, 1200);
                }
                eq_faders_[i]->setValue(value);
                eq_values_[i]->setText(QString::number(value / 100.0, 'f', 2));
                const float f = freqs[i];
                const QString label = f >= 1000 ? QString::number(f / 1000.0f, 'g', 3) + "k"
                                                : QString::number(f, 'g', 4);
                eq_frequencies_[i]->setText(label);
                eq_faders_[i]->setToolTip(QString::number(f, 'g', 6) + " Hz: " + eq_values_[i]->text() + " dB");
            }
            eq_columns_[i]->setVisible(i < n);
        }
        syncGraphicEqEditorColumns();
        updateEqOverview();
    }
    // A hidden nonmodal QDialog contains a QScrollArea that can reveal its
    // children on first show. Inactive faders are retained for mode switching
    // but must never be accessible or mistaken for the active EQ curve.
    void syncGraphicEqEditorColumns() {
        const int n = validEqBands(eq_active_bands_);
        for (int i = 0; i < 31; ++i)
            eq_columns_[i]->setHidden(i >= n);
        eq_fader_container_->setFixedWidth(n * 39 + 6);
        eq_fader_container_->updateGeometry();
    }
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
    // Evenly sampled, log-spaced standard frequency centers. Keep the original
    // three-band starting point. This is GUI initialization only, NOT a DSP
    // filter change; manual frequencies are never silently rewritten.
    static int defaultDeqFrequency(int i, int count) {
        count = std::clamp(count, 1, 10);
        i = std::clamp(i, 0, count - 1);
        if (count == 1) return 1000;
        if (count == 2) return i == 0 ? 125 : 8000;
        if (count == 3) {
            static constexpr int three[] = {100, 1000, 8000};
            return three[i];
        }
        static constexpr int ten[] = {31, 62, 125, 250, 500, 1000,
                                      2000, 4000, 8000, 16000};
        return ten[std::clamp(qRound(i * 9.0 / (count - 1)), 0, 9)];
    }
    static DeqBand defaultDeqBand(int i, int count = 3) {
        DeqBand b;
        b.frequency = defaultDeqFrequency(i, count);
        return b;
    }
    void changeDeqBandCount(int count) {
        deq_active_count_ = std::clamp(count, 1, 10);
        for (int i = 0; i < deq_active_count_; ++i) {
            // Saved/manual bands remain unchanged. Only unconfigured defaults
            // follow the selected band count.
            if (!deq_user_modified_[i])
                deq_bands_[i].frequency = defaultDeqFrequency(i, deq_active_count_);
        }
        {
            const QSignalBlocker mainBlock(deq_count_), editorBlock(deq_editor_count_);
            deq_count_->setValue(deq_active_count_);
            deq_editor_count_->setValue(deq_active_count_);
        }
        refreshDeqBandSelector();
        updateDeqOverview();
        push();
    }
    void spreadDeqFrequencies() {
        // This is an explicit user action: replace only the frequencies of
        // active bands, retaining all the existing gain/dynamics parameters.
        for (int i = 0; i < deq_active_count_; ++i) {
            deq_bands_[i].frequency = defaultDeqFrequency(i, deq_active_count_);
            deq_user_modified_[i] = false;
        }
        refreshDeqBandSelector();
        updateDeqOverview();
        push();
    }
    void loadDeqSettings() {
        const QSignalBlocker b1(deq_enabled_), b2(deq_count_), b3(deq_editor_count_);
        deq_enabled_->setChecked(settings_.value("deq_enabled", false).toBool());
        deq_active_count_ = std::clamp(settings_.value("deq_band_count", 3).toInt(),1,10);
        deq_count_->setValue(deq_active_count_);
        deq_editor_count_->setValue(deq_active_count_);
        for (int i=0; i<10; ++i) {
            DeqBand d = defaultDeqBand(i, i < deq_active_count_ ? deq_active_count_ : 10);
            const QString k = "deq_b" + QString::number(i) + "_";
            // Older releases stored all 10 bands even when they were unused.
            // Treat legacy stored values as custom: never guess whether a
            // coincident 1 kHz band was intentional. The user can choose
            // Spread frequencies explicitly in the editor.
            deq_user_modified_[i] = settings_.contains(k + "user_modified")
                ? settings_.value(k + "user_modified").toBool()
                : settings_.contains(k + "freq");
            d.frequency = std::clamp(settings_.value(k+"freq",d.frequency).toInt(),20,20000);
            d.q = std::clamp(settings_.value(k+"q",100).toInt(),50,800);
            d.gain = std::clamp(settings_.value(k+"gain",0).toInt(),-1200,1200);
            d.threshold = std::clamp(settings_.value(k+"threshold",-240).toInt(),-800,0);
            d.attack = std::clamp(settings_.value(k+"attack",10).toInt(),1,100);
            d.release = std::clamp(settings_.value(k+"release",100).toInt(),10,500);
            d.type = std::clamp(settings_.value(k+"type",0).toInt(),0,2);
            deq_bands_[i] = d;
        }
        refreshDeqBandSelector();
        updateDeqOverview();
    }
    void refreshDeqBandSelector() {
        const int wanted = std::clamp(deq_band_select_->currentIndex(),0,deq_active_count_-1);
        const QSignalBlocker block(deq_band_select_);
        deq_band_select_->clear();
        for (int i=0; i<deq_active_count_; ++i)
            deq_band_select_->addItem(QString("Band %1").arg(i+1));
        deq_band_select_->setCurrentIndex(wanted);
        {
            // Rebuild the compact summary only when the band count or saved
            // settings change. Row selection and item text edits stay stable.
            const QSignalBlocker tableBlock(deq_table_);
            deq_table_->setRowCount(deq_active_count_);
            deq_table_->setFixedHeight(36 + deq_active_count_ * 25);
            for (int i = 0; i < deq_active_count_; ++i)
                updateDeqTableRow(i);
        }
        loadDeqEditorBand(wanted);
    }
    void updateDeqTableRow(int index) {
        if (!deq_table_ || index < 0 || index >= deq_active_count_) return;
        const QSignalBlocker guard(deq_table_);
        const DeqBand& b = deq_bands_[index];
        const QStringList values = {
            QString::number(index + 1), QString::number(b.frequency),
            QString::number(b.gain / 100.0, 'f', 2),
            QString::number(b.threshold / 10.0, 'f', 1)
        };
        for (int col = 0; col < 4; ++col) {
            auto* item = deq_table_->item(index, col);
            if (!item) {
                item = new QTableWidgetItem();
                deq_table_->setItem(index, col, item);
            }
            item->setText(values[col]);
            item->setTextAlignment(Qt::AlignCenter);
            if (col == 0) item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            item->setToolTip(col == 0 ? "Select this band to edit all parameters below" :
                "Double-click to edit this value; other band controls are below");
        }
    }
    void commitDeqTableItem(QTableWidgetItem* item) {
        if (!item || !deq_table_) return;
        const int index = item->row();
        const int col = item->column();
        if (index < 0 || index >= deq_active_count_ || col < 1 || col > 3) return;
        bool ok = false;
        const double value = item->text().trimmed().toDouble(&ok);
        if (ok && std::isfinite(value)) {
            DeqBand& band = deq_bands_[index];
            deq_user_modified_[index] = true;
            if (col == 1) band.frequency = std::clamp(qRound(value), 20, 20000);
            if (col == 2) band.gain = std::clamp(qRound(value * 100.0), -1200, 1200);
            if (col == 3) band.threshold = std::clamp(qRound(value * 10.0), -800, 0);
            // Keep the detailed editor, both previews and the live backend in
            // lockstep. QSignalBlocker avoids recursive itemChanged updates.
            updateDeqTableRow(index);
            if (index == deq_band_select_->currentIndex()) loadDeqEditorBand(index);
            updateDeqOverview();
            push();
        } else {
            // Reject invalid text without changing saved DSP values.
            updateDeqTableRow(index);
        }
    }
    void loadDeqEditorBand(int index) {
        if (index<0 || index>=deq_active_count_) return;
        const DeqBand& b = deq_bands_[index];
        const QSignalBlocker b1(deq_frequency_), b2(deq_q_), b3(deq_gain_),
            b4(deq_threshold_), b5(deq_attack_), b6(deq_release_), b7(deq_type_);
        deq_frequency_->setValue(b.frequency);
        deq_q_->setValue(b.q / 100.0);
        deq_gain_->setValue(b.gain / 100.0);
        deq_threshold_->setValue(b.threshold / 10.0);
        deq_attack_->setValue(b.attack);
        deq_release_->setValue(b.release);
        deq_type_->setCurrentIndex(b.type);
        if (deq_selected_label_)
            deq_selected_label_->setText(QString("Band %1 — %2 Hz").arg(index + 1).arg(b.frequency));
        if (deq_editor_graph_) deq_editor_graph_->setSelectedBand(index);
        if (deq_overview_) deq_overview_->setSelectedBand(index);
        if (deq_table_) {
            const QSignalBlocker guard(deq_table_);
            deq_table_->selectRow(index);
            deq_table_->scrollToItem(deq_table_->item(index, 0));
        }
    }
    void commitDeqEditorBand() {
        const int index = deq_band_select_->currentIndex();
        if (index<0 || index>=deq_active_count_) return;
        DeqBand& b = deq_bands_[index];
        deq_user_modified_[index] = true;
        b.frequency = deq_frequency_->value();
        b.q = qRound(deq_q_->value()*100.0);
        b.gain = qRound(deq_gain_->value()*100.0);
        b.threshold = qRound(deq_threshold_->value()*10.0);
        b.attack = deq_attack_->value();
        b.release = deq_release_->value();
        b.type = deq_type_->currentIndex();
        updateDeqTableRow(index);
        if (deq_selected_label_)
            deq_selected_label_->setText(QString("Band %1 — %2 Hz").arg(index + 1).arg(b.frequency));
        updateDeqOverview();
        push();
    }
    void updateDeqOverview() {
        if (deq_overview_)
            deq_overview_->setBands(deq_bands_, deq_active_count_, deq_enabled_->isChecked());
        if (deq_editor_graph_)
            deq_editor_graph_->setBands(deq_bands_, deq_active_count_, deq_enabled_->isChecked());
    }
    void resetDeq() {
        const QSignalBlocker a(deq_enabled_), b(deq_count_), c(deq_editor_count_);
        deq_enabled_->setChecked(false);
        deq_active_count_=3; deq_count_->setValue(3); deq_editor_count_->setValue(3);
        for (int i=0; i<10; ++i) {
            deq_user_modified_[i] = false;
            deq_bands_[i] = defaultDeqBand(i, i < 3 ? 3 : 10);
        }
        refreshDeqBandSelector(); updateDeqOverview();
    }
    void saveDeqPreset() {
        settings_.setValue("preset/deq_enabled",deq_enabled_->isChecked());
        settings_.setValue("preset/deq_band_count",deq_active_count_);
        for (int i=0;i<10;++i) {
            const QString k = "preset/deq_b"+QString::number(i)+"_";
            const auto& b=deq_bands_[i];
            settings_.setValue(k+"freq", b.frequency);
            settings_.setValue(k+"user_modified", deq_user_modified_[i]);
            settings_.setValue(k+"q",b.q);
            settings_.setValue(k+"gain",b.gain);
            settings_.setValue(k+"threshold",b.threshold);
            settings_.setValue(k+"attack",b.attack);
            settings_.setValue(k+"release",b.release);
            settings_.setValue(k+"type",b.type);
        }
    }
    void restoreDeqPreset() {
        const QSignalBlocker a(deq_enabled_), b(deq_count_), c(deq_editor_count_);
        deq_enabled_->setChecked(settings_.value("preset/deq_enabled",false).toBool());
        deq_active_count_=std::clamp(settings_.value("preset/deq_band_count",3).toInt(),1,10);
        deq_count_->setValue(deq_active_count_);
        deq_editor_count_->setValue(deq_active_count_);
        for (int i=0;i<10;++i) {
            const QString k = "preset/deq_b"+QString::number(i)+"_";
            DeqBand d=defaultDeqBand(i, i < deq_active_count_ ? deq_active_count_ : 10);
            deq_user_modified_[i] = settings_.contains(k + "user_modified")
                ? settings_.value(k + "user_modified").toBool()
                : settings_.contains(k + "freq");
            d.frequency=std::clamp(settings_.value(k+"freq",d.frequency).toInt(),20,20000);
            d.q=std::clamp(settings_.value(k+"q",100).toInt(),50,800);
            d.gain=std::clamp(settings_.value(k+"gain",0).toInt(),-1200,1200);
            d.threshold=std::clamp(settings_.value(k+"threshold",-240).toInt(),-800,0);
            d.attack=std::clamp(settings_.value(k+"attack",10).toInt(),1,100);
            d.release=std::clamp(settings_.value(k+"release",100).toInt(),10,500);
            d.type=std::clamp(settings_.value(k+"type",0).toInt(),0,2);
            deq_bands_[i]=d;
        }
        refreshDeqBandSelector(); updateDeqOverview();
    }
    void applyAnalogXSettings(bool enabled, int mode) {
        const QSignalBlocker a(analogx_enabled_), b(analogx_mode_);
        analogx_enabled_->setChecked(enabled);
        analogx_mode_->setCurrentIndex(std::clamp(mode, 0, 2));
    }
    void applyTubeSettings(bool enabled, int model, int drive, int mix) {
        const QSignalBlocker a(tube_enabled_), b(tube_model_), c(tube_drive_.slider), d(tube_mix_.slider);
        tube_enabled_->setChecked(enabled);
        tube_model_->setCurrentIndex(std::clamp(model, 0, 2));
        tube_drive_.slider->setValue(std::clamp(drive, 0, 100));
        tube_mix_.slider->setValue(std::clamp(mix, 0, 100));
        updateLabels();
    }
    void updateMultibandFrequencyLabels() {
        const int n = mb_band_count_->currentIndex() == 1 ? 5 : 3;
        // Labels identify the actual native DSP frequency regions, not EQ gains.
        static constexpr const char* kThreeNames[3] = {"Bass", "Mids", "Treble"};
        static constexpr const char* kFiveNames[5] = {"Sub", "Bass", "Mids", "Presence", "Treble"};
        for (int i = 0; i < n; ++i) {
            const int low = i == 0 ? 20 : mb_crossovers_[i-1].slider->value();
            const QString range = i == n-1
                ? QString("%1 Hz and above").arg(low)
                : QString("%1–%2 Hz").arg(low).arg(mb_crossovers_[i].slider->value());
            const QString name = QString::fromUtf8(n == 3 ? kThreeNames[i] : kFiveNames[i]);
            mb_band_tabs_->setTabText(i, name);
            mb_band_tabs_->setTabToolTip(i, range);
            mb_band_ranges_[i]->setText(name + " · " + range);
        }
    }
    void updateMultibandVisibility() {
        const int n = mb_band_count_->currentIndex() == 1 ? 5 : 3;
        for (int i = 0; i < 5; ++i) mb_band_tabs_->setTabVisible(i, i < n);
        if (mb_band_tabs_->currentIndex() >= n) mb_band_tabs_->setCurrentIndex(n - 1);
        for (int i = 0; i < 4; ++i) {
            // addSlider creates a QWidget for each QFormLayout row.
            QWidget* row = mb_crossovers_[i].slider->parentWidget();
            if (row) row->setVisible(i < n - 1);
        }
        updateMultibandFrequencyLabels();
    }
    void saveMultibandSettings(const QString& prefix) {
        settings_.setValue(prefix + "mb_enabled", mb_enabled_->isChecked());
        settings_.setValue(prefix + "mb_band_count", mb_band_count_->currentIndex() == 1 ? 5 : 3);
        for (int i=0; i<4; ++i)
            settings_.setValue(prefix + QString("mb_x%1_hz").arg(i), mb_crossovers_[i].slider->value());
        for (int i=0; i<5; ++i) {
            const auto key = prefix + QString("mb_b%1_").arg(i);
            const auto& b = mb_bands_[i];
            settings_.setValue(key + "enabled", b.enabled->isChecked());
            settings_.setValue(key + "threshold_db", b.threshold.slider->value());
            settings_.setValue(key + "ratio_tenths", b.ratio.slider->value());
            settings_.setValue(key + "knee_db", b.knee.slider->value());
            settings_.setValue(key + "makeup_db", b.gain.slider->value());
            settings_.setValue(key + "attack_ms", b.attack.slider->value());
            settings_.setValue(key + "release_ms", b.release.slider->value());
        }
    }
    void loadMultibandSettings(const QString& prefix) {
        const QSignalBlocker en(mb_enabled_), count(mb_band_count_);
        mb_enabled_->setChecked(settings_.value(prefix + "mb_enabled", false).toBool());
        const int bands = settings_.value(prefix + "mb_band_count", 3).toInt() == 5 ? 5 : 3;
        mb_band_count_->setCurrentIndex(bands == 5 ? 1 : 0);
        const std::array<int,4> defaults = bands == 5
            ? std::array<int,4>{120,500,4000,8000} : std::array<int,4>{200,4000,5000,8000};
        for (int i=0; i<4; ++i) {
            const QSignalBlocker block(mb_crossovers_[i].slider);
            mb_crossovers_[i].slider->setValue(std::clamp(settings_.value(prefix + QString("mb_x%1_hz").arg(i), defaults[i]).toInt(),80,15000));
        }
        for (int i=0; i<5; ++i) {
            const auto key = prefix + QString("mb_b%1_").arg(i);
            auto& b = mb_bands_[i];
            const QSignalBlocker block(b.enabled);
            b.enabled->setChecked(settings_.value(key + "enabled", true).toBool());
            for (const auto& item : std::initializer_list<std::pair<SliderRow,int>>{
                {b.threshold, std::clamp(settings_.value(key + "threshold_db", -18).toInt(), -48, 0)},
                {b.ratio, std::clamp(settings_.value(key + "ratio_tenths", 20).toInt(),10,100)},
                {b.knee, std::clamp(settings_.value(key + "knee_db", 6).toInt(),0,12)},
                {b.gain, std::clamp(settings_.value(key + "makeup_db", 0).toInt(),0,24)},
                {b.attack, std::clamp(settings_.value(key + "attack_ms", 10).toInt(),1,100)},
                {b.release, std::clamp(settings_.value(key + "release_ms", 100).toInt(),5,500)}}) {
                const QSignalBlocker lock(item.first.slider);
                item.first.slider->setValue(item.second);
            }
        }
        updateMultibandVisibility(); updateLabels();
    }
    void resetMultibandSettings() {
        const QSignalBlocker en(mb_enabled_), count(mb_band_count_);
        mb_enabled_->setChecked(false);
        mb_band_count_->setCurrentIndex(0);
        const std::array<int,4> defaults{200,4000,5000,8000};
        for (int i=0; i<4; ++i) {
            const QSignalBlocker lock(mb_crossovers_[i].slider);
            mb_crossovers_[i].slider->setValue(defaults[i]);
        }
        for (auto& b : mb_bands_) {
            const QSignalBlocker enBand(b.enabled);
            b.enabled->setChecked(true);
            for (const auto& item : std::initializer_list<std::pair<SliderRow,int>>{
                {b.threshold,-18}, {b.ratio,20}, {b.knee,6},
                {b.gain,0}, {b.attack,10}, {b.release,100}}) {
                const QSignalBlocker lock(item.first.slider);
                item.first.slider->setValue(item.second);
            }
        }
        updateMultibandVisibility(); updateLabels();
    }
    void restoreFetSettings(const QString& prefix) {
        const QSignalBlocker on(fet_enabled_);
        fet_enabled_->setChecked(settings_.value(prefix + "fet_enabled", false).toBool());
        for (const auto& item : std::initializer_list<std::pair<SliderRow,int>>{
            {fet_threshold_, std::clamp(settings_.value(prefix + "fet_threshold_db", -18).toInt(), -48, 0)},
            {fet_ratio_, std::clamp(settings_.value(prefix + "fet_ratio_tenths", 20).toInt(), 10, 100)},
            {fet_knee_, std::clamp(settings_.value(prefix + "fet_knee_db", 6).toInt(), 0, 12)},
            {fet_gain_, std::clamp(settings_.value(prefix + "fet_makeup_db", 0).toInt(), 0, 24)},
            {fet_attack_, std::clamp(settings_.value(prefix + "fet_attack_ms", 10).toInt(), 1, 100)},
            {fet_release_, std::clamp(settings_.value(prefix + "fet_release_ms", 100).toInt(), 5, 500)}}) {
            const QSignalBlocker block(item.first.slider);
            item.first.slider->setValue(item.second);
        }
        updateLabels();
    }
    void resetFetSettings() {
        const QSignalBlocker on(fet_enabled_);
        fet_enabled_->setChecked(false);
        for (const auto& item : std::initializer_list<std::pair<SliderRow,int>>{
            {fet_threshold_, -18}, {fet_ratio_, 20}, {fet_knee_, 6},
            {fet_gain_, 0}, {fet_attack_, 10}, {fet_release_, 100}}) {
            const QSignalBlocker block(item.first.slider);
            item.first.slider->setValue(item.second);
        }
        updateLabels();
    }
    void push() {
        persistCurrentSettings();
        publishToBackend();
    }
    void applySettings(bool master, bool bass, int bass_mode, int strength, int frequency,
                       bool clarity_on, int clarity_mode, int clarity_gain,
                       bool spectrum_on, int spectrum_frequency, int spectrum_exciter,
                       bool headphone_on, int headphone_quality,
                       bool diff_on, int diff_delay, bool diff_reverse, int diff_mix, int diff_lp,
                       bool field_on, int field_widen, int field_mid, int field_depth_step,
                       bool reverb_on, int reverb_room, int reverb_width, int reverb_damp, int reverb_wet, int reverb_dry) {
        // Loading a complete named preset or resetting to defaults explicitly
        // replaces the previous full-precision CLI values.
        settings_.remove("precision");
        replace_precision_on_push_ = true;
        // Suppress intermediate updates: don't send four DSP changes per reset.
        const QSignalBlocker block0(global_);
        const QSignalBlocker block1(enabled_);
        const QSignalBlocker blockBassMode(bass_mode_);
        const QSignalBlocker block2(strength_.slider);
        const QSignalBlocker block3(frequency_.slider);
        const QSignalBlocker block5(clarity_enabled_);
        const QSignalBlocker block6(clarity_mode_);
        const QSignalBlocker block7(clarity_gain_.slider);
        const QSignalBlocker block8(spectrum_enabled_);
        const QSignalBlocker block9(spectrum_frequency_.slider);
        const QSignalBlocker block10(spectrum_exciter_.slider);
        const QSignalBlocker block11(headphone_enabled_);
        const QSignalBlocker block12(headphone_quality_);
        const QSignalBlocker block13(diff_enabled_);
        const QSignalBlocker block14(diff_delay_.slider);
        const QSignalBlocker block15(diff_reverse_);
        const QSignalBlocker block16(diff_mix_.slider);
        const QSignalBlocker block17(diff_lowpass_.slider);
        const QSignalBlocker block18(field_enabled_);
        const QSignalBlocker block19(field_widen_.slider);
        const QSignalBlocker block20(field_mid_.slider);
        const QSignalBlocker block21(field_depth_.slider);
        const QSignalBlocker block22(reverb_enabled_);
        const QSignalBlocker block23(reverb_room_.slider);
        const QSignalBlocker block24(reverb_width_.slider);
        const QSignalBlocker block25(reverb_damp_.slider);
        const QSignalBlocker block26(reverb_wet_.slider);
        const QSignalBlocker block27(reverb_dry_.slider);
        global_->setChecked(master);
        enabled_->setChecked(bass);
        bass_mode_->setCurrentIndex(std::clamp(bass_mode, 0, 2));
        strength_.slider->setValue(std::clamp(strength, 5, 100));
        frequency_.slider->setValue(std::clamp(frequency, 15, 150));
        clarity_enabled_->setChecked(clarity_on);
        clarity_mode_->setCurrentIndex(std::clamp(clarity_mode, 0, 2));
        clarity_gain_.slider->setValue(std::clamp(clarity_gain, 0, 45));
        spectrum_enabled_->setChecked(spectrum_on);
        spectrum_frequency_.slider->setValue(std::clamp(spectrum_frequency, 22, 82));
        spectrum_exciter_.slider->setValue(std::clamp(spectrum_exciter, 0, 60));
        headphone_enabled_->setChecked(headphone_on);
        headphone_quality_->setCurrentIndex(std::clamp(headphone_quality, 0, 4));
        diff_enabled_->setChecked(diff_on);
        diff_delay_.slider->setValue(std::clamp(diff_delay, 1, 20));
        diff_reverse_->setChecked(diff_reverse);
        diff_mix_.slider->setValue(std::clamp(diff_mix, 0, 100));
        diff_lowpass_.slider->setValue(std::clamp(diff_lp, 0, 200));
        field_enabled_->setChecked(field_on);
        field_widen_.slider->setValue(std::clamp(field_widen, 0, 80));
        field_mid_.slider->setValue(std::clamp(field_mid, 100, 200));
        field_depth_.slider->setValue(std::clamp(field_depth_step, 0, 16));
        reverb_enabled_->setChecked(reverb_on);
        reverb_room_.slider->setValue(std::clamp(reverb_room, 0, 100));
        reverb_width_.slider->setValue(std::clamp(reverb_width, 0, 100));
        reverb_damp_.slider->setValue(std::clamp(reverb_damp, 0, 100));
        reverb_wet_.slider->setValue(std::clamp(reverb_wet, 0, 100));
        reverb_dry_.slider->setValue(std::clamp(reverb_dry, 0, 100));
        updateLabels();
        push();
    }
    void saveBassPreset() {
        for (const auto& item : std::initializer_list<std::pair<QString, int>>{
            {"bass_mono_enabled", mono_enabled_->isChecked()}, {"bass_mono_mode", mono_mode_->currentIndex()},
            {"bass_mono_strength_centi", mono_strength_.slider->value()*10},
            {"bass_mono_frequency_hz", mono_frequency_.slider->value()},
            {"psycho_enabled", psycho_enabled_->isChecked()}, {"psycho_cutoff_hz", psycho_cutoff_.slider->value()},
            {"psycho_intensity_percent", psycho_intensity_.slider->value()}, {"psycho_order", psycho_order_->currentIndex()+2},
            {"psycho_original_percent", psycho_original_.slider->value()},
            {"dynamic_enabled", dyn_enabled_->isChecked()}, {"dynamic_x_low", dyn_xlo_.slider->value()},
            {"dynamic_x_high", dyn_xhi_.slider->value()}, {"dynamic_y_low", dyn_ylo_.slider->value()},
            {"dynamic_y_high", dyn_yhi_.slider->value()},
            {"dynamic_side_low_percent", dyn_side_lo_.slider->value()},
            {"dynamic_side_high_percent", dyn_side_hi_.slider->value()},
            {"dynamic_strength_percent", dyn_strength_.slider->value()} }) {
            settings_.setValue("preset/" + item.first, item.second);
        }
    }
    void applyBassExtra(int mono, int monoMode, int monoStrength, int monoFreq,
                        int psycho, int cutoff, int intensity, int order, int original,
                        int dynamic, int xlo, int xhi, int ylo, int yhi, int sideLo, int sideHi, int strength) {
        const QSignalBlocker blocks[] = {
            QSignalBlocker(mono_enabled_), QSignalBlocker(mono_mode_),
            QSignalBlocker(mono_strength_.slider), QSignalBlocker(mono_frequency_.slider),
            QSignalBlocker(psycho_enabled_), QSignalBlocker(psycho_cutoff_.slider),
            QSignalBlocker(psycho_intensity_.slider), QSignalBlocker(psycho_order_),
            QSignalBlocker(psycho_original_.slider), QSignalBlocker(dyn_enabled_),
            QSignalBlocker(dyn_xlo_.slider), QSignalBlocker(dyn_xhi_.slider),
            QSignalBlocker(dyn_ylo_.slider), QSignalBlocker(dyn_yhi_.slider),
            QSignalBlocker(dyn_side_lo_.slider), QSignalBlocker(dyn_side_hi_.slider),
            QSignalBlocker(dyn_strength_.slider)
        };
        mono_enabled_->setChecked(mono != 0);
        mono_mode_->setCurrentIndex(std::clamp(monoMode, 0, 2));
        mono_strength_.slider->setValue(std::clamp(monoStrength, 50, 1000)/10);
        mono_frequency_.slider->setValue(std::clamp(monoFreq, 15, 150));
        psycho_enabled_->setChecked(psycho != 0);
        psycho_cutoff_.slider->setValue(std::clamp(cutoff, 60, 150));
        psycho_intensity_.slider->setValue(std::clamp(intensity, 0, 100));
        psycho_order_->setCurrentIndex(std::clamp(order, 2, 5)-2);
        psycho_original_.slider->setValue(std::clamp(original, 0, 100));
        dyn_enabled_->setChecked(dynamic != 0);
        dyn_xlo_.slider->setValue(std::clamp(xlo, 0, 2400));
        dyn_xhi_.slider->setValue(std::clamp(xhi, 0, 12000));
        dyn_ylo_.slider->setValue(std::clamp(ylo, 0, 200));
        dyn_yhi_.slider->setValue(std::clamp(yhi, 0, 300));
        dyn_side_lo_.slider->setValue(std::clamp(sideLo, 0, 100));
        dyn_side_hi_.slider->setValue(std::clamp(sideHi, 0, 100));
        dyn_strength_.slider->setValue(std::clamp(strength, 0, 100));
        updateLabels();
        push();
    }
    void resetNewBassControls() {
        applyBassExtra(0,0,200,65,0,80,30,3,100,0,100,6000,100,150,50,50,0);
    }
    void restoreBassPreset() {
        auto get = [this](const char* key,int defaultValue){return settings_.value(QString("preset/")+key,defaultValue).toInt();};
        applyBassExtra(get("bass_mono_enabled",0),get("bass_mono_mode",0),get("bass_mono_strength_centi",200),
                       get("bass_mono_frequency_hz",65),get("psycho_enabled",0),get("psycho_cutoff_hz",80),
                       get("psycho_intensity_percent",30),get("psycho_order",3),get("psycho_original_percent",100),
                       get("dynamic_enabled",0),get("dynamic_x_low",100),get("dynamic_x_high",6000),
                       get("dynamic_y_low",100),get("dynamic_y_high",150),get("dynamic_side_low_percent",50),
                       get("dynamic_side_high_percent",50),get("dynamic_strength_percent",0));
    }
    // Output fields are separate from the older seven-effect preset schema;
    // restore them atomically from a logical UI operation, not one slider at a time.
    void applyOutputSettings(int threshold, int gain, int pan, bool agc, int agcStrength,
                             int agcMaxGain, int agcThreshold, bool lufs,
                             int lufsTarget, int lufsMaxGain, int speed) {
        const QSignalBlocker b0(limiter_threshold_.slider), b1(limiter_gain_.slider), b2(limiter_pan_.slider);
        const QSignalBlocker b3(agc_enabled_), b4(agc_strength_.slider), b5(agc_max_gain_.slider);
        const QSignalBlocker b6(agc_threshold_.slider), b7(lufs_enabled_), b8(lufs_target_.slider);
        const QSignalBlocker b9(lufs_gain_.slider), b10(lufs_speed_);
        limiter_threshold_.slider->setValue(std::clamp(threshold, 300, 1000));
        limiter_gain_.slider->setValue(std::clamp(gain, 1, 200));
        limiter_pan_.slider->setValue(std::clamp(pan, -100, 100));
        agc_enabled_->setChecked(agc);
        agc_strength_.slider->setValue(std::clamp(agcStrength, 50, 300));
        agc_max_gain_.slider->setValue(std::clamp(agcMaxGain, 100, 1000));
        agc_threshold_.slider->setValue(std::clamp(agcThreshold, 300, 1000));
        lufs_enabled_->setChecked(lufs);
        lufs_target_.slider->setValue(std::clamp(lufsTarget, -240, -80));
        lufs_gain_.slider->setValue(std::clamp(lufsMaxGain, 0, 120));
        lufs_speed_->setCurrentIndex(std::clamp(speed, 0, 2));
        updateLabels();
        push();
    }
    void saveOutputPreset() {
        settings_.setValue("preset/limiter_threshold_permille", limiter_threshold_.slider->value());
        settings_.setValue("preset/limiter_output_percent", limiter_gain_.slider->value());
        settings_.setValue("preset/limiter_pan_percent", limiter_pan_.slider->value());
        settings_.setValue("preset/agc_enabled", agc_enabled_->isChecked());
        settings_.setValue("preset/agc_strength_centi", agc_strength_.slider->value());
        settings_.setValue("preset/agc_max_gain_centi", agc_max_gain_.slider->value());
        settings_.setValue("preset/agc_threshold_permille", agc_threshold_.slider->value());
        settings_.setValue("preset/lufs_enabled", lufs_enabled_->isChecked());
        settings_.setValue("preset/lufs_target_tenths", lufs_target_.slider->value());
        settings_.setValue("preset/lufs_max_gain_tenths", lufs_gain_.slider->value());
        settings_.setValue("preset/lufs_speed", lufs_speed_->currentIndex());
    }
    void restoreOutputPreset() {
        applyOutputSettings(settings_.value("preset/limiter_threshold_permille", 500).toInt(),
            settings_.value("preset/limiter_output_percent", 100).toInt(),
            settings_.value("preset/limiter_pan_percent", 0).toInt(),
            settings_.value("preset/agc_enabled", false).toBool(),
            settings_.value("preset/agc_strength_centi", 100).toInt(),
            settings_.value("preset/agc_max_gain_centi", 300).toInt(),
            settings_.value("preset/agc_threshold_permille", 900).toInt(),
            settings_.value("preset/lufs_enabled", false).toBool(),
            settings_.value("preset/lufs_target_tenths", -140).toInt(),
            settings_.value("preset/lufs_max_gain_tenths", 60).toInt(),
            settings_.value("preset/lufs_speed", 1).toInt());
    }
    void resetOutputControls() {
        applyOutputSettings(500, 100, 0, false, 100, 300, 900, false, -140, 60, 1);
    }
    // Compare the widgets before/after a backend read. Signal blockers are
    // intentional: reflecting CLI changes must not republish a stale GUI
    // snapshot over them. We then persist the resulting live settings without
    // sending a second change to the DSP. This is GUI-thread-only code.
    // Checkpoint 6: implementation-only synchronization modules.
#include "sync/gui-settings-bridge.inc"
#include "sync/gui-backend-bridge.inc"
    QSettings settings_;
    viperbass::AudioActivityTracker audio_activity_;
    QSystemTrayIcon* tray_icon_{};
    QAction* tray_processing_action_{};
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
    QApplication app(argc, argv);
    app.setApplicationName("Viper4Linux");
    app.setWindowIcon(QIcon(":/viper4linux/blue.png"));
    // A tray-only launch has no visible top-level window. Explicit Quit GUI
    // and closeEvent remain the only reasons for the GUI to terminate.
    app.setQuitOnLastWindowClosed(false);
    ViperWindow window;
    window.showAtStartup();
    return app.exec();
}
