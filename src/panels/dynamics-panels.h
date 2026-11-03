// Dynamics control construction. ViperWindow keeps all settings,
// signal wiring, DSP synchronization and effect-card expansion.
#pragma once
#include "widgets/effect-widgets.h"
#include <QByteArray>
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSizePolicy>
#include <QTabBar>
#include <QTabWidget>
#include <QToolButton>
#include <QWidget>
#include <array>

namespace viper_panels {
template<typename Slider>
struct FetPanel {
    QGroupBox* group{};
    EffectSwitch* fet_enabled{};
    Slider fet_threshold{}, fet_ratio{}, fet_knee{}, fet_gain{}, fet_attack{}, fet_release{};
};
template<typename AddSlider>
auto createFetPanel(QWidget* parent, AddSlider addSlider) {
    using Slider = decltype(addSlider(static_cast<QFormLayout*>(nullptr), "", 0, 0));
    FetPanel<Slider> out;
        auto* fet_group = new QGroupBox("FET Compressor", parent);
        auto* fet_form = new QFormLayout(fet_group);
        fet_form->setRowWrapPolicy(QFormLayout::WrapLongRows);
        out.fet_enabled = new EffectSwitch(fet_group);
        fet_form->addRow("Enable FET dynamic compression", out.fet_enabled);
        out.fet_threshold = addSlider(fet_form, "Threshold", -48, 0);
        out.fet_ratio = addSlider(fet_form, "Compression ratio", 10, 100);
        out.fet_knee = addSlider(fet_form, "Soft knee", 0, 12);
        out.fet_gain = addSlider(fet_form, "Makeup gain", 0, 24);
        out.fet_attack = addSlider(fet_form, "Attack", 1, 100);
        out.fet_release = addSlider(fet_form, "Release", 5, 500);
        auto* fet_note = new QLabel("Native ViPERDSP FET compression; start with conservative settings. "
                                    "Keep off when you want to preserve recording dynamics.", fet_group);
        fet_note->setWordWrap(true);
        fet_form->addRow(fet_note);
        out.group = fet_group;
    return out;
}

struct MultibandPanel {
    QGroupBox* group{};
    EffectSwitch* mb_enabled{};
    QComboBox* mb_band_count{};
    QTabWidget* mb_band_tabs{};
    QWidget* mb_crossover_panel{};
    QToolButton* crossoverToggle{};
};
// Populate ViperWindow's existing arrays by reference, retaining all five
// per-band groups and four crossover slider rows without copying them.
template<typename AddSlider, typename Slider, typename BandRows>
MultibandPanel createMultibandPanel(
    QWidget* parent, AddSlider addSlider,
    std::array<Slider,4>& mb_crossovers,
    std::array<QLabel*,5>& mb_band_ranges,
    std::array<BandRows,5>& mb_bands) {
    MultibandPanel out;
        auto* mb_group = new QGroupBox("Multiband Compressor", parent);
        auto* mb_form = new QFormLayout(mb_group);
        mb_form->setRowWrapPolicy(QFormLayout::WrapLongRows);
        out.mb_enabled = new EffectSwitch(mb_group);
        mb_form->addRow("Enable native multiband compression", out.mb_enabled);
        out.mb_band_count = new QComboBox(mb_group);
        out.mb_band_count->addItems({"3 bands", "5 bands"});
        mb_form->addRow("Processing bands", out.mb_band_count);
        // Frequency bands follow bass -> treble. Select one to edit; all five
        // native compressor instances and their saved values remain intact.
        out.mb_band_tabs = new QTabWidget(mb_group);
        out.mb_band_tabs->setObjectName("viperMultibandBandSelector");
        out.mb_band_tabs->setDocumentMode(true);
        out.mb_band_tabs->setUsesScrollButtons(true);
        out.mb_band_tabs->tabBar()->setExpanding(true);
        out.mb_band_tabs->setMinimumWidth(0);
        out.mb_band_tabs->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        out.mb_band_tabs->setAccessibleName("Multiband frequency bands, bass to treble");
        for (int i = 0; i < 5; ++i) {
            auto* page = new QWidget(out.mb_band_tabs);
            auto* form = new QFormLayout(page);
            form->setRowWrapPolicy(QFormLayout::WrapLongRows);
            mb_band_ranges[i] = new QLabel(page);
            mb_band_ranges[i]->setWordWrap(true);
            form->addRow(mb_band_ranges[i]);
            mb_bands[i].enabled = new QCheckBox("Compress this band", page);
            form->addRow(mb_bands[i].enabled);
            mb_bands[i].threshold = addSlider(form, "Threshold", -48, 0);
            mb_bands[i].ratio = addSlider(form, "Ratio", 10, 100);
            mb_bands[i].knee = addSlider(form, "Soft knee", 0, 12);
            mb_bands[i].gain = addSlider(form, "Makeup gain", 0, 24);
            mb_bands[i].attack = addSlider(form, "Attack", 1, 100);
            mb_bands[i].release = addSlider(form, "Release", 5, 500);
            out.mb_band_tabs->addTab(page, QString("Band %1").arg(i+1));
        }
        mb_form->addRow(out.mb_band_tabs);
        auto* mb_crossover_toggle = new QToolButton(mb_group);
        out.crossoverToggle = mb_crossover_toggle;
        mb_crossover_toggle->setObjectName("viperMultibandCrossoverToggle");
        mb_crossover_toggle->setText("Edit crossover frequencies");
        mb_crossover_toggle->setCheckable(true);
        mb_crossover_toggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        mb_crossover_toggle->setArrowType(Qt::RightArrow);
        mb_crossover_toggle->setToolTip("Adjust boundaries between the frequency bands");
        mb_form->addRow(mb_crossover_toggle);
        out.mb_crossover_panel = new QWidget(mb_group);
        auto* mb_crossover_form = new QFormLayout(out.mb_crossover_panel);
        mb_crossover_form->setRowWrapPolicy(QFormLayout::WrapLongRows);
        for (int i = 0; i < 4; ++i) {
            const QByteArray caption = QString("Crossover %1").arg(i+1).toUtf8();
            mb_crossovers[i] = addSlider(mb_crossover_form, caption.constData(), 80, 15000);
        }
        mb_form->addRow(out.mb_crossover_panel);
        out.mb_crossover_panel->setVisible(false);
        auto* mb_note = new QLabel("Each active band uses native ViPERDSP FET compression. "
                                   "Start with low ratios and watch loudness; crossovers must increase.", mb_group);
        mb_note->setWordWrap(true);
        mb_form->addRow(mb_note);
        out.group = mb_group;
    return out;
}

} // namespace viper_panels
