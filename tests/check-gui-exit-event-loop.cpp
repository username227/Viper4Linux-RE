// Exercise the *production* deferred event-loop exit policy against real
// Qt event delivery, not the spelling of C++ source or a fake Qt event loop.
// The window deliberately ignores closeEvent (as ViperWindow does to permit
// Cancel). QApplication::quit() can then re-enter closeEvent and loop.
// No actual backend, PipeWire, desktop session, or system settings are touched.
#include "gui-exit-event-loop.h"
#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>
#include <cstdio>
#include <cstdlib>
#include <cstring>

enum class Scenario {
    CloseKeep, CloseStop, CloseCancel, MenuKeep, MenuStop
};

class ExitWindow final : public QMainWindow {
public:
    explicit ExitWindow(Scenario scenario) : scenario_(scenario) {
        auto* menu = menuBar()->addMenu("Quit options");
        auto* keep = menu->addAction("Quit GUI and keep backend");
        auto* stop = menu->addAction("Quit GUI and stop backend");
        QObject::connect(keep, &QAction::triggered, this,
                         [this] { requestExit(false); });
        QObject::connect(stop, &QAction::triggered, this,
                         [this] { requestExit(true); });
        keepAction_ = keep;
        stopAction_ = stop;
        resize(320, 200);
    }

    void exercise() {
        switch (scenario_) {
        case Scenario::CloseKeep:
        case Scenario::CloseStop:
        case Scenario::CloseCancel:
            // Real QWidget::close() invokes the native Qt close event.
            close();
            return;
        case Scenario::MenuKeep:
            keepAction_->trigger();
            // A synchronous socket callback can still write its response
            // before the deferred exit runs.
            replyCompleted_ = true;
            return;
        case Scenario::MenuStop:
            stopAction_->trigger();
            replyCompleted_ = true;
            return;
        }
    }

    bool passed() const {
        const bool cancel = scenario_ == Scenario::CloseCancel;
        const bool direct = scenario_ == Scenario::MenuKeep ||
                            scenario_ == Scenario::MenuStop;
        const bool shouldStop = scenario_ == Scenario::CloseStop ||
                                scenario_ == Scenario::MenuStop;
        const bool expected = closeEvents_ == (direct ? 0 : 1) &&
                              prompts_ == (direct ? 0 : 1) &&
                              exitRequests_ == (cancel ? 0 : 1) &&
                              stopRequests_ == (shouldStop ? 1 : 0) &&
                              simulatedBackendAlive_ != shouldStop &&
                              replyCompleted_ == direct &&
                              (!cancel || isVisible());
        if (!expected) {
            std::fprintf(stderr,
                "GUI exit behavior: close events=%d prompts=%d "
                "exit requests=%d stop requests=%d backend alive=%d "
                "reply completed=%d window visible=%d\n",
                closeEvents_, prompts_, exitRequests_, stopRequests_,
                simulatedBackendAlive_, replyCompleted_, isVisible());
        }
        return expected;
    }

protected:
    void closeEvent(QCloseEvent* event) override {
        ++closeEvents_;
        event->ignore(); // Cancel must not destroy the window.
        if (closeEvents_ != 1) {
            std::fputs("FAIL: re-entrant closeEvent during GUI shutdown\n", stderr);
            std::_Exit(31); // Stop a possible Qt quit/close cycle immediately.
        }
        if (scenario_ == Scenario::MenuKeep || scenario_ == Scenario::MenuStop) {
            std::fputs("FAIL: explicit quit re-entered closeEvent\n", stderr);
            std::_Exit(32);
        }

        ++prompts_;
        QMessageBox prompt(QMessageBox::Question, "Quit Viper4Linux",
            "Should the audio backend continue running?", QMessageBox::NoButton, this);
        auto* keep = prompt.addButton("Keep backend running", QMessageBox::AcceptRole);
        auto* stop = prompt.addButton("Stop backend", QMessageBox::DestructiveRole);
        auto* cancel = prompt.addButton(QMessageBox::Cancel);
        QPushButton* selected = scenario_ == Scenario::CloseStop ? stop :
                                scenario_ == Scenario::CloseCancel ? cancel : keep;
        // Press a real Qt dialog button without a display server.
        QTimer::singleShot(0, &prompt, [selected] { selected->click(); });
        prompt.exec();
        if (prompt.clickedButton() == keep) requestExit(false);
        else if (prompt.clickedButton() == stop) requestExit(true);
        // Cancel deliberately leaves the event loop and window alive.
    }

private:
    void requestExit(bool stopBackend) {
        ++exitRequests_;
        if (stopBackend) {
            ++stopRequests_;
            simulatedBackendAlive_ = false;
        }
        viper_gui_exit::scheduleEventLoopExit();
    }

    Scenario scenario_;
    QAction* keepAction_ = nullptr;
    QAction* stopAction_ = nullptr;
    int closeEvents_ = 0;
    int prompts_ = 0;
    int exitRequests_ = 0;
    int stopRequests_ = 0;
    bool simulatedBackendAlive_ = true;
    bool replyCompleted_ = false;
};

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    Scenario scenario;
    if (std::strcmp(argv[1], "close-keep") == 0)
        scenario = Scenario::CloseKeep;
    else if (std::strcmp(argv[1], "close-stop") == 0)
        scenario = Scenario::CloseStop;
    else if (std::strcmp(argv[1], "close-cancel") == 0)
        scenario = Scenario::CloseCancel;
    else if (std::strcmp(argv[1], "menu-keep") == 0)
        scenario = Scenario::MenuKeep;
    else if (std::strcmp(argv[1], "menu-stop") == 0)
        scenario = Scenario::MenuStop;
    else return 2;

    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    ExitWindow window(scenario);
    window.show();

    QTimer::singleShot(0, &window, [&] {
        window.exercise();
        if (scenario == Scenario::CloseCancel) {
            // A Cancel check proves the GUI did not exit, then the harness
            // ends its own event loop without pretending the user quit.
            QTimer::singleShot(60, &app, [] { QCoreApplication::exit(0); });
        }
    });
    // Bounded failure instead of a hanging CI worker if the exit disappears.
    QTimer::singleShot(2500, &app, [] { QCoreApplication::exit(49); });
    const int code = app.exec();
    if (code != 0) {
        std::fprintf(stderr, "FAIL: Qt event loop returned %d\n", code);
        return 1;
    }
    if (!window.passed()) return 1;
    std::printf("PASS: %s exited without closeEvent re-entry\n", argv[1]);
    return 0;
}
