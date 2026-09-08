# Third-party software

Stable Mouse links dynamically to Qt. Qt is copyright The Qt Company Ltd. and other contributors. The application uses Qt Core, GUI, Widgets, Network, and platform plugins. The build uses Qt Test for tests. Open source Qt modules used here are available under LGPL-3.0 or GPL-3.0 terms as applicable. The LGPL-3.0 text is included in `LICENSES/LGPL-3.0.txt`; GPL-3.0 is in `LICENSE`.

Windows and macOS workflow builds use Qt 6.8.3. The complete corresponding Qt source, including its individual third-party notices and license files, is available from [Qt's 6.8.3 source archive](https://download.qt.io/archive/qt/6.8/6.8.3/single/qt-everywhere-src-6.8.3.tar.xz). Preserve these notices with redistributed builds. Users may replace the shared Qt libraries with compatible versions.

Linux packages use system Qt and libevdev rather than bundling them. libevdev is MIT licensed. Distribution packages provide the library licenses and corresponding source through their package repositories. Local Debian 12 builds use Qt 6.4.2 and libevdev 1.13.0; workflow Linux builds use the Ubuntu 24.04 packages.

Review the exact deployed Qt plugins and their notices before a public binary release. The current workflow produces unsigned development artifacts.
