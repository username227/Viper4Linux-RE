// Bass-panel UI construction only. All settings and DSP/CLI connections
// remain with ViperWindow; the four card wrappers stay independently collapsible.
#pragma once
#include "widgets/effect-widgets.h"
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QWidget>

namespace viper_panels {
template<typename Slider>
struct BassPanels {
    QGroupBox* bassGroup{};
    QGroupBox* monoGroup{};
    QGroupBox* psychoGroup{};
    QGroupBox* dynamicGroup{};
    EffectSwitch* enabled{};
    QComboBox* bass_mode{};
    Slider strength{};
    Slider frequency{};
    EffectSwitch* mono_enabled{};
    QComboBox* mono_mode{};
    Slider mono_strength{};
    Slider mono_frequency{};
    EffectSwitch* psycho_enabled{};
    Slider psycho_cutoff{};
    Slider psycho_intensity{};
    QComboBox* psycho_order{};
    Slider psycho_original{};
    EffectSwitch* dyn_enabled{};
    Slider dyn_xlo{};
    Slider dyn_xhi{};
    Slider dyn_ylo{};
    Slider dyn_yhi{};
    Slider dyn_side_lo{};
    Slider dyn_side_hi{};
    Slider dyn_strength{};
};
template<typename AddSlider>
auto createBassPanels(QWidget* parent, AddSlider addSlider) {
    using Slider = decltype(addSlider(static_cast<QFormLayout*>(nullptr), "", 0, 0));
    BassPanels<Slider> out;
        auto* group = new QGroupBox("ViPER Bass", parent);
        auto* form = new QFormLayout(group);
        out.enabled = new EffectSwitch(group);
        form->addRow("Bass processing", out.enabled);
        out.bass_mode = new QComboBox(group);
        out.bass_mode->addItems({"Natural", "Pure Bass+", "Subwoofer"});
        out.bass_mode->setToolTip("Select one of the three original ViPERDSP Bass processing algorithms.");
        form->addRow("Bass mode", out.bass_mode);
        out.strength = addSlider(form, "Bass strength", 5, 100); // tenths
        out.frequency = addSlider(form, "Frequency", 15, 150);
        out.bassGroup = group;

        auto* mono_group = new QGroupBox("ViPER Bass Mono", parent);
        auto* mono_form = new QFormLayout(mono_group);
        out.mono_enabled = new EffectSwitch(mono_group);
        mono_form->addRow("Mono bass processing", out.mono_enabled);
        out.mono_mode = new QComboBox(mono_group);
        out.mono_mode->addItems({"Natural", "Pure Bass+", "Subwoofer"});
        mono_form->addRow("Bass mode", out.mono_mode);
        out.mono_strength = addSlider(mono_form, "Bass strength", 5, 100);
        out.mono_frequency = addSlider(mono_form, "Frequency", 15, 150);
        auto* mono_note = new QLabel("Mono-derived bass is blended into both channels. Start at a low strength.", mono_group);
        mono_note->setWordWrap(true);
        mono_form->addRow(mono_note);
        out.monoGroup = mono_group;

        auto* psycho_group = new QGroupBox("Psychoacoustic Bass", parent);
        auto* psycho_form = new QFormLayout(psycho_group);
        out.psycho_enabled = new EffectSwitch(psycho_group);
        psycho_form->addRow("Harmonic bass enhancement", out.psycho_enabled);
        out.psycho_cutoff = addSlider(psycho_form, "Bass cutoff", 60, 150);
        out.psycho_intensity = addSlider(psycho_form, "Harmonic intensity", 0, 100);
        out.psycho_order = new QComboBox(psycho_group);
        out.psycho_order->addItems({"2nd order", "3rd order", "4th order", "5th order"});
        psycho_form->addRow("Harmonic order", out.psycho_order);
        out.psycho_original = addSlider(psycho_form, "Original bass level", 0, 100);
        out.psychoGroup = psycho_group;

        auto* dyn_group = new QGroupBox("Dynamic System", parent);
        auto* dyn_form = new QFormLayout(dyn_group);
        out.dyn_enabled = new EffectSwitch(dyn_group);
        dyn_form->addRow("Dynamic bass processing", out.dyn_enabled);
        // Strength is the primary adjustment: keep it above the mode-selecting Low X.
        out.dyn_strength = addSlider(dyn_form, "Dynamic bass strength", 0, 100);
        out.dyn_xlo = addSlider(dyn_form, "Low X frequency", 120, 2400);
        out.dyn_xlo.slider->setToolTip("GUI minimum 120 Hz: simple bass processing; above 120 Hz: advanced filters. CLI values below 120 Hz are preserved.");
        out.dyn_xhi = addSlider(dyn_form, "High X frequency", 0, 12000);
        out.dyn_ylo = addSlider(dyn_form, "Low Y frequency", 0, 200);
        out.dyn_yhi = addSlider(dyn_form, "High Y frequency", 0, 300);
        out.dyn_side_lo = addSlider(dyn_form, "Low side gain", 0, 100);
        out.dyn_side_hi = addSlider(dyn_form, "High side gain", 0, 100);
        auto* dyn_note = new QLabel("Low X = 120 Hz: simple bass processing; advanced filters and side gains are inactive. Above 120 Hz: advanced processing. CLI values below 120 Hz display as 120 here without being changed.", dyn_group);
        dyn_note->setWordWrap(true);
        dyn_form->addRow(dyn_note);
        out.dynamicGroup = dyn_group;


    return out;
}
} // namespace viper_panels
