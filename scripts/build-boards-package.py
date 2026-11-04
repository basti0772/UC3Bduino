#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Build the Arduino Boards Manager distribution from reviewed source files."""
import argparse
import hashlib
import json
import re
from pathlib import Path
from zipfile import ZIP_STORED, ZipFile, ZipInfo

ROOT = Path(__file__).resolve().parents[1]
REPO = "https://github.com/basti0772/UC3Bduino"
TEMPLATE = "arduino/hardware/uc3bduino/avr32/"
CORE = "cores/uc3bduino/"
TOOLS = {
    "scripts/dfu-common.ps1", "scripts/dfu-usb.cs", "scripts/upload-dfu.ps1",
    "scripts/installation-common.ps1", "scripts/setup-boards-manager.ps1",
    "scripts/check-boards-compiler.ps1", "scripts/get-dfu-tool.ps1",
}
DOCS = {"INSTALL_ARDUINO_WINDOWS", "THIRD_PARTY_NOTICES", "THIRD_PARTY", "DEVICE_HEADERS",
        "DFU_UPLOAD", "USB_CDC", "TOOLCHAIN", "SPI", "WIRE", "BOARD_REFERENCE"}


def destination(path):
    if path.startswith(TEMPLATE):
        return path[len(TEMPLATE):]
    if path.startswith(("src/hal/", "src/usb/", "src/startup/")):
        return CORE + path[len("src/"):]
    if path == "linker/at32uc3b0256.ld":
        return "system/at32uc3b0256.ld"
    if path in TOOLS:
        return "tools/" + Path(path).name
    if path.startswith("examples/") and (ROOT / Path(path).parent / (Path(path).parent.name + ".ino")).is_file():
        return "libraries/UC3BduinoExamples/" + path
    if path in {"LICENSE", "VERSION"} or (path.startswith("docs/") and Path(path).stem in DOCS):
        return path
    return None


def assemble(version):
    files = {}
    for path in (ROOT / "packaging/source-files.txt").read_text().splitlines():
        dest = destination(path)
        if dest is None:
            continue
        source = ROOT / path
        if source.is_symlink() or dest in files or ".." in Path(dest).parts:
            raise ValueError(f"Invalid platform entry {path}")
        files[dest] = source.read_text(encoding="utf-8").encode()
    platform = files["platform.txt"].decode()
    if f"version={version}\n" not in platform:
        raise ValueError("Platform version mismatch")
    platform = platform.replace("{tools.uc3bdfu.legacy_device}", "{upload.legacy_device}")
    platform += ('\nbuild.usb_flags=\nrecipe.hooks.prebuild.1.pattern.windows=powershell.exe '
                 '-NoProfile -NonInteractive -ExecutionPolicy Bypass -File '
                 '"{runtime.platform.path}/tools/check-boards-compiler.ps1" '
                 '-Compiler "{compiler.path}{compiler.c.cmd}"\n')
    files["platform.txt"] = platform.encode()
    boards = files["boards.txt"].decode().replace("-DUC3B_OSC0_CRYSTAL=1\n",
                                                 "-DUC3B_OSC0_CRYSTAL=1 {build.usb_flags}\n")
    boards += ("\nmenu.dfu=DFU target identification\n"
               "uc3b0256.menu.dfu.standard=Automatic signature check\n"
               "uc3b0256.menu.dfu.standard.upload.legacy_device=Unconfirmed\n"
               "uc3b0256.menu.dfu.confirmed=Confirmed AT32UC3B0256 marking (legacy DFU)\n"
               "uc3b0256.menu.dfu.confirmed.upload.legacy_device=AT32UC3B0256\n")
    files["boards.txt"] = boards.encode()
    return files


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def build(out):
    version = (ROOT / "VERSION").read_text().strip()
    if not re.fullmatch(r"0\.\d+\.\d+-rc[1-9]\d*", version):
        raise ValueError("Expected a release candidate version")
    out.mkdir(parents=True, exist_ok=True)
    archive = out / f"uc3bduino-avr32-{version}.zip"
    with ZipFile(archive, "w") as package:
        for dest, data in sorted(assemble(version).items()):
            entry = ZipInfo(f"uc3bduino-avr32-{version}/{dest}", (2000, 1, 1, 0, 0, 0))
            entry.create_system = 3
            entry.external_attr = 0o100644 << 16
            package.writestr(entry, data, compress_type=ZIP_STORED)
    platform = {"name": "UC3Bduino", "architecture": "avr32", "version": version,
                "category": "Contributed", "url": f"{REPO}/releases/download/v{version}/{archive.name}",
                "archiveFileName": archive.name, "size": str(archive.stat().st_size),
                "checksum": "SHA-256:" + digest(archive), "boards": [{"name": "UC3Bduino AT32UC3B0256"}],
                "toolsDependencies": []}
    index = {"packages": [{"name": "uc3bduino", "maintainer": "UC3Bduino contributors",
                           "websiteURL": REPO, "help": {"online": REPO + "/issues"},
                           "platforms": [platform], "tools": []}]}
    indexfile = out / "package_uc3bduino_index.json"
    indexfile.write_bytes((json.dumps(index, indent=2) + "\n").encode())
    assets = [archive, indexfile]
    for name in ("INSTALL_ARDUINO_WINDOWS.md", "RELEASE_NOTES.md"):
        artifact = out / name
        artifact.write_bytes((ROOT / "docs" / name).read_text(encoding="utf-8").encode())
        assets.append(artifact)
    (out / "SHA256SUMS.txt").write_bytes("".join(f"{digest(p)}  {p.name}\n" for p in assets).encode())
    print(f"{archive.name}: {archive.stat().st_size} bytes, SHA-256 {digest(archive)}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    build(parser.parse_args().output)
