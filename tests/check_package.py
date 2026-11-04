#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Static packaging contract; this is not an AVR32 firmware build."""
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PLATFORM = ROOT / 'arduino/hardware/uc3bduino/avr32'
version = (ROOT / 'VERSION').read_text().strip()
match = re.fullmatch(r'(0)\.(\d+)\.(\d+)-(?:dev|rc[1-9]\d*)', version)
assert match, 'Expected a development or RC version'
assert f'version={version}\n' in (PLATFORM / 'platform.txt').read_text()
assert f'version={version}\n' in (PLATFORM / 'libraries/UC3BduinoExamples/library.properties').read_text()
header = (PLATFORM / 'cores/uc3bduino/UC3BduinoVersion.h').read_text()
assert f'#define UC3BDUINO_VERSION "{version}"' in header
for name, number in zip(('MAJOR', 'MINOR', 'PATCH'), match.groups()):
    assert f'#define UC3BDUINO_VERSION_{name} {number}\n' in header
paths = (ROOT / 'packaging/source-files.txt').read_text().splitlines()
assert len(paths) == len(set(p.lower() for p in paths)), 'Duplicate source entries'
assert 'packaging/source-files.txt' in paths
for path in paths:
    assert (ROOT / path).is_file(), path
    (ROOT / path).read_text(encoding='utf-8')  # All packaged sources must be UTF-8.
    assert not (ROOT / path).is_symlink(), path
    assert re.fullmatch(r'(?:[A-Za-z0-9_.-]+/)*[A-Za-z0-9_.-]+', path), path
    assert not re.search(r'(^|/)(\.\.?|\.git|\.scratch|dist|build)(/|$)|\.local\.txt$|\.(exe|dll|o|elf|hex|bin|a|zip|pyc)$', path), path
if (ROOT / '.git').exists():
    tracked = set(subprocess.check_output(['git', 'ls-files'], cwd=ROOT, text=True).splitlines())
    assert not tracked - set(paths), 'Tracked source missing from manifest: ' + str(tracked - set(paths))
print(f'PASS: packaging metadata {version}, {len(paths)} source files (no firmware build)')
