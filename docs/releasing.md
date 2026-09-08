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
