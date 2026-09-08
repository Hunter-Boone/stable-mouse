# Motion testing

These are generated engineering inputs, not recordings of people's hands. Pixel
amplitudes do not correspond to a diagnosis or a severity score. Mouse settings,
device resolution and the person's movement all affect the resulting cursor path.
Nothing here adds user statistics or recording to the application.

## Reproduce the larger motion tests

Build and run `motion_benchmark motion-traces.csv`. It prints a CSV summary and
optionally writes sample traces. The same scenarios run in the opt-in Windows
backend test. Both use six seconds per scenario and measure after two seconds of
warmup. All distances are pixels at speed 100%.

- Baseline: 300 pixels peak to peak at 4 Hz.
- Large: 1,200 pixels peak to peak at 4 Hz and at 2 Hz.
- Slow: 600 pixels peak to peak at 1 Hz.
- Irregular: amplitude-modulated mixtures of different frequencies on both axes.
- Reach with shake: a prescribed 300-pixel reach over one second, with shaking
  that increases toward the end of the reach, followed by holding at the target.
- Reach without shake: the same reach to expose the delay filtering adds.

The 1 Hz case is a slow disturbance stress test. These waveforms are not models
validated for Parkinson's disease, MS, or any other condition.

RMS error measures distance from the prescribed intended position. During a reach,
it includes lag as well as residual shaking. The error ratio is not meaningful
for the reach without shaking and is reported as -1. Target dwell is the fraction
of samples within a 12-pixel radius of the target during the final two seconds.
It is a geometric measure, not a click success rate. No mouse buttons are replayed.

For Windows, pause Stable Mouse first, then run `windows_backend_tests.exe` with
`STABLE_MOUSE_TEST_NATIVE_INPUT=1` on a desktop left untouched for about 45 seconds.
The test shows red raw input and blue measured cursor output at half scale.
`STABLE_MOUSE_TEST_TRACE` optionally names a CSV output file. The test restores the
original cursor position when it exits normally, including assertion failures.
Ctrl+Alt+F8 pauses the test backend and causes the replay to stop.

The test executable accepts specially tagged injected motion. The production app
ignores injected motion. Replay exercises the native hook, filtering and cursor
output, but does not establish how a physical device or a person will behave.

## Testing usefulness with people

Tremor can occur during different kinds of activity and can vary over time.
These distinctions are described by [NINDS](https://www.ninds.nih.gov/sites/default/files/2025-05/Tremor.pdf).
Shaking a mouse deliberately helps find software faults, but does not establish
whether the settings help someone with involuntary movement.

A study involving 36 people with MS tested pointing, dragging and double-clicking,
and evaluated both alternative input devices and filtering. This supports testing
actual tasks and comfort, rather than drawing conclusions from cursor smoothness
alone. Its results do not validate Stable Mouse.
[Feys et al., 2001](https://pubmed.ncbi.nlm.nih.gov/11392656/).

For a voluntary usability session, let each participant use their own comfortable
mouse and settings. Offer breaks and let them stop or pause whenever they want.
Compare disabled, Balanced and Strong in different orders to reduce practice
bias. Use the same tasks for each setting:

1. Select large and small targets in the practice tab.
2. Click links and menu items, then double-click a harmless test item.
3. Select text and drag an object in a scratch document.
4. Move between monitors and stop over a small target.

With the participant's agreement, manually note completion time, missed clicks,
dropped drags and whether control feels tiring, delayed or easier. Do not require
medical details or save raw pointer traces. Ask which setting they prefer; a
smoother-looking pointer can still make a task harder. Physical device, button,
monitor-edge and pause checks are also listed in [validation.md](validation.md).

Generate an offline interactive report with:

```sh
python3 tests/render_motion_report.py motion-traces.csv motion-report.html --label "Portable simulation"
```

Use a Windows trace and an explicit Windows label to visualize native results.
The report runs locally in a browser and makes no network requests.

## Expanded results, September 8, 2026

With the unchanged 0.1.1 filter, Strong 85% and speed 100%, the portable simulation
produced the following results after warmup. Reduction is RMS error reduction.

| Input | Reduction | Largest residual distance from target |
| --- | ---: | ---: |
| 300-pixel span, 4 Hz | 95.0% | 7 px |
| 1,200-pixel span, 4 Hz | 94.9% | 30 px |
| 1,200-pixel span, 2 Hz | 82.2% | 106 px |
| 600-pixel span, 1 Hz | 53.5% | 139 px |
| Irregular movement on both axes | 90.4% | 52 px |

A 95% reduction still left the large 4 Hz case inside a 24-pixel-diameter target
only 27% of the final two seconds. The prescribed reach with shaking had 77.4%
less RMS error, including lag, and 54% final target dwell at Strong. Maximum
smoothing increased final target dwell to 98% but made error during the reach
worse. This is why attenuation alone cannot establish usability.

For the clean 300-pixel reach, the filter first came within 12 pixels of the target
88 ms after the intended reach ended at Balanced, 464 ms at Strong, and 744 ms at
Maximum. These are settling measurements for this specific path, not a fixed
latency for every movement.

The first expanded [Windows CI replay](https://github.com/Hunter-Boone/stable-mouse/actions/runs/34250766617)
passed all seven native scenarios, hook restart and the emergency pause check.
Its RMS reductions were 94.7%, 93.3%, 82.1%, 52.9%, and 90.3% for the five stationary
cases above. Native timer scheduling and pixel output can change the measurements
relative to the portable simulation. This run's macOS benchmark exposed an exact
floating-point aggregate comparison in the test; the check now compares each
bypassed output sample directly to the input. Ubuntu tests and packaging passed,
but GitHub rejected the artifact finalization request with HTTP 403.

The follow-up [workflow](https://github.com/Hunter-Boone/stable-mouse/actions/runs/34251051206)
passed every job on Windows, macOS and Ubuntu, including native Windows replay
and packaged launches, at revision `663583d74cfd0d261bce136d229b36694a74be4f`.

A coordinated 42-second replay on the user's Windows workstation with three
monitors also passed all seven scenarios, restart and emergency pause. Strong 85%
and speed 100% produced:

| Input | RMS error reduction | Peak residual error | Final target dwell |
| --- | ---: | ---: | ---: |
| 300-pixel span, 4 Hz | 94.9% | 9 px | 100% |
| 1,200-pixel span, 4 Hz | 94.8% | 40 px | 27.0% |
| 1,200-pixel span, 2 Hz | 82.2% | 110 px | 8.8% |
| 600-pixel span, 1 Hz | 53.5% | 139 px | 6.0% |
| Irregular movement on both axes | 90.2% | 53 px | 36.0% |
| Reach with increasing shake | 76.9% | 144 px | 55.8% |

The clean reach had 50.1 pixels RMS tracking error and 100% final target dwell.
It first entered the 12-pixel target radius 480 ms after the intended reach ended.
The cursor position was restored after replay. No clicks were injected. The local
CSV and interactive report contain generated test inputs and measured replay
output, not the user's hand movement. The application and its installer did not
change during this test expansion.
