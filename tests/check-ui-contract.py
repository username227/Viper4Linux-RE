#!/usr/bin/env python3
"""Read-only source checks for the GUI-only redesign (not a Qt runtime test)."""
from pathlib import Path
import re
from gui_source_contract import read_gui_sources
root = Path(__file__).resolve().parents[1]
source = read_gui_sources(root)
overview = (root / "src/widgets/eq-overviews.h").read_text()
required = {
    'Bass': 'enabled_',
    'Clarity': 'clarity_enabled_',
    'Spectrum': 'spectrum_enabled_',
    'Headphone Surround+': 'headphone_enabled_',
    'Differential Surround': 'diff_enabled_',
    'Field Surround': 'field_enabled_',
    'Reverberation': 'reverb_enabled_',
}
for title, variable in required.items():
    assert title in source, title
    field = variable.rstrip('_')
    assert re.search(r'\b(?:out\.)?' + re.escape(field) + r'_?\s*=\s*new EffectSwitch\(', source), variable
    assert f'connect({variable}, &QCheckBox::toggled' in source, variable
assert 'addEffectsTab("Bass")' in source
assert 'addEffectsTab("EQ & Tone")' in source
assert 'addEffectsTab("Spatial")' in source
assert 'addEffectsTab("Output")' in source
assert 'out.limiter_indicator->setChecked(true)' in source
assert 'out.limiter_indicator->setEnabled(false)' in source
assert 'out.limiter_threshold = addSlider(' in source
assert 'out.agc_enabled = new EffectSwitch(' in source
assert 'out.lufs_enabled = new EffectSwitch(' in source
assert 'output_warning_' in source
assert 'saveOutputPreset();' in source
assert 'restoreOutputPreset();' in source
assert 'QString::fromUtf8(label)' in source
assert 'ui/active_tab_name' in source
positions = [source.index('addEffectsTab("'+name+'")') for name in ('Output','Bass','EQ & Tone','Spatial','Dynamics','Sound Correction')]
assert positions == sorted(positions), 'Six-tab order changed'
assert 'Advanced: Adapter Input Trim' not in source
assert 'Gain / Headroom' not in source
assert 'addEffectPanel(outputLayout, gain_group' not in source
assert 'input_percent_' not in source
assert 'input_permille' not in source
assert '#include "backend/processor.inc"' in (root / "src/viper-ladspa.cpp").read_text()
processor = (root / 'src/backend/processor.inc').read_text()
assert '#include "processor-run.inc"' in processor, 'Real-time callback include missing'
backend = processor + "\n" + (root / 'src/backend/processor-run.inc').read_text()
assert '* safe_attenuation' not in backend
assert 'InputScale' not in backend
assert 'input_permille' not in backend
assert 'const float l = in_l && std::isfinite(in_l[ix]) ? in_l[ix] : 0.0f;' in backend
assert 'const float r = in_r && std::isfinite(in_r[ix]) ? in_r[ix] : 0.0f;' in backend
routing = (Path(__file__).resolve().parents[1] / 'routing-config/filter-chain.conf.d/99-viper-smart.conf').read_text()
assert 'Input Scale' not in routing
assert 'addEffectPanel(dynamicsLayout, dynamicsFet.group, "fet", fet_enabled_)' in source
assert 'addEffectPanel(dynamicsLayout, dynamicsMb.group, "multiband", mb_enabled_)' in source
assert 'addEffectPanel(correctionLayout, correctionConvolver.group, "convolver", convolver_enabled_)' in source
assert 'Load' not in source or 'No impulse response' in source
assert 'if (initialTabName == "Enhancements") initialTabName = "EQ & Tone";' in source
assert 'if (initialTabName == "Surround") initialTabName = "Spatial";' in source
for title,control in [('ViPER Bass Mono','mono_enabled_'),('Psychoacoustic Bass','psycho_enabled_'),('Dynamic System','dyn_enabled_')]:
    assert title in source
    field = control.rstrip('_')
    assert re.search(r'\b(?:out\.)?' + re.escape(field) + r'_?\s*=\s*new EffectSwitch\(', source), control
    assert f'connect({control}, &QCheckBox::toggled' in source
