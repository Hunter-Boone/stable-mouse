#!/usr/bin/env python3
"""Inspect the actual package, including permissions that a NAS may override."""
import io
import subprocess
import sys
import tarfile

payload = subprocess.run(
    ["dpkg-deb", "--fsys-tarfile", sys.argv[1]], check=True, capture_output=True
).stdout
with tarfile.open(fileobj=io.BytesIO(payload)) as archive:
    entries = archive.getmembers()
    unsafe = [entry.name for entry in entries if entry.mode & 0o022]
    if unsafe:
        raise SystemExit(f"Package contains group/world-writable paths: {unsafe}")
    names = {entry.name.removeprefix("./") for entry in entries}
    required = {
        "usr/bin/stable-mouse", "usr/libexec/stable-mouse-input",
        "usr/share/applications/org.stablemouse.StableMouse.desktop",
        "usr/share/doc/stable-mouse/LICENSE",
    }
    if required - names:
        raise SystemExit(f"Package is missing: {required - names}")
print("PASS: Debian package contents and permissions")
