#!/usr/bin/env python3
"""Inspect real build artifacts without third-party Python packages.

This checks ELF/HEX/BIN layout, not CPU execution or hardware behavior.
SPDX-License-Identifier: MIT
"""

import argparse
from pathlib import Path
import struct


FLASH_START = 0x80000000
FLASH_END = FLASH_START + 256 * 1024
SRAM_END = 32 * 1024
SHT_NOBITS = 8
SHF_ALLOC = 2
# The tested Atmel binutils uses the historical ID; later tools use EM_AVR32=185.
# See docs/TOOLCHAIN.md and the readelf output recorded in docs/VALIDATION.md.
AVR32_MACHINE_IDS = (0x18AD, 185)


def require(condition, message):
    if not condition:
        raise ValueError(message)


def read_hex(path):
    memory = {}
    base = 0
    finished = False
    for line in path.read_text(encoding="ascii").splitlines():
        require(line.startswith(":"), "Invalid Intel HEX record")
        require(not finished, "Record after HEX EOF")
        record = bytes.fromhex(line[1:])
        require(len(record) == record[0] + 5, "Invalid HEX record length")
        require(sum(record) % 256 == 0, "Invalid HEX checksum")
        address = int.from_bytes(record[1:3], "big")
        kind = record[3]
        payload = record[4:-1]
        if kind == 0:
            for offset, value in enumerate(payload):
                location = base + address + offset
                require(location not in memory, "Overlapping HEX records")
                memory[location] = value
        elif kind == 1:
            require(not payload, "Invalid HEX EOF")
            finished = True
        elif kind in (2, 4):
            require(len(payload) == 2, "Invalid HEX address extension")
            base = int.from_bytes(payload, "big") << (4 if kind == 2 else 16)
        elif kind in (3, 5):
            require(len(payload) == 4, "Invalid HEX start address")
        else:
            raise ValueError("Unknown Intel HEX record type")
    require(finished, "Missing HEX EOF")
    return memory


def verify(directory, startup_check):
    elf = (directory / "firmware.elf").read_bytes()
    require(elf[:7] == b"\x7fELF\x01\x02\x01", "Expected big-endian ELF32")
    header = struct.unpack_from(">HHIIIIIHHHHHH", elf, 16)
    kind, machine, _, entry, phoff, shoff, _, _, phsize, phnum, shsize, shnum, strings = header
    require(kind == 2 and machine in AVR32_MACHINE_IDS and entry == FLASH_START,
            "Expected AVR32 executable with reset entry at flash base")
    require((phsize, shsize) == (32, 40), "Unexpected ELF table format")
    sections = [struct.unpack_from(">10I", elf, shoff + i * shsize) for i in range(shnum)]
    names_section = sections[strings]
    names = elf[names_section[4]:names_section[4] + names_section[5]]

    def section_name(section):
        return names[section[0]:].split(b"\0", 1)[0].decode("ascii")

    by_name = {section_name(section): section for section in sections}
    segments = [struct.unpack_from(">8I", elf, phoff + i * phsize) for i in range(phnum)]
    expected_flash = {}
    for section in sections:
        _, section_type, flags, address, offset, size, _, _, _, _ = section
        if not flags & SHF_ALLOC or size == 0:
            continue
        if section_type == SHT_NOBITS:
            require(0 <= address < address + size <= SRAM_END, "NOBITS outside SRAM")
            continue
        matches = [p for p in segments if p[0] == 1
                   and p[2] <= address and address + size <= p[2] + p[4]
                   and offset - p[1] == address - p[2]]
        require(len(matches) == 1, "Allocated section has no unique file-backed load segment")
        segment = matches[0]
        load = segment[3] + address - segment[2]
        require(FLASH_START <= load < load + size <= FLASH_END, "Load image outside flash")
        for index, value in enumerate(elf[offset:offset + size]):
            require(load + index not in expected_flash, "Overlapping load sections")
            expected_flash[load + index] = value

    require(by_name[".reset"][3] == FLASH_START, "Reset section moved")
    require(by_name[".exception_vectors"][3] % 512 == 0, "EVBA alignment lost")
    stack = by_name[".stack"]
    require(stack[1] == SHT_NOBITS and stack[2] & SHF_ALLOC, "Stack must be allocated NOBITS")
    require((stack[3], stack[5]) == (SRAM_END - 1024, 1024), "Wrong stack reservation")
    for name in (".data", ".bss"):
        section = by_name.get(name)
        if section:
            require(section[3] % 4 == 0 and section[5] % 4 == 0, "Startup word alignment lost")
            require(section[3] + section[5] <= stack[3], "Data overlaps stack")

    # Undefined named global symbols would conceal a missing runtime dependency.
    for section in sections:
        if section[1] == 2:  # SHT_SYMTAB
            for offset in range(section[4] + 16, section[4] + section[5], 16):
                name, _, _, info, _, index = struct.unpack_from(">IIIBBH", elf, offset)
                require(not (name and info >> 4 == 1 and index == 0), "Undefined global symbol")

    hex_memory = read_hex(directory / "firmware.hex")
    require(hex_memory == expected_flash, "HEX differs from ELF load sections")
    binary = (directory / "firmware.bin").read_bytes()
    last = max(expected_flash) + 1
    require(len(binary) == last - FLASH_START, "BIN contains non-flash data or wrong base")
    expected_binary = bytes(expected_flash.get(address, 0) for address in range(FLASH_START, last))
    require(binary == expected_binary, "BIN differs from ELF (including zero-filled gaps)")
    require((directory / "firmware.map").stat().st_size > 0, "Missing/empty linker map")

    if startup_check:
        data = by_name[".data"]
        require(data[3] == 0 and data[5] == 8, "Smoke fixture .data layout changed")
        require(elf[data[4]:data[4] + 7] == bytes((3, 5, 7, 11, 13, 17, 19)),
                "Smoke fixture initializer missing from flash image")
        require(by_name[".bss"][5] >= 11, "Smoke fixture .bss was discarded")

    print(f"PASS {directory}: AVR32 ELF, flash-only HEX/BIN, SRAM layout, no undefined globals")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    parser.add_argument("--startup-check", action="store_true")
    args = parser.parse_args()
    verify(args.directory, args.startup_check)
