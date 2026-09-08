# Center tracking preview

Version 0.1.2 adds an optional setting, "Track the center of repeated shaking".
It is off by default. Enable it in Pointer settings to try it, with Strong and
speed 100% as the tested starting point. Pause with the usual emergency shortcut.

The filter looks for several similar, alternating reversals on each axis. When
those support a consistent center estimate, it fades toward the midpoint of
opposing turning points. When the pattern breaks, it fades back to ordinary
smoothing. Magnitude alone never enables a faster response. It does not inspect
applications, snap to buttons, learn a medical profile, or store motion history.

The fades replace the discontinuous mode change in the earlier experiment. The
center estimate also follows the fallback while unlocked, so it cannot pull the
cursor toward a stale center when tracking resumes. Estimates use relative
coordinates and reset on click or pause. Ordinary button and drag forwarding
continue through the existing OS backends.

## Limits

- It takes several reversals to recognize a pattern. Slow shaking takes longer.
- Irregular shaking can fall back to ordinary smoothing and retain its residual
  movement. This mode does not fix every difficulty with small targets.
- A clean reach keeps the ordinary filter's delay. This version targets steadiness
  during repeated shaking, not faster response during every movement.
- Intentional oscillating motion can look identical to unwanted shaking. Disable
  center tracking for drawing or whenever it interferes with intended movement.
- It cannot infer an intended button from cursor movement alone.

## Test design

Portable checks cover 60, 125, 250 and 1,000 updates per second, holding a small
stationary target, release after shaking stops, irregular input fallback, a
12-pixel correction, click/pause reset, speed fractions and bypass transitions.
The native Windows replay also exercises large and slow shaking, irregular motion,
reaching, shaking that stops, and fine corrections with ongoing shaking. A separate
sequence presses, drags and releases only in the replay's own test window.

Set both `STABLE_MOUSE_TEST_NATIVE_INPUT=1` and `STABLE_MOUSE_TEST_CENTER=1` for
native center-tracking replay. Leave the mouse untouched for about one minute and
pause the installed app first. `STABLE_MOUSE_TEST_TRACE` optionally names a local
CSV of the generated replay. These settings affect the test executable only.

## Focus as an intent clue

Windows UI Automation exposes both the element with input focus and the element
at a supplied screen point. These answer different questions. A focused edit box
or the last clicked button can retain focus while the person aims at another
control. A rectangle also does not guarantee that every point in it is clickable.
[Microsoft: focused element](https://learn.microsoft.com/en-us/windows/win32/api/uiautomationclient/nf-uiautomationclient-iuiautomation-getfocusedelement),
[Microsoft: element at a point](https://learn.microsoft.com/en-us/dotnet/api/system.windows.automation.automationelement.frompoint).

`windows_focus_tests` creates two harmless buttons, focuses the first and queries
the second by its screen point on a separate COM thread. It checks that the answers
differ. It neither moves the cursor nor activates either button. The production
app has no UI Automation code and does not read other applications' controls.

A future target-assistance mode could consider an enabled control near the
estimated movement center, require sustained approach, and release when moving
away. It should be optional, avoid text selection and dragging, reject stale or
ambiguous targets, and never click automatically. Keyboard focus alone is not a
sufficient reason to move the pointer. This is a design proposal, not an enabled
feature or a tested claim about usefulness.
