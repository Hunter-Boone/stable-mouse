# Website

The site is plain HTML and CSS with no external fonts, scripts, analytics, build
step, or runtime dependencies. GitHub Pages deploys this directory using
`.github/workflows/pages.yml`. The default address is
https://hunter-boone.github.io/stable-mouse/ and requires no purchased domain.
Installers stay in GitHub Releases instead of the website repository directory.

To preview locally, run `python3 -m http.server 8080 --directory website` and visit
http://localhost:8080. Check both desktop and narrow layouts, keyboard navigation,
FAQ expansion, and download links before publishing changes. Native details/summary
controls work without JavaScript. Interactive links have generous target sizes.

When publishing a new application release, update the version, asset links and
release-note link in index.html together. Keep platform requirements and preview
limitations accurate. Do not present generated cursor tests as clinical results.
