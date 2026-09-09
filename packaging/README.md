# Application icon

`stable-mouse.svg` is the existing Stable Mouse icon. Windows needs the same
design embedded in the executable as an ICO resource so Explorer and Start menu
shortcuts can display it before the app opens. The installer and uninstaller use
the ICO too.

The checked-in `stable-mouse.ico` contains 16, 24, 32, 48, 64, 128, and 256 pixel
images with transparency. Regenerate it after changing the SVG, using ImageMagick:

```sh
convert -background none -density 384 packaging/stable-mouse.svg -depth 8 \
  -define icon:auto-resize=256,128,64,48,32,24,16 packaging/stable-mouse.ico
```

Normal application builds need no image conversion tools. The Windows packaging
check compares every embedded image with the source ICO and checks the actual
installed Start menu shortcut's icon target.

# Windows installer finish page

The final page offers a checked-by-default **Run Stable Mouse** checkbox. Leaving
it checked launches the installed executable when Finish is clicked. Clearing it
leaves the app closed; silent installs also leave the app closed. CPack's native
NSIS finish-page option resolves the executable under the selected installation
directory, including custom paths with spaces.

`tests/check_windows_finish.py` exercises the actual installer with the checkbox
cleared and selected, verifies the installed app window, and captures the final
page. The Windows build workflow runs this after its silent-install checks.
