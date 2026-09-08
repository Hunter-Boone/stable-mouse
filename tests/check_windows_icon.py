#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Verify the installed executable's native icon resources on Windows."""
import ctypes
from ctypes import wintypes
from pathlib import Path
import struct
import sys


def main():
    executable, icon_path = map(Path, sys.argv[1:])
    icon = icon_path.read_bytes()
    reserved, kind, count = struct.unpack_from('<HHH', icon)
    assert (reserved, kind) == (0, 1)
    source = {}
    for index in range(count):
        w, h, colors, zero, planes, depth, size, offset = struct.unpack_from('<BBBBHHII', icon, 6 + index * 16)
        source[w or 256, h or 256, depth] = icon[offset:offset + size]
    assert {w for w, h, depth in source} == {16, 24, 32, 48, 64, 128, 256}

    kernel = ctypes.WinDLL('kernel32', use_last_error=True)
    kernel.LoadLibraryExW.argtypes = [wintypes.LPCWSTR, wintypes.HANDLE, wintypes.DWORD]
    kernel.LoadLibraryExW.restype = wintypes.HMODULE
    kernel.FindResourceW.argtypes = [wintypes.HMODULE, ctypes.c_void_p, ctypes.c_void_p]
    kernel.FindResourceW.restype = ctypes.c_void_p
    kernel.SizeofResource.argtypes = [wintypes.HMODULE, ctypes.c_void_p]
    kernel.SizeofResource.restype = wintypes.DWORD
    kernel.LoadResource.argtypes = [wintypes.HMODULE, ctypes.c_void_p]
    kernel.LoadResource.restype = ctypes.c_void_p
    kernel.LockResource.argtypes = [ctypes.c_void_p]
    kernel.LockResource.restype = ctypes.c_void_p
    kernel.FreeLibrary.argtypes = [wintypes.HMODULE]
    kernel.FreeLibrary.restype = wintypes.BOOL
    module = kernel.LoadLibraryExW(str(executable.resolve()), None, 2)  # LOAD_LIBRARY_AS_DATAFILE
    assert module, ctypes.WinError(ctypes.get_last_error())
    try:
        def resource(kind, identifier):
            handle = kernel.FindResourceW(module, identifier, kind)
            assert handle, f'Missing Windows resource type {kind}, ID {identifier}'
            size = kernel.SizeofResource(module, handle)
            data = kernel.LockResource(kernel.LoadResource(module, handle))
            assert data and size
            return ctypes.string_at(data, size)

        group = resource(14, 1)  # RT_GROUP_ICON, first application icon
        assert struct.unpack_from('<HHH', group) == (0, 1, count)
        embedded = {}
        for index in range(count):
            w, h, colors, zero, planes, depth, size, identifier = struct.unpack_from('<BBBBHHIH', group, 6 + index * 14)
            data = resource(3, identifier)  # RT_ICON
            assert len(data) == size
            embedded[w or 256, h or 256, depth] = data
        assert embedded == source, 'Embedded icon differs from the existing design'
    finally:
        kernel.FreeLibrary(module)
    print('PASS: installed executable embeds all seven original icon sizes')


if __name__ == '__main__':
    main()
