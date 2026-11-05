#pragma once
// Complete X11 startup when no new top-level window is mapped:
// an existing-instance handoff or a successful cold start hidden in the tray.
// Visible first-launch notification remains Qt's responsibility; Wayland uses
// native xdg-activation-v1 rather than this X11-specific completion.
#include <QByteArray>
#include <QGuiApplication>

#ifdef VIPER_HAVE_X11
#include <X11/Xlib.h>
#include <algorithm>
#include <cstring>
#include <string>
#endif

namespace viper_startup {
inline bool completeX11Startup(const QByteArray& startupId) {
#ifdef VIPER_HAVE_X11
    if (startupId.isEmpty() || startupId.size() > 4096 ||
        QGuiApplication::platformName() != QLatin1String("xcb"))
        return false;

    auto* native = static_cast<QGuiApplication*>(QCoreApplication::instance())->nativeInterface<QNativeInterface::QX11Application>();
    Display* display = native ? reinterpret_cast<Display*>(native->display()) : nullptr;
    if (!display) return false;

    std::string message = "remove: ID=\"";
    for (char ch : startupId) {
        if (ch == '"' || ch == '\\') message += '\\';
        message += ch;
    }
    message += '"';

    const Window root = DefaultRootWindow(display);
    const Window sender = XCreateSimpleWindow(display, root, 0, 0, 1, 1, 0, 0, 0);
    const Atom first = XInternAtom(display, "_NET_STARTUP_INFO_BEGIN", False);
    const Atom follow = XInternAtom(display, "_NET_STARTUP_INFO", False);
    if (!sender || !first || !follow) {
        if (sender) XDestroyWindow(display, sender);
        return false;
    }

    bool sent = true;
    const auto nbytes = message.size() + 1;  // include terminating NUL
    for (size_t offset = 0; offset < nbytes; offset += 20) {
        XEvent event{};
        event.xclient.type = ClientMessage;
        event.xclient.display = display;
        event.xclient.window = sender;
        event.xclient.message_type = offset == 0 ? first : follow;
        event.xclient.format = 8;
        const auto count = std::min(size_t{20}, nbytes - offset);
        std::memcpy(event.xclient.data.b, message.data() + offset, count);
        if (!XSendEvent(display, root, False, PropertyChangeMask, &event))
            sent = false;
    }
    XFlush(display);
    XDestroyWindow(display, sender);
    XFlush(display);
    return sent;
#else
    Q_UNUSED(startupId);
    return false;
#endif
}
}  // namespace viper_startup
