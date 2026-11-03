// Sound Correction widget construction only. Source callbacks, file-system
// watching and live DSP updates are deliberately retained in ViperWindow.
#pragma once
#include "widgets/effect-widgets.h"
#include "widgets/irs-combo-box.h"
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSizePolicy>
#include <QTextOption>
#include <QWidget>

namespace viper_panels {

struct DdcPanel {
    QGroupBox* group{};
    EffectSwitch* ddc_enabled{};
    IrsComboBox* ddc_library{};
    QLabel* ddc_file_name{};
    QLabel* ddc_status{};
    QPushButton* importDdc{};
    QPushButton* refreshDdc{};
};
inline DdcPanel createDdcPanel(QWidget* parent) {
    DdcPanel out;
        auto* ddc_group = new QGroupBox("ViPER-DDC", parent);
        auto* ddc_form = new QFormLayout(ddc_group);
        ddc_form->setRowWrapPolicy(QFormLayout::WrapLongRows);
        out.ddc_enabled = new EffectSwitch(ddc_group);
        ddc_form->addRow("Enable device correction", out.ddc_enabled);
        auto* ddc_file_row = new QWidget(ddc_group);
        auto* ddc_buttons = new QHBoxLayout(ddc_file_row);
        ddc_buttons->setContentsMargins(0,0,0,0);
        ddc_file_row->setMinimumWidth(0);
        ddc_file_row->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        out.ddc_library = new IrsComboBox(ddc_file_row);
        out.ddc_library->setMinimumWidth(0);
        out.ddc_library->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        out.ddc_library->setToolTip("Select a .vdc profile from your local VDC library");
        auto* importDdc = new QPushButton("Import…", ddc_file_row);
        out.importDdc = importDdc;
        auto* refreshDdc = new QPushButton("Refresh", ddc_file_row);
        out.refreshDdc = refreshDdc;
        ddc_buttons->addWidget(out.ddc_library, 1);
        ddc_buttons->addWidget(importDdc);
        ddc_buttons->addWidget(refreshDdc);
        ddc_form->addRow("Correction profile", ddc_file_row);
        out.ddc_file_name = new QLabel(ddc_group);
        out.ddc_file_name->setWordWrap(true);
        out.ddc_file_name->setTextInteractionFlags(Qt::TextSelectableByMouse);
        ddc_form->addRow(out.ddc_file_name);
        out.ddc_status = new QLabel("No DDC profile loaded.", ddc_group);
        out.ddc_status->setWordWrap(true);
        ddc_form->addRow(out.ddc_status);
        auto* ddc_note = new QLabel(
            "Select a .vdc correction profile designed for your specific headphone model. "
            "Correction is available at 44.1 and 48 kHz; at other processing rates, "
            "ViPER-DDC is bypassed. Default: off.", ddc_group);
        ddc_note->setWordWrap(true);
        ddc_form->addRow(ddc_note);
        out.group = ddc_group;
    return out;
}

struct SpeakerCorrectionPanel {
    QGroupBox* group{};
    EffectSwitch* speaker_enabled{};
};
inline SpeakerCorrectionPanel createSpeakerCorrectionPanel(QWidget* parent) {
    SpeakerCorrectionPanel out;
        auto* speaker_group = new QGroupBox("Speaker Correction", parent);
        auto* speaker_form = new QFormLayout(speaker_group);
        speaker_form->setRowWrapPolicy(QFormLayout::WrapLongRows);
        out.speaker_enabled = new EffectSwitch(speaker_group);
        speaker_form->addRow("Enable native speaker correction", out.speaker_enabled);
        auto* speaker_note = new QLabel(
            "For loudspeakers only, not headphones. Uses ViPERDSP's built-in high-pass, "
            "low-pass and band-pass filters; no correction file required. Default: off.", speaker_group);
        speaker_note->setWordWrap(true);
        speaker_form->addRow(speaker_note);
        out.group = speaker_group;
    return out;
}

template<typename Slider>
struct ConvolverPanel {
    QGroupBox* group{};
    EffectSwitch* convolver_enabled{};
    IrsComboBox* convolver_library{};
    QPlainTextEdit* convolver_full_name{};
    Slider convolver_cross{};
    QLabel* convolver_status{};
    QPushButton* browse{};
    QPushButton* refresh{};
};
template<typename AddSlider>
auto createConvolverPanel(QWidget* parent, AddSlider addSlider) {
    using Slider = decltype(addSlider(static_cast<QFormLayout*>(nullptr), "", 0, 0));
    ConvolverPanel<Slider> out;
        auto* conv_group = new QGroupBox("Convolver", parent);
        auto* conv_form = new QFormLayout(conv_group);
        // On a narrow screen, put the controls on a new line rather than
        // allowing a long label/row to force horizontal overflow.
        conv_form->setRowWrapPolicy(QFormLayout::WrapLongRows);
        out.convolver_enabled = new EffectSwitch(conv_group);
        conv_form->addRow("Enable native convolution", out.convolver_enabled);
        auto* fileRow = new QWidget(conv_group);
        auto* fileLayout = new QHBoxLayout(fileRow);
        fileLayout->setContentsMargins(0,0,0,0);
        fileLayout->setSpacing(6);
        fileRow->setMinimumWidth(0);
        fileRow->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        out.convolver_library = new IrsComboBox(fileRow);
        out.convolver_library->setToolTip("Select an impulse response. Hover to see the full filename.");
        auto* browse = new QPushButton("Import…", fileRow);
        out.browse = browse;
        auto* refresh = new QPushButton("Refresh / Reload", fileRow);
        out.refresh = refresh;
        fileLayout->addWidget(out.convolver_library, 1);
        fileLayout->addWidget(browse);
        fileLayout->addWidget(refresh);
        conv_form->addRow("Impulse response", fileRow);
        // Unlike the compact combo box, this line shows the complete selected
        // filename. It can wrap and be selected/copied even on a small screen.
        out.convolver_full_name = new QPlainTextEdit(conv_group);
        out.convolver_full_name->setObjectName("viperIrsFullName");
        out.convolver_full_name->setReadOnly(true);
        out.convolver_full_name->setLineWrapMode(QPlainTextEdit::WidgetWidth);
        // Unlike QLabel wordWrap, this works even with 250 uninterrupted
        // characters. The entire original name remains selectable/copyable;
        // excess lines can scroll vertically, never horizontally.
        out.convolver_full_name->setWordWrapMode(QTextOption::WrapAnywhere);
        out.convolver_full_name->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        out.convolver_full_name->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        out.convolver_full_name->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        out.convolver_full_name->setFixedHeight(52);
        out.convolver_full_name->setMinimumWidth(0);
        out.convolver_full_name->setAccessibleName("Full selected IRS filename");
        conv_form->addRow(out.convolver_full_name);
        out.convolver_cross = addSlider(conv_form, "Cross-channel mix", 0, 100);
        out.convolver_status = new QLabel("Select a kernel to load it into the running audio backend.", conv_group);
        out.convolver_status->setWordWrap(true);
        conv_form->addRow(out.convolver_status);
        out.group = conv_group;
    return out;
}

} // namespace viper_panels
