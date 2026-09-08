# Website

The site is plain HTML, CSS, and first-party JavaScript modules with no external
fonts, analytics, build step, or runtime dependencies. GitHub Pages deploys it using
`.github/workflows/pages.yml`. The default address is
https://hunter-boone.github.io/stable-mouse/ and requires no purchased domain.
Installers stay in GitHub Releases instead of the website repository directory.

To preview locally, run `python3 -m http.server 8080 --directory website` and visit
http://localhost:8080. Check both desktop and narrow layouts, keyboard navigation,
FAQ expansion, and download links before publishing changes. Native details/summary
controls work without JavaScript. Interactive links have generous target sizes.

The practice area draws a second cursor and leaves the system cursor untouched.
Its presets, strength, speed, center tracking, and pause controls use a JavaScript
port of the app filter. Clicking marks the simulated click position without
discarding queued motion. Unlike the desktop app's click-tail cancellation, this
keeps the comparison aligned after a click or drag at 100% speed away from edges.
The center tracking indicator reports recognition on each axis, or fallback to
ordinary smoothing. It does not measure tremor severity.
Leaving the area resets the comparison. Touch input keeps normal
page scrolling. No input history is saved or sent anywhere. Browser event timing,
pixel scaling, and OS input processing can differ from the installed app.

Keep `filter.mjs` aligned with `src/filter.h` and `src/center_tracker.h`. Pages CI
compares both implementations before deployment, including when either C++ header
changes. Run the same check locally:

```sh
g++ -std=c++17 -Isrc tests/web/filter_reference.cpp -o /tmp/filter-reference
node tests/web/check-parity.mjs /tmp/filter-reference
```

The workflow also serves the real page to headless Chrome and checks pointer
movement, click/drag alignment, controls, keyboard operation, and layouts down to
320px. A controlled browser clock also replays identical regular and uneven shaking with
center tracking on and off, verifies recognition, and checks that it returns to
ordinary smoothing after movement stops. This is a generated input test, not a
measurement of how well it handles a person's tremor.

The development filter tolerates more variation than release 0.1.2. Keep that
distinction visible until installers containing the updated detector are released.
To repeat with a local Chrome debugging session on port 9227 and the HTTP server
above, run `node tests/web/check-browser.mjs http://localhost:8080/`.

When publishing a new application release, update the version, asset links and
release-note link in index.html together. Keep platform requirements and preview
limitations accurate. Do not present generated cursor tests as clinical results.


## Always-center experiment

Choose **Always track center** under Center method in the browser demo. Selecting
it enables center tracking. Uncheck Track the center of movement to compare with
ordinary smoothing, or choose Recognize shaking to use the development detector.

`always-center.mjs` takes the midpoint of the minimum and maximum recent positions
on each axis and passes changes in that midpoint through the app's ordinary
smoother. It runs continuously without recognition thresholds. The center window
is adjustable from 100 to 600 ms, starting at 250 ms. Stationary samples expire old
extrema too, so deliberate travel and small corrections reach their destination.
Changing the method or window resets the comparison. Zero strength bypasses both
centering and smoothing. This mode is a browser experiment, not an app feature.

Run `node tests/web/check-always-center.mjs` for the algorithm checks. With Balanced
55%, a 250 ms window, and the test's generated uneven movement, RMS was 13.64px for
ordinary smoothing, 3.83px for the detector, and 6.63px for always-centering. A clean
160px reach over 400 ms settled within 2px at 768 ms with ordinary smoothing and
960 ms with always-centering. These illustrate a tradeoff, not clinical usefulness
or a claim that always-centering is better. Longer windows can help slower shaking
but add delay. The browser tests also verify actual control, click/drag, pause,
and reset behavior in this mode.
