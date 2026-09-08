# Creating a downloadable release

1. Run the build workflow on the source revision to distribute. Download all three platform artifacts and the source archive. A workflow artifact is a preview build, not a tested public release.
2. Complete the hardware checklist in `validation.md` on each supported OS. Record actual device and OS versions. Resolve pointer loss, stuck buttons, or emergency-pause failures before release.
3. Install and uninstall each package on a clean machine without Qt or build tools. Verify login on/off across a real sign-out and sign-in. Disable login startup before uninstalling this preview.
4. For Windows public distribution, configure Authenticode signing for the app and installer using the project owner's signing identity. Test downloaded files with Windows security prompts. Unsigned builds may show warnings.
5. For macOS public distribution, sign the application and its bundled frameworks with a Developer ID, enable the hardened runtime, notarize the disk image, and staple the ticket. Check Accessibility permission using the final signed app. Signing credentials must be configured separately.
6. Publish installer files, SHA-256 checksums, release notes with actual support limits, and the matching source archive. Include Qt license notices and corresponding source obligations for the exact Qt build distributed. Qt is dynamically linked under its open source terms; its source is separate from this application's source archive.

Source is hosted at https://github.com/Hunter-Boone/stable-mouse. The workflow uploads artifacts without automatically creating a public release. Signing credentials are not configured. Keep build, packaged-launch, and physical-device test results separate in release notes.

Platform references:

- [Qt deployment](https://doc.qt.io/qt-6/deployment.html)
- [Windows low-level mouse hooks](https://learn.microsoft.com/en-us/windows/win32/winmsg/lowlevelmouseproc)
- [Windows SendInput restrictions](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-sendinput)
- [Apple event taps](https://developer.apple.com/documentation/coregraphics/cgevent/tapcreate(tap:place:options:eventsofinterest:callback:userinfo:))
- [Linux uinput](https://docs.kernel.org/input/uinput.html)
- [Desktop login startup](https://specifications.freedesktop.org/autostart/0.5/)

## In-app updates

Use the CMake project version as the release number, for example `0.1.4`,
and publish the GitHub tag `v0.1.4`. Bump `project(VERSION ...)` for each release.
Keep development-preview status in the release notes and website copy; regular
releases use plain version numbers and are not marked as GitHub prereleases.
The updater can therefore offer the next regular release to installed users.

The optional `STABLE_MOUSE_RELEASE_VERSION` CMake setting and the build workflow's
`release_version` input allow an explicit version override. Leave them empty for
normal releases. The numeric components must match the CMake project version.

The updater reads the 100 most recent published GitHub releases and compares
semantic versions. Preview builds accept previews and stable releases; stable
builds skip previews. Draft releases are ignored. Publish each release only after
all installers have finished uploading.

Keep these asset names, which CPack generates for the supported release builds:

- `Stable-Mouse-0.1.4-Windows-x64.exe`
- `Stable-Mouse-0.1.4-macOS-universal.dmg`
- `stable-mouse-0.1.4-Debian12-amd64.deb`
- `stable-mouse-0.1.4-Ubuntu24.04-amd64.deb`

Replace `0.1.4` with the package version. macOS must contain both architectures.
Linux packages must be built on their named distribution. Generic CPack asset
names are not selected automatically. Unsupported systems receive a link to the
release page instead of an installer for another platform.

Each asset must have a `sha256:` digest in the GitHub Releases API. The updater
requires HTTPS, a download URL in this repository, a matching platform filename,
an expected size no larger than 512 MiB, and a valid digest. It checks the size and
checksum after download and again before opening the file. A missing digest
leaves the release available through its download page. Checksums from GitHub
protect download integrity; configure platform signing as described above.

Installation is interactive. Windows opens the NSIS installer, macOS opens the
disk image, and Linux asks the desktop to open the package installer. The app
releases mouse input before the handoff and quits only if opening succeeds.
If opening fails, it remains paused and shows the downloaded file's path. Installers
that have been opened stay in the application's cache so quitting does not remove
a file still needed by the installer. Cancelled or failed downloads are removed.

Run `ctest --test-dir build --output-on-failure` for deterministic updater tests.
For an opt-in check that reads GitHub, downloads the current platform's published
installer, and verifies its checksum without executing it:

```sh
QT_QPA_PLATFORM=offscreen STABLE_MOUSE_TEST_LIVE_UPDATER=1 ./build/updater_tests livePublishedRelease
```

Before publishing, test an actual upgrade from an older installed build on each
platform. Check UAC or package authorization, custom Windows install directories,
macOS replacement and Accessibility permission, settings preservation, and
reopening the new version. The automated handoff tests use a fake launcher and
do not establish that native installation completed.
