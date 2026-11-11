// ViPER4Linux-RE: window-local appearance rendering.
// Color rules and stylesheets moved unchanged from ViperWindow::applyTheme.
// The window retains detection, selection, settings and theme-change signals.
#pragma once
#include "widgets/effect-widgets.h"
#include <QApplication>
#include <QCheckBox>
#include <QColor>
#include <QDialog>
#include <QFrame>
#include <QLabel>
#include <QMainWindow>
#include <QPalette>
#include <QPixmap>
#include <QSlider>
#include <QString>
#include <QTabBar>
#include <QTabWidget>
#include <QToolButton>
#include <QWidget>

namespace viper_theme {
inline QString apply(QMainWindow* window, bool classic, bool prefersDark,
                     QDialog* deqEditor, QDialog* eqEditor,
                     QTabWidget* tabs, QLabel* headerIcon, QLabel* status) {

        // Fully specified pairings, not a mixture of old Qt palette + new QSS.
        // Never set a QApplication stylesheet or palette: a window-local theme
        // keeps other Qt windows, dialogs and the user's desktop unaffected.
        const bool dark = prefersDark;
        const QString windowBg = dark ? (classic ? "#171719" : "#172428")
                                      : (classic ? "#f7f9fb" : "#f8fbfb");
        const QString textColor = dark ? "#edf4f6" : "#203038";
        const QString mutedColor = dark ? "#c0cdd1" : "#536a74";
        const QString accent = classic ? (dark ? "#12b8e8" : "#087eae")
                                       : (dark ? "#3299a5" : "#236e76");
        const QString bg = dark ? (classic ? "#252628" : "#273b40")
                                : (classic ? "#ffffff" : "#eaf3f5");
        const QString border = dark ? (classic ? "#414348" : "#426064")
                                    : (classic ? "#ccdae2" : "#c8dfe2");
        const QString roadBg = dark ? (classic ? "#202124" : "#203236")
                                    : (classic ? "#eef4f7" : "#f0f5f6");
        const QString roadBorder = dark ? (classic ? "#54565a" : "#52666a")
                                        : (classic ? "#b9cbd4" : "#bfd4d7");
        const QString inactiveBg = dark ? (classic ? "#26282b" : "#25383c")
                                        : (classic ? "#e6eef2" : "#e4edef");
        const QString inactiveText = dark ? "#e3edf0" : "#395660";
        const QString inactiveBorder = dark ? "#4d6064" : "#b9cdd2";
        const QString hoverBg = dark ? (classic ? "#363c40" : "#344d52")
                                     : (classic ? "#d3e9f2" : "#d4e5e8");
        const QString inputBg = dark ? "#303639" : "#ffffff";
        const QString inputBorder = dark ? "#617479" : "#b9cbd4";
        const QString inputSel = dark ? "#1b5a68" : "#c9eaf5";
        const QString disabledText = dark ? "#a5afb2" : "#687e87";
        const QString activeTabText = dark ? "#092129" : "#ffffff";
        // The caller retains mutedColor for live status updates.
        // A window palette supplies consistent foreground colors even for
        // children with their own frame stylesheets, which do not necessarily
        // inherit color declarations from a parent stylesheet under Qt.
        QPalette pal = qApp->palette();
        pal.setColor(QPalette::Window, QColor(windowBg));
        pal.setColor(QPalette::WindowText, QColor(textColor));
        pal.setColor(QPalette::Base, QColor(inputBg));
        pal.setColor(QPalette::AlternateBase, QColor(bg));
        pal.setColor(QPalette::Text, QColor(textColor));
        pal.setColor(QPalette::Button, QColor(inputBg));
        pal.setColor(QPalette::ButtonText, QColor(textColor));
        pal.setColor(QPalette::Highlight, QColor(accent));
        pal.setColor(QPalette::HighlightedText, QColor("#ffffff"));
        pal.setColor(QPalette::Mid, QColor(mutedColor));
        pal.setColor(QPalette::Disabled, QPalette::Text, QColor(disabledText));
        pal.setColor(QPalette::Disabled, QPalette::WindowText, QColor(disabledText));
        window->setPalette(pal);  // Window-local only.
        if (deqEditor) deqEditor->setPalette(pal);
        if (eqEditor) {
            eqEditor->setPalette(pal);
            eqEditor->setStyleSheet(QString(
                "QDialog#viperEqEditor { background:%1; color:%2; } "
                "QLabel { color:%2; } QPushButton { color:%2; background:%3; "
                "border:1px solid %4; border-radius:7px; padding:5px 8px; }")
                .arg(windowBg, textColor, inputBg, inputBorder));
        }
        window->setStyleSheet(QString(R"THEME(
            QMainWindow#viperWindow, QWidget#viperCentral,
            QWidget#viperEffectsContent, QScrollArea, QScrollArea > QWidget > QWidget {
                background-color:%1; color:%2; }
            QTabWidget::pane { border:0; background:%1; }
            QLabel, QGroupBox, QToolButton, QRadioButton, QCheckBox { color:%2; }
            QPlainTextEdit#viperIrsFullName {
                color:%2; background-color:transparent; border:0; padding:0;
            }
            QPushButton, QComboBox, QLineEdit, QTextBrowser {
                color:%2; background-color:%3;
                border:1px solid %4; border-radius:7px; padding:5px 8px; }
            QPushButton:hover, QComboBox:hover { border-color:%5; }
            /* The Theme selector was visually verified under Cinnamon in r26.
               Apply its complete rounded cap AND explicit chevron to every
               combo so Qt cannot fall back to a square native arrow button.
               The text reserve belongs on the combo, not the dropdown cap. */
            QComboBox { padding-right:26px; }
            QComboBox::drop-down {
                subcontrol-origin:border; subcontrol-position:top right;
                width:24px; border:0;
                border-top-right-radius:7px; border-bottom-right-radius:7px;
                background-color:transparent;
            }
            QComboBox::down-arrow {
                image:url(%8); width:12px; height:8px;
            }
            QPushButton:pressed { background-color:%6; }
            QPushButton:disabled, QComboBox:disabled { color:%7; }
            QComboBox QAbstractItemView {
                color:%2; background-color:%3;
                selection-color:%2; selection-background-color:%6; }
            QToolTip { color:%2; background-color:%3; border:1px solid %4; }
        )THEME").arg(windowBg, textColor, inputBg, inputBorder, accent,
                       inputSel, disabledText,
                       dark ? QStringLiteral(":/viper4linux/combo-chevron-light.png")
                            : QStringLiteral(":/viper4linux/combo-chevron-dark.png")));

        for (auto* frame : window->findChildren<QFrame*>()) {
            if (frame->objectName() == "viperMasterCard" ||
                frame->objectName() == "viperEffectCard") {
                const QString name = frame->objectName();
                frame->setStyleSheet(QString(
                    "QFrame#%1 { background-color:%2; border:1px solid %3; border-radius:16px; }")
                    .arg(name, bg, border));
            } else if (frame->objectName() == "viperRoadmapCard") {
                frame->setStyleSheet(QString(
                    "QFrame#viperRoadmapCard { background-color:%1; border:1px dashed %2; border-radius:13px; }")
                    .arg(roadBg, roadBorder));
            }
        }
        tabs->tabBar()->setStyleSheet(QString(R"(
            QTabBar::tab { background-color:%1; color:%2; border:1px solid %3;
                border-radius:10px; padding:9px 13px; min-width:80px;
                margin-right:7px; margin-bottom:5px; font-size:14px; font-weight:600; }
            QTabBar::tab:selected { background-color:%4; color:%6; border-color:%4; }
            QTabBar::tab:!selected:hover { background-color:%5; }
        )").arg(inactiveBg, inactiveText, inactiveBorder, accent, hoverBg, activeTabText));
        const QString remaining = dark ? "#52666a" : "#cbdfe4";
        // Qt custom QSS does not automatically dim slider subcontrols.
        // Disabled colors must be part of this final theme stylesheet;
        // addSlider()'s initial stylesheet is replaced here on startup.
        const QString disabledTrack = dark ? "#354449" : "#e1e7e9";
        const QString disabledHandle = dark ? "#64757a" : "#a0b0b5";
        const QString disabledBorder = dark ? "#7b8e94" : "#becbd0";
        for (auto* slider : window->findChildren<QSlider*>()) {
            if (slider->orientation() == Qt::Vertical) {
                slider->setStyleSheet(QString(R"(
                    QSlider::groove:vertical { width:9px; background:%1; border-radius:4px; }
                    QSlider::sub-page:vertical { background:%1; border-radius:4px; }
                    QSlider::add-page:vertical { background:%2; border-radius:4px; }
                    QSlider::handle:vertical { background:%2; border:1px solid %3;
                        height:10px; margin:0 -7px; border-radius:4px; }
                )").arg(remaining, accent, dark ? "#c1e8ee" : "#ffffff"));
                continue;
            }
            slider->setStyleSheet(QString(R"(
                QSlider::groove:horizontal { height:10px; border-radius:5px; background:%1; }
                QSlider::sub-page:horizontal { border-radius:5px; background:%2; }
                QSlider::add-page:horizontal { border-radius:5px; background:%1; }
                QSlider::handle:horizontal { background:%2; border:1px solid %3;
                    width:9px; margin:-8px 0; border-radius:4px; }
                QSlider::groove:horizontal:disabled { background:%4; }
                QSlider::sub-page:horizontal:disabled { background:%4; }
                QSlider::add-page:horizontal:disabled { background:%4; }
                QSlider::handle:horizontal:disabled { background:%5; border:1px solid %6; }
            )").arg(remaining, accent, dark ? "#c1e8ee" : "#ffffff",
                    disabledTrack, disabledHandle, disabledBorder));
        }
        // Title icon follows the selected UI style; the taskbar/window icon
        // stays blue for a stable application identity.
        if (headerIcon) {
            const QPixmap logo(classic ? ":/viper4linux/blue.png" : ":/viper4linux/teal.png");
            headerIcon->setPixmap(logo.scaled(36,36,Qt::KeepAspectRatio,Qt::SmoothTransformation));
        }
        // The vector glyphs receive the theme accent directly, independently
        // of font availability and QLabel style inheritance.
        // Qt6 window->findChildren<T>() requires T to declare Q_OBJECT. EffectGlyph
        // is deliberately a lightweight QWidget with no signals or slots;
        // discover QWidget children and downcast only the glyph instances.
        for (auto* widget : window->findChildren<QWidget*>()) {
            if (auto* glyph = dynamic_cast<EffectGlyph*>(widget))
                glyph->setColor(QColor(accent));
        }
        for (auto* label : window->findChildren<QLabel*>()) {
            if (label == status) continue; // live status color is set by pollBackend().
            if (label->property("viperWarningText").toBool()) {
                label->setStyleSheet(QString(
                    "QLabel { color:%1; font-weight:600; } QLabel:disabled { color:%2; }")
                    .arg(dark ? "#ffce88" : "#9b5e12", disabledText));
            } else if (label->property("viperSliderValue").toBool()) {
                label->setStyleSheet(QString(
                    "QLabel { color:%1; font-weight:600; } QLabel:disabled { color:%2; }")
                    .arg(accent, disabledText));
            } else if (label->property("viperMutedText").toBool() ||
                       label->styleSheet().trimmed() == "color: palette(mid);") {
                label->setProperty("viperMutedText", true);
                label->setStyleSheet(QString(
                    "QLabel { color:%1; } QLabel:disabled { color:%2; }")
                    .arg(mutedColor, disabledText));
            } else {
                // Do not let a former Classic palette leave dark text on dark
                // Modern cards (or vice versa). All plain labels get an explicit
                // foreground; QFont-based bold headings remain bold.
                label->setStyleSheet(QString(
                    "QLabel { color:%1; } QLabel:disabled { color:%2; }")
                    .arg(textColor, disabledText));
            }
        }
        for (auto* button : window->findChildren<QToolButton*>()) {
            if (button->accessibleName().startsWith("Expand or collapse ")) {
                button->setStyleSheet(QString(
                    "QToolButton { color:%1; font-weight:600; font-size:14px; "
                    "border:none; padding:7px 3px; text-align:left; background:transparent; }")
                    .arg(textColor));
            }
        }
        for (auto* cb : window->findChildren<QCheckBox*>()) {
            if (auto* sw = dynamic_cast<EffectSwitch*>(cb)) sw->setClassicTheme(classic, dark);
        }
        if (status && status->text().startsWith("○"))
            status->setStyleSheet(QString("color:%1;").arg(mutedColor));
        return mutedColor;
}
} // namespace viper_theme
