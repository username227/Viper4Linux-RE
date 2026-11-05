// Passive X11 startup-notification observer. Does not send any messages.
// Observes _NET_STARTUP_INFO_BEGIN/_NET_STARTUP_INFO on the root window.
#include <X11/Xlib.h>
#include <chrono>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <unordered_map>

int main(int argc, char** argv) {
    if (argc != 3) return 2;
    const std::string id = argv[1];
    const int timeoutSeconds = std::stoi(argv[2]);
    Display* display = XOpenDisplay(nullptr);
    if (!display) {
        std::cerr << "Cannot connect to X11 display\n";
        return 2;
    }
    Window root = DefaultRootWindow(display);
    const Atom first = XInternAtom(display, "_NET_STARTUP_INFO_BEGIN", False);
    const Atom next = XInternAtom(display, "_NET_STARTUP_INFO", False);
    XSelectInput(display, root, PropertyChangeMask);
    XSync(display, False);
    std::cout << "READY\n" << std::flush;
    std::unordered_map<Window, std::string> messages;
    const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(timeoutSeconds);
    while (std::chrono::steady_clock::now() < until) {
        while (XPending(display)) {
            XEvent event{};
            XNextEvent(display, &event);
            if (event.type != ClientMessage || event.xclient.format != 8) continue;
            if (event.xclient.message_type != first && event.xclient.message_type != next) continue;
            std::string& message = messages[event.xclient.window];
            if (event.xclient.message_type == first) message.clear();
            const char* data = event.xclient.data.b;
            const void* end = std::memchr(data, '\0', 20);
            message.append(data, end ? static_cast<const char*>(end) - data : 20);
            if (end) {
                std::cout << "EVENT " << message << "\n" << std::flush;
                if (message.rfind("remove:", 0) == 0 && message.find(id) != std::string::npos) {
                    std::cout << "COMPLETED\n" << std::flush;
                    XCloseDisplay(display);
                    return 0;
                }
                messages.erase(event.xclient.window);
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    std::cout << "TIMEOUT\n" << std::flush;
    XCloseDisplay(display);
    return 0;
}
