#!/usr/bin/env python3
# Copyright (C) 2026 The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
"""Bind the exact legacy Widevine blob to Lineage's versioned protobuf ABI."""
import argparse
import hashlib
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile

ORIGINAL = "f128342e572418785eff44a39b37b86f2899c3cc5777d87ea2a31717d4f25aa6"
PATCHED = "c3278d96f47c37218b3a00f346e1c9962a1a64ce3bce129e0117213216860384"
OLD = "libprotobuf-cpp-lite.so"
NEW = "libprotobuf-cpp-lite-v29.so"

def elf_info(data):
    assert data[:6] == b"\x7fELF\x01\x01"
    h = struct.unpack_from("<16sHHIIIIIHHHHHH", data)
    assert h[1:3] == (3, 40), "Expected ARM ELF32 shared library"
    sections = [struct.unpack_from("<10I", data, h[6] + i*h[11]) for i in range(h[12])]
    names = sections[h[13]]
    strings = data[names[4]:names[4]+names[5]]
    named = {strings[s[0]:].split(b"\0", 1)[0].decode(): s for s in sections}
    dynamic = named[".dynamic"]
    tags = [struct.unpack_from("<iI", data, offset)
            for offset in range(dynamic[4], dynamic[4]+dynamic[5], 8)]
    dynstr = named[".dynstr"]
    table = data[dynstr[4]:dynstr[4]+dynstr[5]]
    needed = [table[value:].split(b"\0", 1)[0].decode() for tag,value in tags if tag == 1]
    # Compare all executable sections, unwind data and packed relocations.
    protected = {name:data[s[4]:s[4]+s[5]] for name,s in named.items()
                 if s[2]&4 or name in (".ARM.exidx", ".ARM.extab", ".android.rel.dyn", ".rel.dyn", ".rel.plt", ".init_array", ".fini_array", ".rodata")}
    return needed,protected

def fixup(source, target, tool):
    original = source.read_bytes()
    digest = hashlib.sha256(original).hexdigest()
    if digest not in (ORIGINAL, PATCHED):
        raise SystemExit("Unknown Widevine blob SHA256: " + digest)
    if digest == PATCHED:
        assert NEW in elf_info(original)[0] and OLD not in elf_info(original)[0]
        if source != target: shutil.copy2(source, target)
        return
    before, protected = elf_info(original)
    assert before.count(OLD) == 1 and NEW not in before
    with tempfile.TemporaryDirectory(prefix="widevine-protobuf-") as directory:
        candidate = Path(directory)/source.name
        shutil.copy2(source, candidate)
        subprocess.run([tool, "--replace-needed", OLD, NEW, str(candidate)], check=True)
        patched = candidate.read_bytes()
        after, result = elf_info(patched)
        assert after == [NEW if name == OLD else name for name in before]
        assert protected == result, "Executable, constants, unwind or relocations changed"
        assert hashlib.sha256(patched).hexdigest() == PATCHED, "Use patchelf 0.17.2 for reproducible output"
        shutil.copy2(candidate, target)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", nargs="?", type=Path)
    parser.add_argument("--patchelf", default=os.environ.get("PATCHELF", "patchelf"))
    args = parser.parse_args()
    fixup(args.source, args.output or args.source, args.patchelf)