for key in ['bass', 'bass_mono', 'psycho_bass', 'dynamic_bass', 'clarity', 'spectrum', 'headphone', 'differential', 'field', 'reverb', 'convolver', 'limiter', 'agc', 'lufs']:
    assert f'"{key}"' in source, key
assert 'ui/expanded/' in source
assert 'setVisible(expanded)' in source
assert 'QSlider::groove:horizontal' in source
# This lambda is the ONLY code called when a card is opened or closed. It
# must never publish DSP control changes or modify the enable switch.
handler = re.search(r'connect\(expand, &QToolButton::toggled, this, \[this, group, expand, key\]\(bool expanded\) \{(.*?)\n        \}\);', source, re.S)
assert handler, 'No independently wired expand/collapse callback'
body = "\n".join(line.split("//", 1)[0] for line in handler.group(1).splitlines())
for forbidden in ['push(', 'publishToBackend(', 'setChecked(', 'enable->', 'persistCurrentSettings(']:
    assert forbidden not in body, forbidden
for symbol in ['field_room', 'reverb_enabled_', 'diff_enabled_', 'pollBackend()', 'pending_edit_', 'readBackend()']:
    if symbol != 'field_room':
        assert symbol in source, symbol
print('PASS: native Convolver GUI contract + six tabs, independent collapse, settings, GUI reconnection')

# Live IRS library, no-restart DSP handoff and compact Help UI.
assert 'QStandardPaths::GenericDataLocation' in source
assert '"/viper4linux/IRS"' in source
assert 'viper_library_import::copyOrSelect(selectedFile, irsDirectory())' in source
assert 'out.convolver_library = new IrsComboBox' in source
assert 'convolver_request_serial.fetch_add(1' in source
assert 'void showHelp()' in source
assert 'QTextBrowser' in source
assert 'stack->addWidget(descriptionLabel)' not in source
assert 'Restart backend to apply it' not in source
assert 'The neutral test profile verifies loading' not in source
assert 'correction profile designed for your specific headphone model' in source
assert 'this project is a proof of concept' not in source
assert 'std::thread' in backend
assert 'std::try_to_lock' in backend
assert 'ir_worker_loop()' in backend
assert 'wet.UnloadConvolverKernel()' in backend
assert 'ir_transition.store(true' in backend
assert 'run_dry_while_loading' in backend
print('PASS: live Convolver IPC, IRS library/import, Help dialog source checks')

assert 'QFileDialog::getOpenFileNames(' in source
assert 'QFileSystemWatcher::directoryChanged' in source
assert 'convolver_library_->popupIsOpen()' in source
assert 'if (irs_library_dirty_) refreshIrsLibrary();' in source
assert 'scanTimer' not in source
assert 'viper_ir::resample_to(wave, rate)' in backend
print('PASS: multiselect IRS import, filesystem-notified stable popup, offline live IR resampling')

# Very long IRS filenames must never make the entire Convolver form wider than
# the available window or push its slider and Import/Reload buttons off-screen.
# These checks run without Qt installed; the actual native widget geometry
# still needs to be tested on the target desktop.
for token in [
    'setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon)',
    'setMinimumContentsLength(12)',
    'setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed)',
    'view()->setTextElideMode(Qt::ElideMiddle)',
    'fileRow->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred)',
    'conv_form->setRowWrapPolicy(QFormLayout::WrapLongRows)',
    'out.convolver_full_name->setReadOnly(true)',
    'out.convolver_full_name->setWordWrapMode(QTextOption::WrapAnywhere)',
    'out.convolver_full_name->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff)',
    'out.convolver_full_name->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed)',
    'convolver_library_->setItemData(index, entry.first, Qt::ToolTipRole)',
    'void updateIrsName(const QString& path)',
    'convolver_full_name_->setPlainText("Selected IRS: " + name)',
    'updateIrsName(path); // even if the popup is still open and frozen',
]:
    assert token in source, 'Missing responsive IRS GUI feature: ' + token
