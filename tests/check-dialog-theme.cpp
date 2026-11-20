// Regression: dark/light ViPER palettes must reach separate Qt dialog windows.
// Runs offscreen, without a backend, filesystem preferences or desktop theme.
#include "viper-theme.h"

#include <QApplication>
#include <QDialog>
#include <QLabel>
#include <QMainWindow>
#include <QMessageBox>
#include <QPalette>
#include <QRadioButton>
#include <cstdio>
#include <initializer_list>

static bool check(bool ok, const char* message) {
    if (!ok) std::fprintf(stderr, "dialog theme: %s\n", message);
    return ok;
}

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QMainWindow owner;
    const QColor originalAppBg = app.palette().color(QPalette::Window);
    bool passed = true;

    for (const bool dark : {true, false}) {
        const QColor bg(dark ? "#172428" : "#f8fbfb");
        const QColor fg(dark ? "#edf4f6" : "#203038");
        QPalette ownerPalette = owner.palette();
        for (const auto group : {QPalette::Active, QPalette::Inactive, QPalette::Disabled}) {
            ownerPalette.setColor(group, QPalette::Window, bg);
            ownerPalette.setColor(group, QPalette::WindowText, fg);
            ownerPalette.setColor(group, QPalette::Text, fg);
            ownerPalette.setColor(group, QPalette::ButtonText, fg);
        }
        owner.setPalette(ownerPalette);

        QDialog preferences(&owner);
        viper_theme::applyDialogTheme(&preferences, &owner);
        QLabel description("Preferences section", &preferences);
        QRadioButton choice("Start normally", &preferences);
        preferences.ensurePolished();
        description.ensurePolished();
        choice.ensurePolished();
        passed &= check(preferences.testAttribute(Qt::WA_WindowPropagation),
                        "Preferences must enable palette window propagation");
        passed &= check(preferences.palette().color(QPalette::Window) == bg,
                        "Preferences background must match main window");
        passed &= check(preferences.palette().color(QPalette::WindowText) == fg,
                        "Preferences foreground must match main window");
        passed &= check(description.palette().color(QPalette::WindowText) == fg,
                        "Preferences labels must have readable text");
        passed &= check(preferences.styleSheet().contains(bg.name()) &&
                        preferences.styleSheet().contains(fg.name()),
                        "Preferences stylesheet must pair foreground/background");

        QMessageBox about(&owner);
        viper_theme::applyDialogTheme(&about, &owner);
        about.setText("<b>ViPER4Linux</b>");
        about.setInformativeText("Application revision");
        about.ensurePolished();
        passed &= check(about.testAttribute(Qt::WA_WindowPropagation),
                        "About must enable palette window propagation");
        passed &= check(about.palette().color(QPalette::Window) == bg,
                        "About background must match main window");
        passed &= check(about.palette().color(QPalette::WindowText) == fg,
                        "About foreground must match main window");
        passed &= check(about.styleSheet().contains(bg.name()) &&
                        about.styleSheet().contains(fg.name()),
                        "About stylesheet must pair foreground/background");
    }
    passed &= check(app.palette().color(QPalette::Window) == originalAppBg,
                    "dialog theming must not modify QApplication palette");
    return passed ? 0 : 1;
}
