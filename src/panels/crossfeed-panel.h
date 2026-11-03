// Crossfeed (ViPER Cure) presentation: original controls and defaults only.
// Card expansion, settings, DSP publishing and signals remain in ViperWindow.
#pragma once

#include "widgets/effect-widgets.h"
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QWidget>

namespace viper_panels {
struct CrossfeedPanel {
    QGroupBox* group{};
    EffectSwitch* enabled{};
    QComboBox* mode{};
};

inline CrossfeedPanel createCrossfeedPanel(QWidget* parent) {
    CrossfeedPanel panel;
    panel.group = new QGroupBox("Crossfeed (ViPER Cure)", parent);
    auto* crossfeed_form = new QFormLayout(panel.group);
    panel.enabled = new EffectSwitch(panel.group);
    crossfeed_form->addRow("Speaker-like headphone crossfeed", panel.enabled);
    panel.mode = new QComboBox(panel.group);
    panel.mode->addItems({"Mild", "Medium", "Strong"});
    panel.mode->setToolTip("Native ViPERDSP Cure: filtered left/right crossfeed presets.");
    crossfeed_form->addRow("Crossfeed strength", panel.mode);
    return panel;
}
} // namespace viper_panels
