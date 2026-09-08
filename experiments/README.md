# Reversal midpoint experiment

The original reversal prototypes below remain offline experiments. Version 0.1.2
adds a revised optional implementation described in [center tracking](../docs/center-tracking.md).
The comparison program now also runs that implementation as `center_tracking_preview`.

This code is an offline research prototype. It is not linked into Stable Mouse
and is not included in the application installers. It keeps no user data.

The current filter smooths all motion with two low-pass stages. That leaves some
oscillation and delays intentional movement. This experiment tests the user's
suggestion to estimate the middle of repeated opposing turning points.

## Reproduce

From the repository root:

```sh
c++ -O2 -std=c++17 -Isrc -Itests experiments/compare_reversals.cpp -o /tmp/compare-reversals
/tmp/compare-reversals /tmp/reversal-traces.csv > /tmp/reversal-summary.csv
python3 tests/render_motion_report.py /tmp/reversal-traces.csv /tmp/reversal-comparison.html --label "Experimental portable simulation"
```

The original comparison used four algorithms on 14 generated paths at 125 samples per second:

- Current Strong smoothing, with its existing time constant.
- Midpoint estimation after alternating reversals, with faster fallback smoothing.
- The same midpoint estimator with a six-pixel hold radius around the output.
- Current Strong smoothing plus midpoint estimation only after several consistent
  reversal intervals, spans and center changes. This is labeled
  `Strong_plus_gated_midpoint` in the CSV.

All versions use speed 100%. The experiment measures continuous filter output,
without pixel rounding or a native OS backend. It reports tracking RMS after two
seconds, time within 12 pixels of the prescribed target during seconds four to
six, startup peak error, largest eight-millisecond output step, and peak error
during seconds four to six. The report's "strength" selector contains algorithm
names because it reuses the existing trace format. No result is a medical score
or a measured click success rate.

The added cases include a 12-pixel intentional nudge, a 20-pixel nudge with shaking,
changing amplitude, changing frequency, slow deliberate corrections, shaking
that stops, and an intentional path identical to one of the unwanted paths.
That last pair demonstrates an information limit: the same input cannot reveal
whether the user wanted that motion.

## Results, September 8, 2026

| Generated case | Current Strong RMS error | Simple midpoint RMS error | Gated midpoint RMS error |
| --- | ---: | ---: | ---: |
| 1,200-pixel span at 4 Hz | 21.76 px | 0.73 px | 0.73 px |
| Irregular movement on both axes | 23.60 px | 72.02 px | 23.60 px |
| Reach with increasing shake | 49.50 px | 24.81 px | 48.74 px |
| Clean reach | 48.38 px | 12.33 px | 48.38 px |
| 20-pixel nudge with shaking | 11.27 px | 1.47 px | 1.47 px |

The gated midpoint version held the large regular 4 Hz path within one pixel of
the target throughout the final two seconds. Current Strong stayed within the
12-pixel target radius for only 25.5% of that interval. Repeated midpoint estimates
can therefore address residual regular shaking without increasing the low-pass
time constant.

The simple faster version had much worse acquisition behavior. For that same
large shake, its startup peak error was 354 pixels, versus 84 pixels with Current
Strong and with the gated version. Irregular reversals also moved its estimated
center around, increasing tracking error roughly threefold. A six-pixel hold
radius did not solve that failure and introduced an offset in regular cases.

The conservative version preserves the existing delay during a clean reach.
It also needs several reversals before using the midpoint; the slow 1 Hz case
only locks after more than two seconds. When the shake stopped, returning to the
fallback caused a 26.4-pixel output step in eight milliseconds, versus 3.6 pixels
with Current Strong. This mode transition needs work before live cursor use.

## Original experiment decision

Keep the released algorithm unchanged. Continue investigating a confidence-gated
center estimate, but do not present this prototype as a working replacement.
Before any native trial, eliminate jumps when entering or leaving center tracking,
check acquisition and release across sample rates and intermittent input, and
verify click reset, drag, pause, monitor-edge behavior and deliberate fine
corrections. A faster response during intentional movement needs separate evidence;
large movement alone is not a reliable signal to reduce filtering.

This experiment addresses the same broad discrimination problem described in
[TechFilter, Rocon et al., 2006](https://journals.sagepub.com/doi/10.3233/TAD-2006-18101).
It does not implement that paper's algorithm or inherit its user-validation results.
