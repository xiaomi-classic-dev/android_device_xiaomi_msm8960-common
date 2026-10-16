#!/usr/bin/env python3
# Copyright (C) 2026 The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
"""Backport cleanup tolerance to the exact Aries vendor executable.

Later netmgr_kif_reset_link implementations tolerate initialization failure for
both NETMGR_MODEM_IS_EV and NETMGR_INVALID_EV. Our vendor version only accepts
the former, and aborts when its exit callback probes absent rmnet_usb ports.

In this exact ELF, the event argument is only used for that failure policy.
Select its existing tolerant policy at the cleanup callsite. Real interface
resets and all other reset callers retain their original implementation.

Reference: netmgr_kif.c, lines 7018-7024, revision
f242858a8cbb2c9ad9b5b2e53d75380ae775727a in
https://github.com/bcyj/android_tools_leeco_msm8996
"""

import argparse
import hashlib
from pathlib import Path
import stat
import struct

ORIGINAL = "00776c0bfb23d41ef332724d498fffccd9a7502c79984435624088b4563e145b"
PATCHED = "f2057979bc3b1affa9333b863733add3be6cd54788da5c1ae8adee5aa34128fc"


def fixup(source, target):
    data = bytearray(source.read_bytes())
    digest = hashlib.sha256(data).hexdigest()
    if digest not in (ORIGINAL, PATCHED):
        raise SystemExit("Unsupported netmgrd SHA256; refusing to patch unknown firmware: " + digest)
    if digest == PATCHED and source == target:
        return
    if digest == ORIGINAL:
        # ELF32 ARM, with the code segment mapped at file offset zero.
        assert data[:6] == b"\x7fELF\x01\x01" and struct.unpack_from("<H", data, 18)[0] == 40
        header = struct.unpack_from("<16sHHIIIIIHHHHHH", data)
        segments = [struct.unpack_from("<8I", data, header[5] + i * header[9])
                    for i in range(header[10])]
        assert any(p[0] == 1 and p[1] == p[2] == 0 and
                   p[4] > 0xc540 and p[6] & 1 for p in segments)
        assert data[0xc530:0xc540] == bytes.fromhex("0024204611210134fff730ff222cf8d1")
        assert data[0xc3ea:0xc3f0] == bytes.fromhex("b9f1040f0fd1")
        # Cleanup only: Thumb movs r1,#17 -> movs r1,#4.
        data[0xc534] = 4
        assert hashlib.sha256(data).hexdigest() == PATCHED
    target.write_bytes(data)
    target.chmod(stat.S_IMODE(source.stat().st_mode))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", nargs="?", type=Path,
                        help="Output path; omit to fix the extracted blob in place")
    args = parser.parse_args()
    fixup(args.source, args.output or args.source)