assert 'convolver_library_->setFixedWidth(' not in source
assert 'convolver_library_->setMinimumWidth(300)' not in source
print('PASS: responsive IRS selector, wrap-anywhere full name, item tooltips, flexible slider row')

# Native EQ uses a single permanent widget bank; one selected layout visible.
assert 'for (int i = 0; i < 31; ++i)' in source
assert 'eq_columns_[i]->setVisible(i < n);' in source
assert 'eq_fader_container_->setFixedWidth(n * 39 + 6);' in source
assert 'QSlider::groove:vertical' in source
assert 'widgets.faderContainer->setObjectName("viperEqFaders")' in source
assert 'connect(eq_mode_, QOverload<int>::of(&QComboBox::currentIndexChanged)' in source
assert 'widgets.faderContainer->setFixedSize(31 * 39 + 6, 235)' in source
assert 'eq_changed.notify(c->eq_request_serial);' in source
assert 'wet.ApplyEqualizer(p);' in backend
assert 'if (changing_layout)' in backend
assert 'std::lock_guard<std::mutex> guard(wet_mutex)' in backend
print('PASS: one reusable visible EQ fader bank, 4 modes, live non-RT coefficient updates')

# Compact EQ: two responsive selectors above a log-frequency smooth curve;
# clicking the curve opens the separate persistent fader editor.
for token in [
    'class EqOverview final : public QWidget',
    'QPainterPath response(points.front());',
    'response.cubicTo(c1, c2, b);',
    'viper_eq::frequencies(static_cast<int>(levels_.size()))',
    'eq_overview_->openEditor = [this]',
    'eq_editor_->show();',
    'out.eq_mode->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed)',
    'out.eq_preset->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed)',
    'eq_fader_row->setSpacing(3);',
    'eq_columns_[i]->setVisible(i < n);',
    'eq_preset_->addItem("Custom", "custom")',
    'void saveUserEqPreset()',
    'void deleteUserEqPreset()',
    'void updateEqOverview()',
    'settings_.setValue("eq/selected_preset", eq_selected_preset_)',
]:
    assert token in source, 'Missing compact EQ feature: ' + token
assert 'eq_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded)' in source
assert 'QTimer' in source  # Existing settings debounce; no new EQ polling timers
assert 'const qreal zeroY = plot.center().y()' in source
assert 'migrateEqPrecision();' in source
assert 'vals = item.levels(eq_active_bands_)' in source
assert 'fader->setRange(-1200, 1200)' in source
print('PASS: compact smooth EQ response, click editor, presets and responsive controls')

assert source.index('eq_selected_preset_ = settings_.value("eq/selected_preset"') < source.index('populateEqPresets();'), 'Startup erased persisted named preset'
print('PASS: EQ selection restored before dropdown population')

# Tube simulator is a native, independent, non-RT-updated effect.
for token in [
    'addEffectsTab("Sound Correction")',
    'setWindowTitle("Viper4Linux")',
    'auto* title = new QLabel("Viper4Linux", central)',
    'out.tube_enabled = new EffectSwitch(',
    'out.tube_model->addItems({"12AX7 (High Gain)", "12AU7 (Low Gain)", "6N1P (Medium Gain)"})',
    'addEffectPanel(effectLayout, eqToneTube.tubeGroup, "tube", tube_enabled_)',
    '"tube_enabled", tube_enabled_, 0',
    'tube_changed.set(c->tube_enabled, tube_enabled_->isChecked() ? 1 : 0)',
    'void applyTubeSettings(bool enabled, int model, int drive, int mix)',
]: assert token in source, token
for token in [
    'wet.ApplyTubeSimulator(p);',
    'void apply_tube_from_controls()',
    'controls->tube_request_serial',
    'controls->tube_applied_serial',
    'std::lock_guard<std::mutex> lock(wet_mutex)',
]: assert token in backend, token
assert 'Proof of concept · Native ViPERDSP · PipeWire smart routing' not in source
assert 'Closing the GUI doesn\'t stop audio or affect routing.' not in source
for token in (
    'out.analogx_enabled = new EffectSwitch(',
    'out.analogx_mode->addItems({"Mild", "Medium", "Strong"})',
    'addEffectPanel(effectLayout, eqToneTube.analogxGroup, "analogx", analogx_enabled_)',
    '"analogx_enabled", analogx_enabled_, 0',
    'settings_.setValue("preset/analogx_enabled", analogx_enabled_->isChecked())',
    'analogx_changed.set(c->analogx_enabled, analogx_enabled_->isChecked() ? 1 : 0)',
    'applyAnalogXSettings(false, 0)',
    'applyAnalogXSettings(settings_.value("preset/analogx_enabled", false).toBool()',
    'wet.ApplyAnalogX(p);',
    'controls->analogx_request_serial',
    'controls->analogx_applied_serial',
    'kind_ == "analogx"',
):
    assert token in ((Path(__file__).resolve().parents[1] / 'src/backend/processor.inc').read_text() if token.startswith(('wet.', 'controls->analogx_request_serial', 'controls->analogx_applied_serial')) else source), token
