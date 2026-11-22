// Offscreen regression: verify the colors Qt actually assigns to the main
// window and its dialogs, rather than parsing variable names in C++ source.
#include "viper-theme.h"
#include <QApplication>
#include <QDialog>
#include <QLabel>
#include <QMainWindow>
#include <QMessageBox>
#include <QPalette>
#include <QTabWidget>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <initializer_list>

static bool check(bool ok, const char* message) {
    if (!ok) std::fprintf(stderr, "dialog theme: %s\n", message);
    return ok;
}

static double luminance(const QColor& color) {
    const auto linear = [](int channel) {
        const double v = static_cast<double>(channel) / 255.0;
        return v <= 0.04045 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * linear(color.red())
         + 0.7152 * linear(color.green())
         + 0.0722 * linear(color.blue());
}
static double contrast(const QColor& a, const QColor& b) {
    const double x = luminance(a), y = luminance(b);
    return (std::max(x, y) + 0.05) / (std::min(x, y) + 0.05);
}

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QMainWindow owner;
    owner.setObjectName("viperWindow");
    QTabWidget tabs(&owner);
    const QPalette originalAppPalette = app.palette();
    bool passed = true;

    for (const bool classic : {false, true}) {
        for (const bool dark : {false, true}) {
            // Exercise the production appearance function and inspect the
            // resulting palette for Modern/Classic × dark/light.
            viper_theme::apply(&owner, classic, dark, nullptr, nullptr,
                               &tabs, nullptr, nullptr);
            const QPalette palette = owner.palette();
            passed &= check((palette.color(QPalette::Window).lightness() < 130) == dark,
                            "main window's light/dark mode differs from selected scheme");
            passed &= check(contrast(palette.color(QPalette::Window),
                                     palette.color(QPalette::WindowText)) >= 4.5,
                            "main window text has insufficient contrast");
            passed &= check(contrast(palette.color(QPalette::AlternateBase),
                                     palette.color(QPalette::WindowText)) >= 4.5,
                            "card text has insufficient contrast");
            passed &= check(contrast(palette.color(QPalette::Base),
                                     palette.color(QPalette::Text)) >= 4.5,
                            "input text has insufficient contrast");

            QDialog preferences(&owner);
            viper_theme::applyDialogTheme(&preferences, &owner);
            preferences.ensurePolished();
            passed &= check(preferences.testAttribute(Qt::WA_WindowPropagation),
                            "dialog must propagate theme to child widgets");
            passed &= check(preferences.palette().color(QPalette::Window) ==
                            palette.color(QPalette::Window),
                            "dialog background differs from owner");
            passed &= check(preferences.palette().color(QPalette::WindowText) ==
                            palette.color(QPalette::WindowText),
                            "dialog foreground differs from owner");

            QMessageBox about(&owner);
            viper_theme::applyDialogTheme(&about, &owner);
            about.ensurePolished();
            passed &= check(about.palette().color(QPalette::Window) ==
                            palette.color(QPalette::Window),
                            "message box background differs from owner");
            passed &= check(about.palette().color(QPalette::WindowText) ==
                            palette.color(QPalette::WindowText),
                            "message box foreground differs from owner");
        }
    }
    passed &= check(app.palette().color(QPalette::Window) ==
                    originalAppPalette.color(QPalette::Window),
                    "themed windows changed QApplication palette");
    return passed ? 0 : 1;
}
