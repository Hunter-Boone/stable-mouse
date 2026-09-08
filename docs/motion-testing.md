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
