// Offscreen regression: ViPER-owned top-level dialogs retain foreground and
// background contrast in both schemes, without changing QApplication's palette.
#include "viper-theme.h"
#include <QApplication>
#include <QDialog>
#include <QLabel>
#include <QMainWindow>
#include <QMessageBox>
#include <QPalette>
#include <cstdio>
#include <initializer_list>

static bool check(bool ok, const char* message) {
    if (!ok) std::fprintf(stderr, "dialog theme: %s\n", message);
    return ok;
}
int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QMainWindow owner;
    const QPalette appPalette = app.palette();
    bool passed = true;
    for (const bool dark : {true, false}) {
        const QColor background(dark ? "#172428" : "#f8fbfb");
        const QColor foreground(dark ? "#edf4f6" : "#203038");
        const QColor input(dark ? "#303639" : "#ffffff");
        QPalette palette = owner.palette();
        for (auto group : {QPalette::Active, QPalette::Inactive, QPalette::Disabled}) {
            palette.setColor(group, QPalette::Window, background);
            palette.setColor(group, QPalette::WindowText, foreground);
            palette.setColor(group, QPalette::Text, foreground);
            palette.setColor(group, QPalette::Base, input);
        }
        owner.setPalette(palette);

        QDialog preferences(&owner);
        viper_theme::applyDialogTheme(&preferences, &owner);
        preferences.ensurePolished();
        passed &= check(preferences.testAttribute(Qt::WA_WindowPropagation),
                        "dialog must propagate theme to child widgets");
        passed &= check(preferences.palette().color(QPalette::Window) == background,
                        "dialog background differs from owner");
        passed &= check(preferences.palette().color(QPalette::WindowText) == foreground,
                        "dialog foreground differs from owner");
        passed &= check(preferences.styleSheet().contains(background.name()) &&
                        preferences.styleSheet().contains(foreground.name()),
                        "dialog style lacks matched background/foreground");

        QMessageBox about(&owner);
        viper_theme::applyDialogTheme(&about, &owner);
        about.ensurePolished();
        passed &= check(about.palette().color(QPalette::Window) == background,
                        "message box background differs from owner");
        passed &= check(about.palette().color(QPalette::WindowText) == foreground,
                        "message box foreground differs from owner");
    }
    passed &= check(app.palette().color(QPalette::Window) ==
                    appPalette.color(QPalette::Window),
                    "dialog theme changed QApplication palette");
    return passed ? 0 : 1;
}
