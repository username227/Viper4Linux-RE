// Spatial tab controls only. All persistence, DSP connections and card
// enable/expand logic remain in the main window.
#pragma once
#include "widgets/effect-widgets.h"
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSlider>
#include <QWidget>
#include <utility>

namespace viper_panels {
template<typename Slider>
struct SpatialPanels {
    QGroupBox* crossfeedGroup{};
    QGroupBox* headphoneGroup{};
    QGroupBox* diffGroup{};
    QGroupBox* fieldGroup{};
    QGroupBox* reverbGroup{};
    QGroupBox* imagerGroup{};
    EffectSwitch* crossfeed_enabled{};
    QComboBox* crossfeed_mode{};
    EffectSwitch* headphone_enabled{};
    QComboBox* headphone_quality{};
    EffectSwitch* diff_enabled{};
    Slider diff_delay{};
    QCheckBox* diff_reverse{};
    Slider diff_mix{};
    Slider diff_lowpass{};
    EffectSwitch* field_enabled{};
    Slider field_widen{};
    Slider field_mid{};
    Slider field_depth{};
    EffectSwitch* reverb_enabled{};
    Slider reverb_room{};
    Slider reverb_width{};
    Slider reverb_damp{};
    Slider reverb_wet{};
    Slider reverb_dry{};
    EffectSwitch* imager_enabled{};
    Slider imager_low{};
    Slider imager_mid{};
    Slider imager_high{};
    Slider imager_low_xover{};
    Slider imager_high_xover{};
};

template<typename AddSlider>
auto createSpatialPanels(QWidget* parent, AddSlider addSlider) {
    using Slider = decltype(addSlider(static_cast<QFormLayout*>(nullptr), "", 0, 0));
    SpatialPanels<Slider> out;
    out.crossfeedGroup = new QGroupBox("Crossfeed (ViPER Cure)", parent);
    auto* crossfeed_form = new QFormLayout(out.crossfeedGroup);
    out.crossfeed_enabled = new EffectSwitch(out.crossfeedGroup);
    crossfeed_form->addRow("Speaker-like headphone crossfeed", out.crossfeed_enabled);
    out.crossfeed_mode = new QComboBox(out.crossfeedGroup);
    out.crossfeed_mode->addItems({"Mild", "Medium", "Strong"});
    out.crossfeed_mode->setToolTip("Native ViPERDSP Cure: filtered left/right crossfeed presets.");
    crossfeed_form->addRow("Crossfeed strength", out.crossfeed_mode);

        auto* headphone_group = new QGroupBox("Headphone Surround+", parent);
        auto* headphone_form = new QFormLayout(headphone_group);
        out.headphone_enabled = new EffectSwitch(headphone_group);
        headphone_form->addRow("Surround processing", out.headphone_enabled);
        out.headphone_quality = new QComboBox(headphone_group);
        out.headphone_quality->addItems({"Level 1", "Level 2", "Level 3", "Level 4", "Level 5"});
        out.headphone_quality->setToolTip("Five distinct upstream VHE surround profiles; higher levels are not necessarily better. Supported at 44.1 and 48 kHz.");
        headphone_form->addRow("Surround level", out.headphone_quality);
        out.headphoneGroup = headphone_group;

        auto* diff_group = new QGroupBox("Differential Surround", parent);
        auto* diff_form = new QFormLayout(diff_group);
        out.diff_enabled = new EffectSwitch(diff_group);
        diff_form->addRow("Surround processing", out.diff_enabled);
        out.diff_delay = addSlider(diff_form, "Channel delay", 1, 20);
        out.diff_delay.slider->setToolTip("Delay one stereo channel by 1–20 milliseconds.");
        out.diff_reverse = new QCheckBox("Delay left instead of right", diff_group);
        diff_form->addRow("Reverse channel", out.diff_reverse);
        out.diff_mix = addSlider(diff_form, "Delayed channel mix", 0, 100);
        out.diff_mix.slider->setToolTip("0% fully copies the direct channel; 100% uses only delayed channel.");
        out.diff_lowpass = addSlider(diff_form, "Delayed-channel low-pass", 0, 200);
        out.diff_lowpass.slider->setToolTip("0 means off; other values are in 100 Hz steps, up to 20,000 Hz.");
        out.diffGroup = diff_group;

        auto* field_group = new QGroupBox("Field Surround", parent);
        auto* field_form = new QFormLayout(field_group);
        out.field_enabled = new EffectSwitch(field_group);
        field_form->addRow("Spatial processing", out.field_enabled);
        out.field_widen = addSlider(field_form, "Stereo widening", 0, 80);
        out.field_widen.slider->setToolTip("Upstream widening parameter, 0.0–8.0 in 0.1 steps. Start low.");
        out.field_mid = addSlider(field_form, "Mid image", 100, 200);
        out.field_mid.slider->setToolTip("Upstream mid-image parameter, 1.00–2.00 in 0.01 steps.");
        out.field_depth = addSlider(field_form, "Depth", 0, 16);
        out.field_depth.slider->setToolTip("0 disables depth; 1–16 select upstream 200–950 in steps of 50.");
        out.fieldGroup = field_group;

        auto* reverb_group = new QGroupBox("Reverberation", parent);
        auto* reverb_form = new QFormLayout(reverb_group);
        out.reverb_enabled = new EffectSwitch(reverb_group);
        reverb_form->addRow("Room effect", out.reverb_enabled);
        out.reverb_room = addSlider(reverb_form, "Room size", 0, 100);
        out.reverb_room.slider->setToolTip("ViPERDSP room size, 0–100% (internally 0.0–1.0).");
        out.reverb_width = addSlider(reverb_form, "Stereo width", 0, 100);
        out.reverb_width.slider->setToolTip("Width of the simulated reverberant field.");
        out.reverb_damp = addSlider(reverb_form, "Damping", 0, 100);
        out.reverb_damp.slider->setToolTip("Higher damping reduces high-frequency reverberation.");
        out.reverb_wet = addSlider(reverb_form, "Reverb wet level", 0, 100);
        out.reverb_wet.slider->setToolTip("Reverberant signal level. Start at 20% and adjust gently.");
        out.reverb_dry = addSlider(reverb_form, "Reverb dry level", 0, 100);
        out.reverb_dry.slider->setToolTip("Original direct signal level in the reverberation model. Use 50% for unity dry gain (upstream scales dry by 2).");
        out.reverbGroup = reverb_group;
        // Native ViPERDSP Stereo Imager: independent low, mid and high width.
        // Set 100% for neutral width and keep the entire effect off by default.
        auto* imager_group = new QGroupBox("Stereo Imager", parent);
        auto* imager_form = new QFormLayout(imager_group);
        out.imager_enabled = new EffectSwitch(imager_group);
        imager_form->addRow("Three-band stereo imaging", out.imager_enabled);
        out.imager_low = addSlider(imager_form, "Low-band width", 0, 200);
        out.imager_mid = addSlider(imager_form, "Mid-band width", 0, 200);
        out.imager_high = addSlider(imager_form, "High-band width", 0, 200);
        for (const auto& r : {out.imager_low, out.imager_mid, out.imager_high})
            r.slider->setToolTip("100% = normal width; below narrows, above widens, up to 200%.");
        out.imager_low_xover = addSlider(imager_form, "Low crossover", 80, 400);
        out.imager_high_xover = addSlider(imager_form, "High crossover", 20, 80); // 100 Hz steps
        out.imager_low_xover.slider->setToolTip("Low/mid crossover: 80 to 400 Hz.");
        out.imager_high_xover.slider->setToolTip("Mid/high crossover: 2,000 to 8,000 Hz.");
        out.imagerGroup = imager_group;


    return out;
}
} // namespace viper_panels
