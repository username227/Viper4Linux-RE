#pragma once

// When a GUI close event is deliberately ignored (so the user can cancel
// quitting), QApplication::quit() can ask Qt to close every window again.
// That re-enters closeEvent() and can repeat the backend-exit prompt forever.
// Exit the event loop directly instead. Keep the zero-delay deferral so a
// GUI-control socket callback can finish sending its reply before shutdown.
//
// This is Qt event-loop policy only; it does not manage the audio backend.
#include <QCoreApplication>
#include <QTimer>

namespace viper_gui_exit {

inline void scheduleEventLoopExit() {
    QTimer::singleShot(0, QCoreApplication::instance(), [] {
        QCoreApplication::exit(0);
    });
}

} // namespace viper_gui_exit
