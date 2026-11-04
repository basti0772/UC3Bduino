"""Validate an isolated Boards Manager install with real Windows executables."""
import argparse
import contextlib
import functools
import hashlib
import http.server
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import threading

ROOT = Path(__file__).resolve().parents[1]
FQBN = "uc3bduino:avr32:uc3b0256"
INDEX = "package_uc3bduino_index.json"
PUBLIC = f"https://raw.githubusercontent.com/basti0772/UC3Bduino/boards-manager/{INDEX}"
VERSION = (ROOT / "VERSION").read_text().strip()


@contextlib.contextmanager
def index_url(assets, work):
    if assets is None:
        yield PUBLIC
        return
    mirror = work / "mirror"
    mirror.mkdir()
    meta = json.loads((assets / INDEX).read_bytes())
    platform = meta["packages"][0]["platforms"][0]
    name = platform["archiveFileName"]
    shutil.copyfile(assets / name, mirror / name)
    handler = functools.partial(http.server.SimpleHTTPRequestHandler, directory=mirror)
    with http.server.ThreadingHTTPServer(("127.0.0.1", 0), handler) as server:
        address = f"http://127.0.0.1:{server.server_port}"
        platform["url"] = f"{address}/{name}"
        (mirror / INDEX).write_text(json.dumps(meta), encoding="utf-8")
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        try:
            yield f"{address}/{INDEX}"
        finally:
            server.shutdown()
            thread.join()


def validate(work, url):
    data = work / "data"
    config = work / "cli.yaml"
    settings = {"directories": {"data": str(data), "user": str(work / "sketchbook"),
                                 "downloads": str(work / "downloads")},
                "build_cache": {"path": str(work / "cache")}}
    config.write_text(json.dumps(settings), encoding="utf-8")
    env = {k: v for k, v in os.environ.items() if not k.startswith("ARDUINO_")}

    def run(args, name):
        with (work / (name + ".log")).open("w", encoding="utf-8") as log:
            result = subprocess.run(args, stdout=log, stderr=subprocess.STDOUT, env=env, timeout=450)
        output = (work / (name + ".log")).read_text(encoding="utf-8", errors="replace")
        if result.returncode:
            raise RuntimeError(output[-4500:])
        print(f"PASS {name}", flush=True)
        return output

    cli = ["arduino-cli", "--config-file", str(config)]
    extra = ["--additional-urls", url]
    run(cli + ["core", "update-index"] + extra, "index")
    run(cli + ["core", "install", f"uc3bduino:avr32@{VERSION}"] + extra, "install")
    if FQBN not in run(cli + ["board", "listall", "UC3Bduino"] + extra, "board-list"):
        raise RuntimeError("Installed board missing")
    meta = json.loads((data / INDEX).read_bytes())["packages"][0]
    platform = meta["platforms"][0]
    archive = work / "downloads/packages" / platform["archiveFileName"]
    if hashlib.sha256(archive.read_bytes()).hexdigest() != platform["checksum"].split(":")[1]:
        raise RuntimeError("Installed archive checksum mismatch")
    installed = data / "packages/uc3bduino/hardware/avr32" / VERSION
    powershell = ["powershell.exe", "-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass", "-File"]
    tool = work / "external-dfu/dfu-programmer.exe"
    env.pop("UC3B_DFU_TOOL_PATH", None)
    run(powershell + [str(installed / "tools/setup-boards-manager.ps1"),
                      "-DfuInstallDirectory", str(tool.parent)], "compiler-setup")
    if "dfu-programmer 1.1.0" not in run([str(tool), "--version"], "dfu-version"):
        raise RuntimeError("Unexpected installed DFU version")
    for name in ("ArduinoMinimal", "Blink", "USBSerialBlink", "USBSerialEcho", "AnalogRead", "SPILoopback"):
        build = work / "build" / name
        sketch = installed / "libraries/UC3BduinoExamples/examples" / name
        run(cli + ["compile", "--fqbn", FQBN, "--build-path", str(build), str(sketch)] + extra, name)
        run([sys.executable, str(ROOT / "tests/verify_firmware.py"), str(build),
             "--basename", name + ".ino"], name + "-layout")
    # Exercise the installed IDE upload recipe including its resolved tool path.
    # Invoke the installed entry point directly so no CLI override can drop CheckOnly.
    props = run(cli + ["compile", "--fqbn", FQBN + ":dfu=confirmed", "--show-properties=expanded",
                       str(installed / "libraries/UC3BduinoExamples/examples/Blink")] + extra, "properties")
    pattern = next(line.split("=", 1)[1] for line in props.splitlines()
                   if line.startswith("tools.uc3bdfu.upload.pattern="))
    if "AT32UC3B0256" not in pattern or str(tool.parent).replace("\\", "/").lower() not in pattern.replace("\\", "/").lower():
        raise RuntimeError("IDE recipe did not resolve confirmed target and installed DFU tool")
    uploader = installed / "tools/upload-dfu.ps1"
    if str(uploader).replace("\\", "/").lower() not in pattern.replace("\\", "/").lower():
        raise RuntimeError("IDE recipe did not resolve the installed uploader")
    args = powershell + [str(uploader), "-CheckOnly", "-Firmware", str(work / "build/Blink/Blink.ino.hex"),
                         "-ToolPath", str(tool), "-LegacyDevice", "AT32UC3B0256"]
    if "CHECK ONLY" not in run(args, "dfu-recipe-preflight"):
        raise RuntimeError("Expected DFU preflight result")
    print(f"Installed test completed: {url}", flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("work", type=Path, help="New isolated test directory")
    parser.add_argument("--local-assets", type=Path, help="Serve a local preview of the release archive")
    args = parser.parse_args()
    work = args.work.resolve()
    work.mkdir(parents=True, exist_ok=False)
    with index_url(args.local_assets, work) as url:
        validate(work, url)
