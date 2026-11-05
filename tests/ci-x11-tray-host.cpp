// Test-only X11 system-tray owner for the actual Qt GUI (XEmbed protocol).
// Claims _NET_SYSTEM_TRAY_Sn so start-in-tray does not fall back to showing
// the main window in virtual Xvfb sessions. Never installs any runtime code.
#include <X11/Xlib.h>
#include <chrono>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    Display* display = XOpenDisplay(nullptr);
    if (!display) return 3;
    const int screen = DefaultScreen(display);
    const Window root = RootWindow(display, screen);
    Window owner = XCreateSimpleWindow(display, root, 0, 0, 24, 24, 0, 0, 0);
    if (!owner) return 4;
    const Atom selection = XInternAtom(display,
        ("_NET_SYSTEM_TRAY_S" + std::to_string(screen)).c_str(), False);
    const Atom opcode = XInternAtom(display, "_NET_SYSTEM_TRAY_OPCODE", False);
    const Atom manager = XInternAtom(display, "MANAGER", False);
    XSelectInput(display, owner, StructureNotifyMask);
    XSetSelectionOwner(display, selection, owner, CurrentTime);
    XSync(display, False);
    if (XGetSelectionOwner(display, selection) != owner) return 5;
    XEvent notice{};
    notice.xclient.type = ClientMessage;
    notice.xclient.window = root;
    notice.xclient.message_type = manager;
    notice.xclient.format = 32;
    notice.xclient.data.l[0] = CurrentTime;
    notice.xclient.data.l[1] = static_cast<long>(selection);
    notice.xclient.data.l[2] = static_cast<long>(owner);
    XSendEvent(display, root, False, StructureNotifyMask, &notice);
    XFlush(display);
    std::cout << "READY" << std::endl;
    bool docked = false;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
    while (std::chrono::steady_clock::now() < deadline) {
        while (XPending(display)) {
            XEvent event{};
            XNextEvent(display, &event);
            // _NET_SYSTEM_TRAY_REQUEST_DOCK has opcode 0.
            if (event.type == ClientMessage &&
                event.xclient.message_type == opcode &&
                event.xclient.format == 32 && event.xclient.data.l[1] == 0) {
                Window icon = static_cast<Window>(event.xclient.data.l[2]);
                XReparentWindow(display, icon, owner, 0, 0);
                XMapWindow(display, owner);
                XMapWindow(display, icon);
                XFlush(display);
                std::ofstream(argv[1]) << "DOCK\n";
                docked = true;
                std::cout << "DOCK" << std::endl;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    XDestroyWindow(display, owner);
    XCloseDisplay(display);
    return docked ? 0 : 6;
}
