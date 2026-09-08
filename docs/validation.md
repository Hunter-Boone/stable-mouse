# Validation

The automated suite checks synthetic motion attenuation, displacement conservation, settling without new events, fractional movement, configuration persistence, UI enable/pause/error paths, the practice target, and Linux login entry creation/removal in a temporary directory. The smoke test opens the actual application without taking control of input.

Tests with a fake backend verify UI behavior only. They do not validate OS mouse interception.

## Local results, September 8, 2026

- Debian 12 x86_64, GCC 12.2, Qt 6.4.2: application and input helper built successfully.
- All three CTest entries passed: filter, app controls/persistence/practice, and application smoke launch.
- Synthetic 8 Hz, 8-unit oscillation at 125 samples/second: RMS fell from 5.657 to 1.383 with Balanced and 0.941 with Strong. These numbers describe a generated test path, not measured hand tremor.
- Opened the app offscreen and inspected its rendered window. Generated a `.deb` and inspected its executable, helper, desktop entry, and dependency metadata.
- Opened the actual app under Xvfb with the X11 Qt platform plugin. Tested single-instance handoff and restart after process termination with isolated settings.
- Extracted the Debian package and launched its executable. Confirmed package directory permissions are 0755 after packaging outside the NAS workspace.
- Physical Linux filtering is untested. This environment has no `uinput` device or available kernel module.
- GitHub Actions built and passed the filter, app, and smoke CTest entries on Windows Server 2022, macOS 14, and Ubuntu 24.04. The workflow also checks packaged launches and, on Windows, real hook activation and emergency pause. Physical-device checks remain pending.
- [Final preview workflow](https://github.com/Hunter-Boone/stable-mouse/actions/runs/34245319581) passed all jobs at source revision `0173c8e3b169445b065bdd727e7ccd66f639aaf7`. Windows ran four CTest entries, installed the NSIS package, and launched the installed app without the build Qt environment. macOS mounted its disk image, launched the packaged app, and verified both arm64 and x86_64 executable slices. Ubuntu checked package contents/permissions and launched the extracted executable.

The Windows native backend test is opt-in with `STABLE_MOUSE_TEST_NATIVE_INPUT=1`. It temporarily installs the actual hook and sends the emergency keyboard shortcut. Run it only on an isolated test desktop. CI enables it; ordinary local CTest runs skip it.

## Hardware release checklist

Run with a keyboard or second mouse available. Record OS/build, mouse model, display scaling, and settings for each run.

- Start paused. Confirm ordinary pointer movement before enabling.
- Enable, compare low and high smoothing, then pause. Check that pause immediately restores normal motion with no jump.
- Move slowly, move quickly, trace circles, and stop. Check drift and response delay.
- Left, right, middle, and side-button clicks. Double-click and scroll in both directions.
- Drag text, resize windows, and drag files. Confirm button release always reaches the application.
- Cross display edges, including negative screen coordinates, different scaling factors, and gaps in display layouts.
- Use the emergency pause while dragging and with another window focused.
- Close to tray, reopen, quit, and launch a second instance.
- Enable and disable login startup. Sign out and back in for both cases.
- Disconnect and reconnect the mouse. Sleep and resume the computer.
- Terminate the app unexpectedly. Confirm the original mouse resumes and no button stays pressed.
- Windows: test normal windows, elevated windows, UAC desktop transitions, remote desktop, and shortcut conflicts.
- macOS: test denied, granted, and revoked Accessibility access, screen locking, and disabled event taps.
- Linux: test X11 and Wayland, denied authentication, helper stdin closure, stopped heartbeats, and the two-button pause gesture.

Do not claim completion of a hardware check from a simulation or successful compilation.