assert 'viper_worker::applyPending(controls->tube_request_serial' in backend
assert 'viper_worker::applyPending(controls->analogx_request_serial' in backend
print('PASS: Tube Simulator, native AnalogX and simplified Viper4Linux GUI controls')
for token in ('imager_enabled_', 'imager_low_', 'imager_mid_', 'imager_high_',
              'imager_low_xover_', 'imager_high_xover_', 'preset/imager_enabled',
              'imager_changed.set(c->imager_enabled', 'addEffectPanel(spatialLayout, spatialPanels.imagerGroup, "imager", imager_enabled_)'):
    assert token in source, f'Stereo Imager missing {token}'
assert 'viper_worker::applyPending(controls->imager_request_serial' in backend
assert 'wet.ApplyStereoImager(p);' in backend
assert 'imager_request_serial' in backend
print('PASS: Stereo Imager independent native three-band UI, presets and worker IPC')

# Every active effect card has a distinct, font-independent vector glyph.
import re as _re
card_keys = _re.findall(r'addEffectPanel\([^;]+?,\s*"([a-z_]+)"\s*,', source)
glyph_code = source.split('class EffectGlyph final : public QWidget {', 1)[1].split('class EffectSwitch final', 1)[0]
assert len(card_keys) == 24, card_keys
assert len(card_keys) == len(set(card_keys)), card_keys
for key in card_keys:
    assert f'kind_ == "{key}"' in glyph_code, 'Missing vector icon for ' + key
assert 'new EffectGlyph(key, panel)' in source
assert 'QString::fromUtf8("◉")' not in source
assert 'setAttribute(Qt::WA_TransparentForMouseEvents)' in glyph_code
# Qt6 forbids findChildren<EffectGlyph*>() without Q_OBJECT. Use the Qt
# widget hierarchy and a checked downcast, preserving theme color updates.
assert 'findChildren<EffectGlyph*>()' not in source
assert 'for (auto* widget : window->findChildren<QWidget*>())' in source
assert 'dynamic_cast<EffectGlyph*>(widget)' in source
print('PASS: 24 unique monochrome effect glyphs, full icon coverage, no font-dependent fallback')

# Dynamic EQ: a separate editor and static preview, native 10-band backend.
for token in [
 'class DynamicEqOverview final',
 'setObjectName("viperDynamicEqOverview")',
 'deq_overview_->openEditor',
 'deq_editor_->show()',
 'out.deq_count->setRange(1, 10)',
 'result.frequency->setRange(20,20000)',
 'result.gain->setRange(-12,12)',
 'result.threshold->setRange(-80,0)',
 'result.attack->setRange(1,100)',
 'result.release->setRange(10,500)',
 'void commitDeqEditorBand()',
 'void restoreDeqPreset()',
 'void saveDeqPreset()',
 'addEffectPanel(effectLayout, eqToneEq.deqGroup, "dynamic_eq", deq_enabled_)',
 'tabName == "EQ & Tone" ? "EQ && Tone" : tabName',
]:
    assert token in source, "Missing Dynamic EQ UI: " + token
