// EQ & Tone tab widget construction. Creation is divided into three factories
// to preserve the established order relative to EQ editor setup and other tabs.
// All signal connections, state, preset actions, and DSP transport stay in ViperWindow.
#pragma once
#include "widgets/effect-widgets.h"
#include "widgets/eq-overviews.h"
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSizePolicy>
#include <QSpinBox>
#include <QWidget>

namespace viper_panels {

struct EqAndDynamicPanels {
    QGroupBox* eqGroup{};
    QGroupBox* deqGroup{};
    EffectSwitch* eq_enabled{};
    QComboBox* eq_mode{};
    QComboBox* eq_preset{};
    EqOverview* eq_overview{};
    EffectSwitch* deq_enabled{};
    QSpinBox* deq_count{};
    DynamicEqOverview* deq_overview{};
    QPushButton* eqSave{};
    QPushButton* eqDelete{};
};
inline EqAndDynamicPanels createEqAndDynamicPanels(QWidget* parent) {
    EqAndDynamicPanels out;
        // The two selectors share a flexible row; no selector grows to the
        // length of its longest label or forces the containing tab wider.
        auto* eq_group = new QGroupBox("Graphic Equalizer", parent);
        auto* eq_form = new QFormLayout(eq_group);
        eq_form->setRowWrapPolicy(QFormLayout::WrapLongRows);
        out.eq_enabled = new EffectSwitch(eq_group);
        eq_form->addRow("Enable native EQ", out.eq_enabled);
        auto* eq_controls = new QWidget(eq_group);
        auto* eq_row = new QHBoxLayout(eq_controls);
        eq_row->setContentsMargins(0, 0, 0, 0);
        eq_row->setSpacing(6);
        eq_row->addWidget(new QLabel("Bands", eq_controls));
        out.eq_mode = new QComboBox(eq_controls);
        out.eq_mode->setObjectName("viperEqBandCount");
        for (int count : {10, 15, 25, 31})
            out.eq_mode->addItem(QString::number(count), count);
        out.eq_mode->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        out.eq_mode->setMinimumContentsLength(2);
        out.eq_mode->setMinimumWidth(54);
        out.eq_mode->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        eq_row->addWidget(out.eq_mode, 2);
        eq_row->addWidget(new QLabel("Preset", eq_controls));
        out.eq_preset = new QComboBox(eq_controls);
        out.eq_preset->setObjectName("viperEqPreset");
        out.eq_preset->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        out.eq_preset->setMinimumContentsLength(9);
        out.eq_preset->setMinimumWidth(92);
        out.eq_preset->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        eq_row->addWidget(out.eq_preset, 3);
        eq_controls->setMinimumWidth(0);
        eq_controls->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        eq_form->addRow(eq_controls);
        out.eq_overview = new EqOverview(eq_group);
        eq_form->addRow(out.eq_overview);
        auto* eq_actions = new QWidget(eq_group);
        auto* eq_action_row = new QHBoxLayout(eq_actions);
        eq_action_row->setContentsMargins(0, 0, 0, 0);
        eq_action_row->setSpacing(6);
        auto* eq_save = new QPushButton("Save preset…", eq_actions);
        auto* eq_delete = new QPushButton("Delete preset", eq_actions);
        eq_delete->setObjectName("viperEqDeletePreset");
        out.eqDelete = eq_delete;
        out.eqSave = eq_save;
        eq_action_row->addWidget(eq_save);
        eq_action_row->addWidget(eq_delete);
        eq_action_row->addStretch(1);
        eq_form->addRow(eq_actions);
        out.eqGroup = eq_group;

        // Dynamic EQ: matching compact, clickable maximum-target preview.
        // Ten configurable native filters live in one separate dialog.
        auto* deq_group = new QGroupBox("Dynamic EQ", parent);
        auto* deq_form = new QFormLayout(deq_group);
        deq_form->setRowWrapPolicy(QFormLayout::WrapLongRows);
        out.deq_enabled = new EffectSwitch(deq_group);
        deq_form->addRow("Enable native Dynamic EQ", out.deq_enabled);
        out.deq_count = new QSpinBox(deq_group);
        out.deq_count->setObjectName("viperDynamicEqMainBandCount");
        out.deq_count->setRange(1, 10);
        out.deq_count->setMinimumWidth(70);
        out.deq_count->setMaximumWidth(104);
        out.deq_count->setToolTip("Number of active native Dynamic EQ bands; also adjustable in the editor");
        deq_form->addRow("Active bands", out.deq_count);
        auto* deq_label = new QLabel("Illustrative target curve — click to edit bands", deq_group);
        deq_label->setWordWrap(true);
        deq_form->addRow(deq_label);
        out.deq_overview = new DynamicEqOverview(deq_group);
        deq_form->addRow(out.deq_overview);
        out.deqGroup = deq_group;


    return out;
}

template<typename Slider>
struct ClaritySpectrumPanels {
    QGroupBox* clarityGroup{};
    QGroupBox* spectrumGroup{};
    EffectSwitch* clarity_enabled{};
    QComboBox* clarity_mode{};
    Slider clarity_gain{};
    EffectSwitch* spectrum_enabled{};
    Slider spectrum_frequency{};
    Slider spectrum_exciter{};
};
template<typename AddSlider>
auto createClaritySpectrumPanels(QWidget* parent, AddSlider addSlider) {
    using Slider = decltype(addSlider(static_cast<QFormLayout*>(nullptr), "", 0, 0));
    ClaritySpectrumPanels<Slider> out;
        auto* clarity_group = new QGroupBox("ViPER Clarity", parent);
        auto* clarity_form = new QFormLayout(clarity_group);
        out.clarity_enabled = new EffectSwitch(clarity_group);
        clarity_form->addRow("Clarity processing", out.clarity_enabled);
        out.clarity_mode = new QComboBox(clarity_group);
        out.clarity_mode->addItems({"Natural", "Ozone", "XHiFi"});
        clarity_form->addRow("Clarity mode", out.clarity_mode);
        out.clarity_gain = addSlider(clarity_form, "Clarity strength", 0, 45); // tenths, 0..4.5
        out.clarityGroup = clarity_group;

        auto* spectrum_group = new QGroupBox("Spectrum Extension", parent);
        auto* spectrum_form = new QFormLayout(spectrum_group);
        out.spectrum_enabled = new EffectSwitch(spectrum_group);
        spectrum_form->addRow("Spectrum processing", out.spectrum_enabled);
        out.spectrum_frequency = addSlider(spectrum_form, "Reference frequency", 22, 82); // 100 Hz increments
        out.spectrum_frequency.slider->setToolTip("Reference frequency, 2200–8200 Hz (100 Hz steps).");
        out.spectrum_exciter = addSlider(spectrum_form, "Harmonic strength", 0, 60); // tenths, 0..6
        out.spectrum_exciter.slider->setToolTip("Added harmonic enhancement, 0.0–6.0.");
        out.spectrumGroup = spectrum_group;


    return out;
}

template<typename Slider>
struct TubeAnalogXPanels {
    QGroupBox* tubeGroup{};
    QGroupBox* analogxGroup{};
    EffectSwitch* tube_enabled{};
    QComboBox* tube_model{};
    Slider tube_drive{};
    Slider tube_mix{};
    EffectSwitch* analogx_enabled{};
    QComboBox* analogx_mode{};
};
template<typename AddSlider>
auto createTubeAnalogXPanels(QWidget* parent, AddSlider addSlider) {
    using Slider = decltype(addSlider(static_cast<QFormLayout*>(nullptr), "", 0, 0));
    TubeAnalogXPanels<Slider> out;
        // Tube Simulator is the native ViPERDSP TubeSimulator: no third-party DSP.
        auto* tube_group = new QGroupBox("Tube Simulator", parent);
        auto* tube_form = new QFormLayout(tube_group);
        out.tube_enabled = new EffectSwitch(tube_group);
        tube_form->addRow("Enable tube simulation", out.tube_enabled);
        out.tube_model = new QComboBox(tube_group);
        out.tube_model->addItems({"12AX7 (High Gain)", "12AU7 (Low Gain)", "6N1P (Medium Gain)"});
        out.tube_model->setToolTip("Native ViPERDSP vacuum-tube models.");
        tube_form->addRow("Tube model", out.tube_model);
        out.tube_drive = addSlider(tube_form, "Drive", 0, 100);
        out.tube_drive.slider->setToolTip("Tube harmonic drive; 0–100% (native range 0.0–1.0).");
        out.tube_mix = addSlider(tube_form, "Wet/Dry mix", 0, 100);
        out.tube_mix.slider->setToolTip("How much processed tube signal to blend in; 0% dry, 100% wet.");
        out.tubeGroup = tube_group;

        // AnalogX has exactly the three upstream modes; no invented controls.
        auto* analogx_group = new QGroupBox("AnalogX", parent);
        auto* analogx_form = new QFormLayout(analogx_group);
        out.analogx_enabled = new EffectSwitch(analogx_group);
        analogx_form->addRow("Enable AnalogX", out.analogx_enabled);
        out.analogx_mode = new QComboBox(analogx_group);
        out.analogx_mode->addItems({"Mild", "Medium", "Strong"});
        out.analogx_mode->setToolTip("Native ViPERDSP AnalogX coloration modes.");
        analogx_form->addRow("Processing mode", out.analogx_mode);
        out.analogxGroup = analogx_group;


    return out;
}

} // namespace viper_panels
