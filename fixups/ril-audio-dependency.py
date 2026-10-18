#!/usr/bin/env python3
"""Retarget the known Qualcomm RIL's AudioSystem dependency on Android 16."""

import argparse
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ORIGINAL_SHA256 = "cb12c4b4e7c0de24831567d95cd1ddb394a4258b7313660bc82dae2b01b087bf"
PATCHED_SHA256 = "92a457517bf279720f3a4db974149678601dcd8ef7a3e8a575907ed57a6e613f"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("blob", type=Path)
    parser.add_argument("--patchelf", required=True)
    args = parser.parse_args()
    digest = hashlib.sha256(args.blob.read_bytes()).hexdigest()
    if digest == PATCHED_SHA256:
        print("Qualcomm RIL AudioSystem dependency already repaired")
        return
    if digest != ORIGINAL_SHA256:
        raise SystemExit("Refusing unknown Qualcomm RIL blob: " + digest)

    with tempfile.TemporaryDirectory(dir=str(args.blob.parent)) as temp:
        target = Path(temp) / args.blob.name
        shutil.copy2(str(args.blob), str(target))
        subprocess.run([
            args.patchelf, "--page-size", "4096", "--replace-needed",
            "libmedia.so", "libaudioclient.so", str(target),
        ], check=True)
        if hashlib.sha256(target.read_bytes()).hexdigest() != PATCHED_SHA256:
            raise SystemExit("Unexpected patchelf output; original blob preserved")
        os.replace(str(target), str(args.blob))
    print("Qualcomm RIL now directly imports libaudioclient.so")


if __name__ == "__main__":
    main()