assert source.index('addEffectPanel(effectLayout, eqToneEq.eqGroup') < source.index('addEffectPanel(effectLayout, eqToneEq.deqGroup') < source.index('addEffectPanel(effectLayout, eqToneClarity.clarityGroup')
assert 'Dynamic EQ is not implemented yet.' not in source
assert 'QTimer' not in overview[overview.index('class DynamicEqOverview final'):], 'Static preview must not have polling'
assert 'wet.ApplyDynamicEq(' in backend
assert 'deq_request_serial' in backend
assert 'apply_dynamic_eq_from_controls();' in backend
assert 'std::lock_guard<std::mutex> guard(wet_mutex);' in backend
print('PASS: Dynamic EQ static preview, independent click editor, native filter handoff, tab ampersand')

# Qt resources, enlarged two-row header, and application icon identity.
from pathlib import Path as _BrandPath
brand_root = _BrandPath(__file__).resolve().parent.parent
brand_gui = read_gui_sources(brand_root)
brand_cmake = (brand_root / "CMakeLists.txt").read_text()
brand_qrc = (brand_root / "resources/viper4linux-icons.qrc").read_text()
assert 'resources/viper4linux-icons.qrc' in brand_cmake
assert 'PROPERTIES AUTORCC ON' in brand_cmake
assert 'app.setWindowIcon(QIcon(":/viper4linux/blue.png"))' in brand_gui
# Suppress the duplicate native decoration icon locally, not the app icon.
assert 'setWindowIcon(QIcon(titleBarIcon))' in brand_gui
assert 'titleBarIcon.fill(Qt::transparent)' in brand_gui
# The Audio Output remains directly under the program name, not the icon.
assert '#include <QGridLayout>' in brand_gui
assert 'headerGrid->addWidget(header_icon_, 0, 0, 2, 1, Qt::AlignVCenter)' in brand_gui
assert 'headerGrid->addWidget(title, 0, 1)' in brand_gui
assert 'headerGrid->addWidget(audio_output_label_, 1, 1, 1, 4)' in brand_gui
assert 'headerGrid->setColumnStretch(1, 1)' in brand_gui
assert 'layout->addWidget(audio_output_label_)' not in brand_gui
assert 'header_icon_->setFixedSize(58, 58)' in brand_gui
# The main window owns the label; theme painting sets its pixmap through
# the helper's headerIcon parameter. Both sides of the connection must survive.
assert 'header_icon_ = new QLabel(central);' in brand_gui
assert 'tabs_, header_icon_, status_' in brand_gui
assert 'headerIcon->setPixmap(logo.scaled(56,56,Qt::KeepAspectRatio,Qt::SmoothTransformation))' in brand_gui
assert 'classic ? ":/viper4linux/blue.png" : ":/viper4linux/teal.png"' in brand_gui
assert 'viper4linux-teal.png' in brand_qrc and 'viper4linux-blue.png' in brand_qrc
for palette in ('teal','blue'):
    png=(brand_root / f'resources/viper4linux-{palette}.png').read_bytes()
    assert png[:8] == bytes.fromhex('89504e470d0a1a0a') and png[25] == 6, palette+' must have RGBA transparency'
# Preserve the original Graphic EQ click handler; only Dynamic EQ is selectable.
normal = overview.split('class EqOverview final')[1].split('class DynamicEqOverview final')[0]
dynamic = overview.split('class DynamicEqOverview final')[1]
assert 'if (editor_mode_ && selectBand)' not in normal
assert 'if (nearest >= 0 && (editor_mode_ || distance <= 19.0))' in dynamic
assert 'if (selectBand) selectBand(nearest);' in dynamic
assert 'if (!editor_mode_ && openEditor) openEditor();' in dynamic
assert 'selectBand(nearest)' in dynamic
assert 'deq_table_->setFixedHeight(36 + deq_active_count_ * 25)' in brand_gui
backend = processor + "\n" + (brand_root / 'src/backend/processor-run.inc').read_text()
assert 'if (!enabled && !last_deq_enabled) return;' in backend
print('PASS: two-row aligned Audio Output, enlarged theme logo, blue application icon, compact Dynamic EQ editor, disabled-edit bypass fix')

