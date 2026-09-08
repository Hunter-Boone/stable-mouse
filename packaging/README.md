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
