// Detailed Graphic EQ widget factory. Only presentation belongs here;
// live precision, preset changes, signals and DSP state stay in ViperWindow.
#pragma once

#include "widgets/effect-widgets.h"
#include <QDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QSlider>
#include <QVBoxLayout>
#include <QWidget>
#include <array>

namespace viper_eq_ui {
struct GraphicEqEditorWidgets {
    QDialog* dialog{};
    QWidget* faderContainer{};
    std::array<QWidget*, 31> columns{};
    std::array<QSlider*, 31> faders{};
    std::array<QLabel*, 31> values{};
    std::array<QLabel*, 31> frequencies{};
    QPushButton* reset{};
};

inline GraphicEqEditorWidgets createGraphicEqEditor(QWidget* parent) {
    GraphicEqEditorWidgets widgets;
        widgets.dialog = new QDialog(parent);
        widgets.dialog->setWindowTitle("ViPER Graphic EQ — Detailed Editor");
        widgets.dialog->setObjectName("viperEqEditor");
        widgets.dialog->resize(720, 355);
        widgets.dialog->setMinimumSize(340, 320);
        auto* editorLayout = new QVBoxLayout(widgets.dialog);
        editorLayout->addWidget(new QLabel("Drag individual faders to adjust the current EQ curve.", widgets.dialog));
        auto* eq_scroll = new QScrollArea(widgets.dialog);
        eq_scroll->setObjectName("viperEqScroller");
        eq_scroll->setWidgetResizable(false);
        eq_scroll->setFrameShape(QFrame::NoFrame);
        eq_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        eq_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        eq_scroll->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        widgets.faderContainer = new QWidget(eq_scroll);
        widgets.faderContainer->setObjectName("viperEqFaders");
        auto* eq_fader_row = new QHBoxLayout(widgets.faderContainer);
        eq_fader_row->setSpacing(3);
        eq_fader_row->setContentsMargins(3, 0, 3, 3);
        for (int i = 0; i < 31; ++i) {
            auto* column = new QWidget(widgets.faderContainer);
            column->setFixedWidth(36);
            auto* stack = new QVBoxLayout(column);
            stack->setSpacing(2);
            stack->setContentsMargins(0, 0, 0, 0);
            auto* value = new QLabel("0", column);
            value->setObjectName("viperEqValue");
            value->setAlignment(Qt::AlignCenter);
            value->setProperty("viperSliderValue", true);
            auto* fader = new SafeSlider(Qt::Vertical, column);
            fader->setObjectName("viperEqFader");
            fader->setRange(-1200, 1200);  // hundredths of a decibel
            fader->setSingleStep(5);
            fader->setPageStep(25);
            fader->setTickPosition(QSlider::NoTicks);
            fader->setMinimumHeight(150);
            fader->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
            auto* frequency = new QLabel("", column);
            frequency->setAlignment(Qt::AlignCenter);
            stack->addWidget(value);
            stack->addWidget(fader, 1, Qt::AlignHCenter);
            stack->addWidget(frequency);
            widgets.columns[i] = column;
            widgets.faders[i] = fader;
            widgets.values[i] = value;
            widgets.frequencies[i] = frequency;
            eq_fader_row->addWidget(column);
        }
        widgets.faderContainer->setFixedSize(31 * 39 + 6, 235);
        eq_scroll->setWidget(widgets.faderContainer);
        editorLayout->addWidget(eq_scroll, 1);
        auto* editorActions = new QHBoxLayout();
        widgets.reset = new QPushButton("Flat", widgets.dialog);
        widgets.reset->setToolTip("Set all visible bands in the current layout to 0 dB.");
        editorActions->addWidget(widgets.reset);
        editorActions->addStretch();
        auto* eq_close = new QPushButton("Close", widgets.dialog);
        editorActions->addWidget(eq_close);
        editorLayout->addLayout(editorActions);
        QObject::connect(eq_close, &QPushButton::clicked, widgets.dialog, &QDialog::hide);
    return widgets;
}
} // namespace viper_eq_ui
