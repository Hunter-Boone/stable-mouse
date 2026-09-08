#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Build uncommitted UI changes locally on Windows without GitHub Actions.

Needs CMake, VS 2019 C++ build tools, and Qt 6.4.3 msvc2019_64 (qtbase/qtsvg).
Uses a local disk build directory even when this checkout is on a network share.
Does not launch filtering or replace a running preview.
"""
import argparse
from datetime import datetime
import os
from pathlib import Path
import shutil
import subprocess


def main():
    if os.name != "nt":
        raise SystemExit("Run this script with Windows Python.")
    home = Path(os.environ["LOCALAPPDATA"]) / "StableMouse"
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--qt", type=Path, default=home / "Qt/6.4.3/msvc2019_64")
    args = parser.parse_args()
    assert (args.qt / "bin/Qt6Widgets.dll").is_file(), args.qt
    repo = Path(__file__).resolve().parents[1]
    work = home / "local-build"
    source = work / "source"
    build = work / "build"
    source.mkdir(parents=True, exist_ok=True)
    for name in ("src", "tests", "packaging", "docs", "LICENSES"):
        shutil.copytree(repo / name, source / name, dirs_exist_ok=True)
    for name in ("CMakeLists.txt", "LICENSE", "README.md", "THIRD_PARTY.md"):
        shutil.copy2(repo / name, source / name)
    env = os.environ.copy()
    env["PATH"] = str(args.qt / "bin") + os.pathsep + env["PATH"]
    env.pop("QT_PLUGIN_PATH", None)
    env.pop("STABLE_MOUSE_TEST_NATIVE_INPUT", None)
    env["QT_QPA_PLATFORM"] = "offscreen"

    def run(*command):
        print("Running:", subprocess.list2cmdline([str(c) for c in command]), flush=True)
        subprocess.run([str(c) for c in command], check=True, cwd=work, env=env)

    run("cmake", "-S", source, "-B", build, "-G", "Visual Studio 16 2019", "-A", "x64",
        f"-DCMAKE_PREFIX_PATH={args.qt}")
    run("cmake", "--build", build, "--config", "Release", "--target", "stable-mouse", "app_tests", "--parallel", "4")
    run(build / "Release/app_tests.exe")
    preview = home / ("Preview-local-" + datetime.now().strftime("%Y%m%d-%H%M%S"))
    run("cmake", "--install", build, "--config", "Release", "--prefix", preview)
    # Smoke-test the complete deployed runtime without the SDK on PATH.
    env["PATH"] = os.environ["PATH"]
    env.pop("QT_QPA_PLATFORM", None)
    run(preview / "bin/stable-mouse.exe", "--smoke-test")
    run(preview / "bin/stable-mouse.exe", "--screenshot", preview / "preview.png")
    (home / "latest-local-preview.txt").write_text(str(preview), encoding="utf-8")
    print("PREVIEW_READY=" + str(preview), flush=True)


if __name__ == "__main__":
    main()
