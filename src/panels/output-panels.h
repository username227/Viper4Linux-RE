// Output tab widget construction. Persistence, backend publishing, control
// connections and independently collapsible card wrappers stay in ViperWindow.
#pragma once
#include "widgets/effect-widgets.h"
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QWidget>

namespace viper_panels {

template<typename Slider>
struct OutputPanels {
    QGroupBox* limiterGroup{};
    QGroupBox* agcGroup{};
    QGroupBox* lufsGroup{};
    EffectSwitch* limiter_indicator{};
    Slider limiter_threshold{};
    Slider limiter_gain{};
    Slider limiter_pan{};
    EffectSwitch* agc_enabled{};
    Slider agc_strength{};
    Slider agc_max_gain{};
    Slider agc_threshold{};
    EffectSwitch* lufs_enabled{};
    Slider lufs_target{};
    Slider lufs_gain{};
    QComboBox* lufs_speed{};
    QLabel* output_warning{};
};

template<typename AddSlider>
auto createOutputPanels(QWidget* parent, AddSlider addSlider) {
    using Slider = decltype(addSlider(static_cast<QFormLayout*>(nullptr), "", 0, 0));
    OutputPanels<Slider> out;

    // Output processing — native limiter is always present, so show its
    // switch as ON but inactive. Its card remains independently collapsible.
    auto* limiter_group = new QGroupBox("Master Limiter", parent);
    auto* limiter_form = new QFormLayout(limiter_group);
    out.limiter_indicator = new EffectSwitch(limiter_group);
    out.limiter_indicator->setChecked(true);
    out.limiter_indicator->setEnabled(false);
    out.limiter_indicator->setToolTip("The native Master Limiter is permanently active. Adjust its threshold, output level and balance below.");
    limiter_form->addRow("Limiter always active", out.limiter_indicator);
    out.limiter_threshold = addSlider(limiter_form, "Peak threshold", 300, 1000); // thousandths
    out.limiter_threshold.slider->setToolTip("Linear peak threshold, -10.46 to 0 dBFS; default -6.02 dBFS.");
    out.limiter_gain = addSlider(limiter_form, "Output gain", 1, 200); // percent
    out.limiter_gain.slider->setToolTip("Final output gain, 1–200%. Raising gain can cause more limiting.");
    out.limiter_pan = addSlider(limiter_form, "Channel balance", -100, 100);
    out.limiter_pan.slider->setToolTip("-100 left; 0 center; +100 right. Attenuates the opposite channel.");
    out.limiterGroup = limiter_group;

    auto* agc_group = new QGroupBox("Playback Gain Control (AGC)", parent);
    auto* agc_form = new QFormLayout(agc_group);
    out.agc_enabled = new EffectSwitch(agc_group);
    agc_form->addRow("Automatic gain control", out.agc_enabled);
    out.agc_strength = addSlider(agc_form, "Strength", 50, 300);
    out.agc_max_gain = addSlider(agc_form, "Maximum gain", 100, 1000);
    out.agc_threshold = addSlider(agc_form, "Output threshold", 300, 1000);
    out.agcGroup = agc_group;

    auto* lufs_group = new QGroupBox("LUFS Targeting", parent);
    auto* lufs_form = new QFormLayout(lufs_group);
    out.lufs_enabled = new EffectSwitch(lufs_group);
    lufs_form->addRow("Automatic loudness targeting", out.lufs_enabled);
    out.lufs_target = addSlider(lufs_form, "Target loudness", -240, -80);
    out.lufs_gain = addSlider(lufs_form, "Maximum gain adjustment", 0, 120);
    out.lufs_speed = new QComboBox(lufs_group);
    out.lufs_speed->addItems({"Slow", "Normal", "Fast"});
    lufs_form->addRow("Response speed", out.lufs_speed);
    out.lufsGroup = lufs_group;

    out.output_warning = new QLabel("AGC and LUFS are both enabled. Their gain adjustments may interact; test carefully.", parent);
    out.output_warning->setWordWrap(true);
    out.output_warning->setProperty("viperWarningText", true);
    out.output_warning->setStyleSheet("color: #b36c20; font-weight: 600;");
    return out;
}
} // namespace viper_panels
