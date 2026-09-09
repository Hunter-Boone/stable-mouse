#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Exercise the packaged NSIS finish checkbox, unchecked and checked, on Windows."""
import ctypes
from ctypes import wintypes
import json
import os
from pathlib import Path
import subprocess
import sys
import time

user = ctypes.WinDLL('user32', use_last_error=True)
callback_type = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
user.EnumWindows.argtypes = [callback_type, wintypes.LPARAM]
user.EnumChildWindows.argtypes = [wintypes.HWND, callback_type, wintypes.LPARAM]
user.GetWindowTextW.argtypes = [wintypes.HWND, wintypes.LPWSTR, ctypes.c_int]
user.GetClassNameW.argtypes = [wintypes.HWND, wintypes.LPWSTR, ctypes.c_int]
user.IsWindowVisible.argtypes = [wintypes.HWND]
user.IsWindowEnabled.argtypes = [wintypes.HWND]
user.GetWindowThreadProcessId.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.DWORD)]
user.SendMessageW.argtypes = [wintypes.HWND, wintypes.UINT, wintypes.WPARAM, wintypes.LPARAM]
user.SendMessageW.restype = ctypes.c_ssize_t
user.PostMessageW.argtypes = user.SendMessageW.argtypes
user.GetWindowRect.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.RECT)]


def label(hwnd, class_name=False):
    value = ctypes.create_unicode_buffer(2048)
    (user.GetClassNameW if class_name else user.GetWindowTextW)(hwnd, value, len(value))
    return value.value.replace('&', '')


def windows(parent=None):
    result = []
    @callback_type
    def collect(hwnd, _):
        if user.IsWindowVisible(hwnd):
            result.append(hwnd)
        return True
    if parent is None:
        user.EnumWindows(collect, 0)
    else:
        user.EnumChildWindows(parent, collect, 0)
    return result


def wait_for(check, timeout=30):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        value = check()
        if value:
            return value
        time.sleep(.2)
    raise AssertionError('Timed out waiting for installer/app state')


def click(hwnd):
    assert user.IsWindowEnabled(hwnd), label(hwnd)
    assert user.PostMessageW(hwnd, 0x00F5, 0, 0)  # BM_CLICK, asynchronously
    time.sleep(.4)


def app_processes():
    output = subprocess.check_output(['powershell', '-NoProfile', '-Command',
        '@(Get-Process -Name stable-mouse -ErrorAction SilentlyContinue | '
        'Where-Object { $_.Path -eq $env:STABLE_MOUSE_FINISH_APP } | '
        'Select-Object -ExpandProperty Id) | ConvertTo-Json -Compress'], text=True).strip()
    value = json.loads(output) if output else []
    return value if isinstance(value, list) else [value]


def capture(hwnd, path):
    rect = wintypes.RECT()
    assert user.GetWindowRect(hwnd, ctypes.byref(rect))
    env = dict(os.environ, STABLE_MOUSE_FINISH_CAPTURE=str(path))
    subprocess.run(['powershell', '-NoProfile', '-Command',
        'Add-Type -AssemblyName System.Drawing; '
        f'$b = New-Object System.Drawing.Bitmap({rect.right-rect.left}, {rect.bottom-rect.top}); '
        '$g = [System.Drawing.Graphics]::FromImage($b); '
        f'$g.CopyFromScreen({rect.left}, {rect.top}, 0, 0, $b.Size); '
        '$b.Save($env:STABLE_MOUSE_FINISH_CAPTURE, [System.Drawing.Imaging.ImageFormat]::Png); '
        '$g.Dispose(); $b.Dispose()'], check=True, env=env)


def main():
    installer, root, screenshot = map(lambda p: Path(p).resolve(), sys.argv[1:])
    os.environ['STABLE_MOUSE_FINISH_APP'] = str(root / 'bin/stable-mouse.exe')
    assert not app_processes(), 'Close the app before testing the installer'
    for run_after in (False, True):
        # NSIS requires /D last and its path unquoted, including spaces.
        command = subprocess.list2cmdline([str(installer)]) + ' /D=' + str(root)
        process = subprocess.Popen(command)
        try:
            dialog = wait_for(lambda: next((w for w in windows() if label(w, True) == '#32770'
                and 'Stable Mouse' in label(w) and 'Setup' in label(w)), None))
            deadline = time.monotonic() + 90
            while time.monotonic() < deadline:
                controls = windows(dialog)
                run_box = next((w for w in controls if label(w) == 'Run Stable Mouse' and label(w, True) == 'Button'), None)
                if run_box:
                    break
                buttons = [w for w in controls if label(w, True) == 'Button' and user.IsWindowEnabled(w)
                           and label(w) in ('Next >', 'I Agree', 'Install')]
                if buttons:
                    click(buttons[0])
                else:
                    time.sleep(.2)
            else:
                raise AssertionError('Finish checkbox missing; visible controls: ' + repr([label(w) for w in windows(dialog)]))
            assert user.SendMessageW(run_box, 0x00F0, 0, 0) == 1, 'Run must be checked by default'
            assert not app_processes(), 'App launched before Finish'
            if run_after:
                capture(dialog, screenshot)
            else:
                click(run_box)
                assert user.SendMessageW(run_box, 0x00F0, 0, 0) == 0
            finish = next(w for w in windows(dialog) if label(w) == 'Finish' and label(w, True) == 'Button')
            click(finish)
            assert process.wait(timeout=30) == 0
            if run_after:
                pids = wait_for(app_processes)
                def visible_app():
                    for hwnd in windows():
                        pid = wintypes.DWORD()
                        user.GetWindowThreadProcessId(hwnd, ctypes.byref(pid))
                        if pid.value in pids and label(hwnd) == 'Stable Mouse':
                            return True
                    return False
                wait_for(visible_app)
                print('PASS: checked Run Stable Mouse opens the installed application window')
            else:
                time.sleep(2)
                assert not app_processes(), 'Unchecked Run launched the app'
                print('PASS: unchecked Run Stable Mouse leaves the application closed')
        finally:
            if process.poll() is None:
                subprocess.run(['taskkill', '/PID', str(process.pid), '/T', '/F'], check=False)
            for pid in app_processes():
                subprocess.run(['taskkill', '/PID', str(pid), '/F'], check=True)


if __name__ == '__main__':
    main()
