// Offscreen regression: verify the colors Qt actually assigns to the main
// window and its dialogs, rather than parsing variable names in C++ source.
#include "viper-theme.h"
#include "widgets/safe-combo-box.h"
#include "widgets/irs-combo-box.h"
#include <QApplication>
#include <QComboBox>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QtTest/qtestspontaneevent.h>
#include <QDialog>
#include <QLabel>
#include <QMainWindow>
#include <QMessageBox>
#include <QPixmap>
#include <QStyleOptionButton>
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


// Wheel input over an unfocused selector must scroll the form, not edit a DSP
// setting. Exercise actual Qt wheel delivery for normal and IRS/DDC combos.
static bool checkComboWheel(QApplication& app) {
    int changes = 0;
    QScrollArea area;
    auto* content = new QWidget;
    auto* layout = new QVBoxLayout(content);
    auto* normal = new SafeComboBox(content);
    auto* library = new IrsComboBox(content);
    normal->addItems({"First", "Second", "Third"});
    library->addItems({"None", "Room A", "Room B"});
    normal->setCurrentIndex(1);
    library->setCurrentIndex(1);
    layout->addWidget(normal);
    layout->addWidget(library);
    auto* other = new QPushButton("Move focus here", content);
    layout->addWidget(other);
    for (int i = 0; i < 30; ++i)
        layout->addWidget(new QLabel(QString("Scrolling content %1").arg(i), content));
    area.setWidget(content);
    area.setWidgetResizable(true);
    area.resize(280, 180);
    area.show();
    app.processEvents();

    bool passed = check(area.verticalScrollBar()->maximum() > 0,
                        "wheel regression needs a scrollable form");
    auto sendWheel = [&app](QComboBox* combo, int angle) {
        const QPoint center = combo->rect().center();
        QWheelEvent wheel(QPointF(center), QPointF(combo->mapToGlobal(center)),
                          QPoint(), QPoint(0, angle), Qt::NoButton, Qt::NoModifier,
                          Qt::NoScrollPhase, false);
        // sendEvent clears spontaneity and Qt then suppresses parent propagation.
        // Match QtTest's spontaneous widget input delivery instead.
        QSpontaneKeyEvent::setSpontaneous(&wheel);
        app.notify(combo, &wheel);
    };
    other->setFocus();
    app.processEvents();
    passed &= check(other->hasFocus() && !normal->hasFocus() && !library->hasFocus(),
                    "wheel regression needs both selectors initially unfocused");
    QObject::connect(normal, &QComboBox::currentIndexChanged,
                     [&changes](int) { ++changes; });
    QObject::connect(library, &QComboBox::currentIndexChanged,
                     [&changes](int) { ++changes; });
    const int beforeScroll = area.verticalScrollBar()->value();
    sendWheel(normal, -120);
    passed &= check(normal->currentIndex() == 1 && changes == 0 && !normal->hasFocus(),
                    "unfocused standard combo changed under the mouse wheel");
    passed &= check(area.verticalScrollBar()->value() > beforeScroll,
                    "ignored combo wheel did not reach the parent scroll area");

    area.verticalScrollBar()->setValue(0);
    other->setFocus();
    app.processEvents();
    const int beforeLibraryScroll = area.verticalScrollBar()->value();
    sendWheel(library, -120);
    passed &= check(library->currentIndex() == 1 && changes == 0 && !library->hasFocus(),
                    "unfocused IRS/DDC selector changed under the mouse wheel");
    passed &= check(area.verticalScrollBar()->value() > beforeLibraryScroll,
                    "ignored IRS/DDC wheel did not reach the parent scroll area");
    area.verticalScrollBar()->setValue(0);

    normal->setFocus();
    app.processEvents();
    passed &= check(normal->hasFocus(), "standard combo could not receive focus");
    sendWheel(normal, -120);
    passed &= check(normal->currentIndex() == 2 && changes == 1,
                    "focused combo lost its intentional wheel selection");

    library->setFocus();
    app.processEvents();
    passed &= check(library->hasFocus(), "IRS/DDC combo could not receive focus");
    sendWheel(library, -120);
    passed &= check(library->currentIndex() == 2 && changes == 2,
                    "focused IRS/DDC selector lost its intentional wheel selection");
    return passed;
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

            // Check the rendered indicator, not a source string or CSS
            // declaration: the checked tick must be genuinely black even
            // when the dialog inherits ViPER's dark palette.
            ViperPreferenceCheckBox preferenceCheck("Test checkbox", &preferences);
            preferenceCheck.resize(220, 28);
            preferenceCheck.setChecked(true);
            preferenceCheck.ensurePolished();
            QPixmap checkboxImage(preferenceCheck.size());
            checkboxImage.fill(Qt::transparent);
            preferenceCheck.render(&checkboxImage);
            QStyleOptionButton checkOption;
            checkOption.initFrom(&preferenceCheck);
            const QRect indicator = preferenceCheck.style()->subElementRect(
                QStyle::SE_CheckBoxIndicator, &checkOption, &preferenceCheck);
            const QImage bitmap = checkboxImage.toImage();
            int blackPixels = 0;
            for (int y = indicator.top() + 3; y < indicator.bottom() - 2; ++y) {
                for (int x = indicator.left() + 3; x < indicator.right() - 2; ++x) {
                    if (!bitmap.rect().contains(x, y)) continue;
                    const QColor pixel = bitmap.pixelColor(x, y);
                    if (pixel.alpha() > 200 && pixel.red() < 65 &&
                        pixel.green() < 65 && pixel.blue() < 65)
                        ++blackPixels;
                }
            }
            passed &= check(blackPixels >= 2,
                            "checked Preferences indicator lacks a black tick");

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
    passed &= checkComboWheel(app);
    return passed ? 0 : 1;
}
