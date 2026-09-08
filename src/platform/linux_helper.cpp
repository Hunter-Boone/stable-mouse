// SPDX-License-Identifier: GPL-3.0-only
// Narrow privileged process: one validated relative mouse, no shell, no files
// written, and no network. Closing stdin or missing heartbeats releases the grab.
#include "filter.h"
#include <libevdev/libevdev.h>
#include <libevdev/libevdev-uinput.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <array>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <string>
#include <sstream>
#include <thread>

namespace {
volatile sig_atomic_t interrupted = 0;
void interrupt(int) { interrupted = 1; }
using Clock = std::chrono::steady_clock;
struct Device {
    int fd = -1;
    libevdev *input = nullptr;
    libevdev_uinput *output = nullptr;
    std::array<bool, KEY_CNT> down{};
    ~Device() {
        if (output) {
            for (int key = BTN_MOUSE; key < KEY_CNT; ++key)
                if (down[key]) libevdev_uinput_write_event(output, EV_KEY, key, 0);
            libevdev_uinput_write_event(output, EV_SYN, SYN_REPORT, 0);
        }
        if (input) libevdev_grab(input, LIBEVDEV_UNGRAB);
        if (output) libevdev_uinput_destroy(output);
        if (input) libevdev_free(input);
        if (fd >= 0) close(fd);
    }
    bool send(unsigned type, unsigned code, int value) { return libevdev_uinput_write_event(output, type, code, value) == 0; }
};
int fail(const char *message) { std::cerr << message << '\n'; return 1; }
}
int main(int argc, char **argv) {
    if ((argc != 4 && argc != 5)) return fail("Usage: stable-mouse-input /dev/input/eventN strength speed [centerTracking]");
    const std::string path = argv[1];
    const std::string prefix = "/dev/input/event";
    if (path.compare(0, prefix.size(), prefix) != 0 || path.size() == prefix.size() ||
        path.find_first_not_of("0123456789", prefix.size()) != std::string::npos)
        return fail("Choose a /dev/input/eventN device.");
    char *end1 = nullptr, *end2 = nullptr;
    FilterConfig config{std::strtod(argv[2], &end1), std::strtod(argv[3], &end2)};
    if (*end1 || *end2 || !std::isfinite(config.strength) || !std::isfinite(config.speed)) return fail("Invalid filter settings.");
    if (argc == 5) {
        if (std::string(argv[4]) != "0" && std::string(argv[4]) != "1") return fail("Invalid center tracking setting.");
        config.centerTracking = std::string(argv[4]) == "1";
    }
    Device device;
    device.fd = open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC | O_NOFOLLOW);
    struct stat statbuf{};
    if (device.fd < 0 || fstat(device.fd, &statbuf) != 0 || !S_ISCHR(statbuf.st_mode)) return fail("Could not open the selected mouse.");
    if (libevdev_new_from_fd(device.fd, &device.input) < 0) return fail("Could not read mouse capabilities.");
    if (!libevdev_has_event_code(device.input, EV_REL, REL_X) || !libevdev_has_event_code(device.input, EV_REL, REL_Y) ||
        !libevdev_has_event_code(device.input, EV_KEY, BTN_LEFT) || libevdev_has_event_type(device.input, EV_ABS))
        return fail("Only relative mice are supported. Touchpads and tablets are not supported yet.");
    for (int key = 0; key < KEY_CNT; ++key)
        if ((key < BTN_MOUSE || key > BTN_TASK) && libevdev_has_event_code(device.input, EV_KEY, key))
            return fail("This device also exposes keyboard or non-mouse controls. Select a mouse-only device.");
    if (std::string(libevdev_get_name(device.input)).find("Stable Mouse") == 0) return fail("Cannot filter a virtual Stable Mouse device.");
    libevdev_set_name(device.input, "Stable Mouse filtered pointer");
    if (libevdev_uinput_create_from_device(device.input, LIBEVDEV_UINPUT_OPEN_MANAGED, &device.output) < 0)
        return fail("Could not create the virtual mouse. Load the uinput kernel module and try again.");
    // Give the desktop time to enumerate the replacement before grabbing input.
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    for (int key = BTN_MOUSE; key <= BTN_TASK; ++key)
        if (libevdev_get_event_value(device.input, EV_KEY, key))
            return fail("Release the mouse buttons and enable stabilization again.");
    if (libevdev_grab(device.input, LIBEVDEV_GRAB) < 0) return fail("Another application is already using this mouse exclusively.");
    std::signal(SIGTERM, interrupt); std::signal(SIGINT, interrupt); std::signal(SIGHUP, interrupt); std::signal(SIGPIPE, SIG_IGN);
    fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) | O_NONBLOCK);
    Stabilizer filter; filter.configure(config);
    auto last = Clock::now(), heartbeat = last, bothSince = last;
    bool bothHeld = false;
    std::string commands;
    std::cout << "READY" << std::endl;
    while (!interrupted) {
        pollfd fds[] = {{STDIN_FILENO, POLLIN, 0}, {device.fd, POLLIN, 0}};
        const int polled = poll(fds, 2, 8);
        if (polled < 0 && errno != EINTR) return fail("Input polling failed.");
        const auto now = Clock::now();
        if (now - heartbeat > std::chrono::seconds(2)) return fail("The app stopped responding. Mouse released.");
        if (fds[0].revents & (POLLHUP | POLLERR | POLLNVAL)) break;
        if (fds[1].revents & (POLLHUP | POLLERR | POLLNVAL)) return fail("The mouse was disconnected.");
        if (fds[0].revents & POLLIN) {
            char buffer[512]; const auto n = read(STDIN_FILENO, buffer, sizeof(buffer));
            if (n == 0) break;
            if (n > 0) commands.append(buffer, size_t(n));
            if (commands.size() > 4096) return fail("Invalid control message.");
            size_t newline;
            while ((newline = commands.find('\n')) != std::string::npos) {
                auto line = commands.substr(0, newline); commands.erase(0, newline + 1);
                if (line == "STOP") return 0;
                if (line == "PING") heartbeat = now;
                if (line.rfind("CONFIG ", 0) == 0) {
                    std::istringstream values(line.substr(7));
                    if (!(values >> config.strength >> config.speed)) return fail("Invalid filter settings.");
                    int center = 0;
                    values >> std::ws;
                    if (!values.eof()) {
                        if (!(values >> center) || (center != 0 && center != 1)) return fail("Invalid center tracking setting.");
                        values >> std::ws;
                        if (!values.eof()) return fail("Invalid filter settings.");
                    }
                    config.centerTracking = center == 1;
                    filter.configure(config);
                }
            }
        }
        // Bound each batch so a noisy device cannot starve the watchdog.
        for (int i = 0; i < 256; ++i) {
            input_event event{};
            const int result = libevdev_next_event(device.input, LIBEVDEV_READ_FLAG_NORMAL, &event);
            if (result == -EAGAIN) break;
            if (result != LIBEVDEV_READ_STATUS_SUCCESS) return fail("Mouse events were lost. Filtering paused; enable it again to reconnect.");
            if (event.type == EV_REL && event.code == REL_X) filter.add(event.value, 0);
            else if (event.type == EV_REL && event.code == REL_Y) filter.add(0, event.value);
            else {
                if (event.type == EV_KEY && event.code < KEY_CNT) {
                    device.down[event.code] = event.value != 0;
                    if (event.value == 1) filter.reset();
                }
                if (!device.send(event.type, event.code, event.value)) return fail("Could not forward a mouse event.");
            }
        }
        const auto movement = filter.pixels(std::chrono::duration<double>(now - last).count()); last = now;
        if ((movement.x && !device.send(EV_REL, REL_X, int(movement.x))) ||
            (movement.y && !device.send(EV_REL, REL_Y, int(movement.y))) || !device.send(EV_SYN, SYN_REPORT, 0))
            return fail("Could not move the virtual pointer.");
        const bool both = device.down[BTN_LEFT] && device.down[BTN_RIGHT];
        if (both && !bothHeld) bothSince = now;
        bothHeld = both;
        if (both && now - bothSince >= std::chrono::seconds(2)) { std::cout << "PAUSED" << std::endl; break; }
    }
    return 0;
}
