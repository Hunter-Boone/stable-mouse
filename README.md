# Stable Mouse

A free native desktop application for adjustable mouse stabilization. Licensed under GPL-3.0-only. No accounts, telemetry, advertising, or movement history.

**Status: 0.1.0 development preview.** Windows is the first hardware testing priority. Platform backends and installer recipes are implemented, but a passing build is not proof that filtering works on a physical mouse. See [validation](docs/validation.md) for what has actually been tested and what remains.

## Controls

- Enable or pause stabilization from the window or tray.
- Choose Light, Balanced, or Strong smoothing, or adjust strength from 0 to 100.
- Adjust pointer speed from 25% to 200%.
- Practice moving and clicking targets without saving movement data.
- Open the app at login, and separately choose whether stabilization starts enabled.

The first launch is paused and login startup is off. Settings stay on your computer. Closing the window keeps the app in the tray when available. Quit releases input. If there is no system tray, closing the window stops the app.

Smoothing introduces delay. Start with Balanced and adjust for comfort. This software does not measure a medical condition or promise a particular result.

## Platform support

| Platform | Implementation | Current limits |
| --- | --- | --- |
| Windows | Low-level mouse hook on a dedicated thread, filtered pointer output, Ctrl+Alt+F8 emergency pause | Needs hardware testing. Raw-input games may bypass filtering. Protected administrator windows may reject output and cause filtering to pause. |
| macOS | Core Graphics event tap, filtered movement and drag events, Control+Option+F8 emergency pause | Needs macOS build and hardware testing. Accessibility permission required. |
| Linux | Selected relative mouse through evdev/uinput, separate privileged helper | Needs physical-device testing under both X11 and Wayland. Requires `pkexec`, an authentication agent, and the `uinput` kernel module. No touchpads, tablets, or combined keyboard/mouse interfaces yet. |

On Linux, hold left and right mouse buttons together for two seconds to pause. The helper releases the mouse if the app disconnects or stops sending heartbeats for two seconds. Your keyboard and other mice are not grabbed. Authentication is requested each time filtering starts, including at login if you opted into automatic enabling. No broad input-device permission rules are installed.

“All OSs” here means Windows, macOS, and Linux desktops. Mobile operating systems, BSD, ChromeOS, remote desktops, and specialized input devices need separate integrations or testing.

## Build

Requires CMake 3.22+, a C++17 compiler, and Qt 6.4+ with Widgets, Network, and Test. Qt remains dynamically linked.

On Debian 12 or Ubuntu with Qt 6.4+:

```sh
sudo apt install cmake ninja-build g++ qt6-base-dev qt6-svg-dev libevdev-dev pkexec
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
./build/stable-mouse
```

On Windows, use a Visual Studio 2022 developer shell with matching MSVC Qt 6 installed. On macOS, use Xcode command line tools and Qt 6. Set `CMAKE_PREFIX_PATH` to your Qt installation if CMake cannot find it.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
cd build
cpack -C Release
```

Windows packaging needs NSIS on PATH. CPack produces an NSIS `.exe` installer, a macOS `.dmg`, or a Debian `.deb`. Windows and macOS packages include deployed Qt libraries. The Debian package uses distribution dependencies; build it against the oldest distribution you intend to support. It is not a universal Linux package.

The [build workflow](.github/workflows/build.yml) builds and uploads preview installers and a corresponding source archive. It does not publish releases. Windows/macOS signing and Apple notarization are not configured; see [release steps](docs/releasing.md).

## Filtering

The shared filter accumulates relative input and drains it with a time-based low-pass response. Large pending movements reduce the time constant so long movements catch up faster. Residual motion settles even after input stops. Fractional movement is preserved at low speeds. A button press clears the remaining tail to keep the click at the visible pointer position. Dragging continues to be filtered.

This is a heuristic, not a classifier of intentional motion. Synthetic oscillation tests check attenuation and movement conservation. They cannot establish usability for a person with tremor. OS acceleration and device differences mean the same settings may feel different across platforms.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md). Contributions are licensed under GPL-3.0-only. Please include the OS version, mouse model, display arrangement, settings, and reproduction steps for input bugs. Do not include medical records or private screen recordings.
