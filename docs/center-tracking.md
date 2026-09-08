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


## Verified 0.1.2 results, September 8, 2026

[Build a977457](https://github.com/Hunter-Boone/stable-mouse/actions/runs/34254193180)
passed all Windows, macOS and Ubuntu jobs, including packaged launches. The macOS
disk-image job passed on retry after a resource-busy error. The earlier Windows
build exposed missing COM header declarations in the focus test, which were fixed.
A Debian 12 package was also inspected and its extracted app launched locally.
Physical macOS and Linux mouse interception remains untested in this environment.

On a three-monitor Windows workstation, nine coordinated native scenarios and
click/reset/drag/release, restart and emergency-pause checks passed at Strong 85%,
speed 100%, center tracking enabled. Results during the final two seconds:

| Generated case | Time within 12 pixels of the prescribed target |
| --- | ---: |
| 300-pixel span, 4 Hz | 100% |
| 1,200-pixel span, 4 Hz | 77.2% |
| 1,200-pixel span, 2 Hz | 100% |
| 600-pixel span, 1 Hz | 100% |
| Irregular movement on both axes | 35.3% |
| Reach with increasing shaking | 100% |
| Clean reach | 100% |
| Shaking that stops | 100% |
| Fine correction with ongoing shaking | 100% |

The large 4 Hz case reduced RMS movement by 97.3% after warmup, but still had a
40-pixel peak error during that measurement interval. Its prior ordinary-smoothing
run had 27.0% final target dwell. Windows CI was steadier than this workstation
run, with 100% final dwell and a four-pixel peak. The workstation replay input
intervals were regular, with a median 8.06 ms and maximum 9.39 ms. The exact reason
for the remaining native variation has not been established. Do not generalize
the CI result to every mouse or machine.

The test observed one press and one release and 99 pixels of a requested
100-pixel drag. These events were confined to the test window. The focus test
passed in Windows CI and confirmed that the focused button and the control at a
candidate point can differ; no steering was added.

The first manually extracted workstation preview omitted the installer’s plugins
folder and failed to launch. Copying the complete runtime fixed it. The corrected
copy passed a launch check and its actual window was inspected, paused with center
tracking selected. The downloadable installer already contained that folder and
had passed its installation-and-launch check.

## 0.1.3: uneven reversals

The browser demo and 0.1.3 app use a more tolerant detector. The earlier 0.1.2
installers used the stricter rules below. This update does not change ordinary
smoothing.

The old detector required three consecutive supporting reversals. Each new
half-cycle had to be 70–130% as long as the last, and the change in successive
midpoint steps had to stay within 3 pixels or 2% of the swing span. A single
rejection dropped tracking immediately. A retreat of only 1 pixel counted as a
new reversal, so sampling noise could repeatedly interrupt recognition.

The updated detector:

- Requires a 4–12 pixel retreat from a turning point, scaled to the recent span,
  so small backsteps do not count as full reversals.
- Allows a half-cycle to be 40–250% of the previous duration, within the existing
  absolute 30–600 ms interval. Swing spans may be 35–280% of the previous span.
- Allows changes in midpoint steps up to 4 pixels or 30% of the current span.
- Acquires after three supporting reversals following initial observations.
  Confidence builds to five; a rejection subtracts two. An established lock can
  survive one rejection, but two consecutive rejections release it.
- Updates the center only from accepted reversals, moving 30% toward each new
  midpoint. It releases after 2.5 previous half-cycle durations without a reversal,
  capped at 700 ms, then fades toward ordinary smoothing as before.

These are engineering thresholds, not a definition of a person's tremor. Small
motions that do not cross the retreat threshold retain ordinary smoothing.
Intentional oscillation can still engage tracking. More averaging can add delay
when reaching while shaking; matching the midpoint cannot prove intent.

`variance_tests` uses 20 deterministic seeds at 60, 125, 250, and 1,000 Hz, Strong
85%, speed 100%, and a nominal 130-pixel swing span. Timing and amplitude vary
independently per half-cycle. All cases include a subsequent 160-pixel reach and
stationary hold to check release and conserved movement. Mean stationary RMS
across rates and seeds, in pixels:

| Generated input | Previous center detector | Updated detector |
| --- | ---: | ---: |
| Timing varies ±40% | 2.52 | 0.05 |
| Amplitude varies ±40% | 6.71 | 5.59 |
| Both vary independently ±35% | 6.32 | 4.92 |
| Regular wave plus ±2px sampling noise | 1.70 | 0.36 |
| Two superimposed frequencies | 4.03 | 3.22 |

The separate existing two-axis irregular scenario improved RMS from 23.60 pixels
with ordinary smoothing to 22.52 pixels with the updated detector. In the reach
with ongoing shaking scenario, RMS increased from ordinary smoothing's 49.50 to
65.80 pixels over the measured interval, although final two-second target dwell
was 100%. A clean reach remains identical to ordinary smoothing. These generated
results are not clinical validation, and native workstation testing of this
update remains pending.


## 0.1.3: always-center and simpler controls

The app includes the browser's always-center method with its 100–600 ms window.
It continuously finds the midpoint of the recent position range on each axis,
then applies the ordinary smoother. Old positions expire even after movement
stops. There is no recognition gate in this mode. More options contains the time
window and fine strength settings, plus the app's speed and startup settings.
Existing center-tracking settings migrate to Recognize shaking.

The native implementation matched the browser across 256,446 generated output
pairs, including fractional speed, bypass, configuration changes, and reset. Build
[104be54](https://github.com/Hunter-Boone/stable-mouse/actions/runs/34268372745)
passed Windows, macOS, Ubuntu, and packaged-launch checks. A Debian 12 package was
also inspected and launched locally. Both center modes passed Windows generated
input, click/drag, restart, and emergency-pause checks. The new mode remains
untested with a physical mouse on the user's workstation.

Always-center is not uniformly better. At Strong 85% with a 250 ms window, the
Windows CI irregular case had 45.0% final target dwell with always-center versus
20.7% with recognition. The slow 1 Hz case had only 8.8% dwell with always-center
versus 100% with recognition. The clean reach also had more delay with always-center.
These are generated tests, not evidence of clinical usefulness. A longer window
can help slower shaking, with more delay.