# Native Cure Crossfeed: independent switch, three presets, saved state and IPC.
for token in (
    'out.crossfeed_enabled = new EffectSwitch(',
    'out.crossfeed_mode->addItems({"Mild", "Medium", "Strong"})',
    'addEffectPanel(spatialLayout, spatialPanels.crossfeedGroup, "crossfeed", crossfeed_enabled_)',
    '"crossfeed_enabled", crossfeed_enabled_, 0',
    'settings_.setValue("preset/crossfeed_enabled", crossfeed_enabled_->isChecked())',
    'crossfeed_changed.set(c->crossfeed_enabled, crossfeed_enabled_->isChecked() ? 1 : 0)',
    'crossfeed_changed.notify(c->crossfeed_request_serial);',
    'controls->crossfeed_applied_serial',
    'wet.ApplyCure(p);',
):
    if token in source: continue
    assert token in (Path(__file__).resolve().parents[1] / 'src/backend/processor.inc').read_text(), token
print('PASS: native Crossfeed UI and presets, no substitute DSP')

# DDC: dedicated library, no disposable test profile in package.
for token in [
    '"/viper4linux/VDC"',
    'out.ddc_library = new IrsComboBox(',
    'QFileSystemWatcher::directoryChanged',
    'void refreshDdcLibrary()',
    'viper_library_import::copyOrSelect(selected, ddcDirectory())',
    'selectDdcProfile(destination);',
]:
    assert token in source, token
assert 'tests/fixtures/neutral-test.vdc' not in source
print('PASS: dedicated VDC profile library, import, refresh and selection')

# Sound Correction tab keeps the independent effect controls, without a
# redundant introductory Device correction notice taking up vertical space.
assert 'addTabNotice(correctionLayout, "Device correction"' not in source
assert 'addEffectPanel(correctionLayout, correctionDdc.group, "ddc", ddc_enabled_)' in source
assert 'addEffectPanel(correctionLayout, correctionSpeaker.group, "speaker_correction", speaker_enabled_)' in source
print('PASS: compact Sound Correction tab without redundant notice')

# CLI-to-GUI readback: worker-managed effect switches must not trigger a
# stale GUI push while polling. Persist only actual readback changes, not every
# 500 ms tick (which would starve QSettings' 300 ms write timer).
# Include precise native Dynamic System low-X: a CLI update below 120 Hz may
# change the saved native value without moving the clamped GUI slider.
for token in (
    'const auto before = liveUiSnapshot();',
    'const QSignalBlocker blockDdc(ddc_enabled_), blockSpeaker(speaker_enabled_);',
    'const QSignalBlocker blockDeq(deq_enabled_);',
    'deq_enabled_->setChecked(c->deq_enabled.load() != 0);',
    'const int beforeNativeLowX = dyn_xlo_native_;',
    'dyn_xlo_native_ = std::clamp(c->dynamic_x_low.load(), 0, 2400);',
    'if (before != liveUiSnapshot() || beforeNativeLowX != dyn_xlo_native_) persistCurrentSettings();',
):
    assert token in source, token
assert 'if (before != liveUiSnapshot()) publishToBackend();' not in source
print('PASS: CLI readback blocks feedback and persists GUI-observed backend edits')

# A silent but connected backend uses distinct, theme-readable blue status text.
bridge = (root / "src/sync/gui-backend-bridge.inc").read_text()
assert 'Connected — no audio signal (DSP ready)' in bridge
assert 'color: #75B9F4;' in bridge and 'color: #2B75B8;' in bridge
assert 'palette().color(QPalette::Window).lightness() < 130' in bridge
print('PASS: distinct blue no-signal status in light and dark themes')
