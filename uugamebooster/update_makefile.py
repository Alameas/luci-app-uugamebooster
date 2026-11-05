#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only

import hashlib
import json
import os
from pathlib import Path
import re
import sys
import tempfile
from urllib.error import URLError
from urllib.request import urlopen


DIRECTORY = Path(__file__).resolve().parent
TEMPLATE = DIRECTORY / "Makefile.template"
MAKEFILE = DIRECTORY / "Makefile"
VERSION_URL = "https://router.uu.163.com/api/plugin?type=openwrt-x86_64"
ARCHIVE_URL = "https://uurouter.gdl.netease.com/uuplugin/{type}/v{version}/uu.tar.gz?"
ARCHITECTURES = {
    "aarch64": "h3c-nx30pro",
    "arm": "h3c-be18000",
    "mipsel": "jd-hr06",
}
PLACEHOLDER = re.compile(r"@[A-Z0-9_]+@")


def get_version():
    with urlopen(VERSION_URL, timeout=30) as response:
        url = json.load(response)["url"]
    match = re.search(r"/v([0-9]+(?:\.[0-9]+)*)/uu\.tar\.gz(?:\?|$)", url)
    if not match:
        raise ValueError("Cannot determine the UU version from the API response")
    return match.group(1)


def get_hash(device_type, version, archive=None):
    digest = hashlib.sha256()
    url = ARCHIVE_URL.format(type=device_type, version=version)
    with urlopen(url, timeout=30) as response:
        if archive is None:
            while True:
                chunk = response.read(1024 * 1024)
                if not chunk:
                    break
                digest.update(chunk)
        else:
            with archive.open("wb") as output:
                while True:
                    chunk = response.read(1024 * 1024)
                    if not chunk:
                        break
                    digest.update(chunk)
                    output.write(chunk)
    return digest.hexdigest()


def generate(version=None, cache_dir=None):
    template = TEMPLATE.read_text(encoding="utf-8")
    required = {"@VERSION@"}
    for arch in ARCHITECTURES:
        required.update({f"@TYPE_{arch.upper()}@", f"@HASH_{arch.upper()}@"})
    placeholders = PLACEHOLDER.findall(template)
    if set(placeholders) != required or len(placeholders) != len(required):
        raise ValueError("Makefile.template placeholders do not match the configured architectures")

    if version is None:
        version = get_version()
    values = {"@VERSION@": version}
    for arch, device_type in ARCHITECTURES.items():
        print(f"Downloading {device_type} {version}...", flush=True)
        values[f"@TYPE_{arch.upper()}@"] = device_type
        archive = cache_dir / f"{device_type}.tar.gz" if cache_dir else None
        values[f"@HASH_{arch.upper()}@"] = get_hash(device_type, version, archive)

    result = PLACEHOLDER.sub(lambda match: values.get(match.group(), match.group()), template)
    remaining = PLACEHOLDER.findall(result)
    if remaining:
        raise ValueError(f"Unknown template placeholders: {', '.join(remaining)}")

    mode = MAKEFILE.stat().st_mode & 0o777 if MAKEFILE.exists() else 0o644
    with tempfile.NamedTemporaryFile("w", encoding="utf-8", dir=DIRECTORY, delete=False) as output:
        temporary = Path(output.name)
        try:
            output.write(result)
            os.chmod(temporary, mode)
            output.close()
            os.replace(temporary, MAKEFILE)
        finally:
            temporary.unlink(missing_ok=True)
    print(f"Updated Makefile to {version}")


if __name__ == "__main__":
    try:
        generate()
    except (OSError, URLError, ValueError, KeyError, TypeError) as error:
        print(f"Update failed: {error}", file=sys.stderr)
        sys.exit(1)
