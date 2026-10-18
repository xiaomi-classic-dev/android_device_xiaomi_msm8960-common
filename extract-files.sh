#!/bin/bash
#
# Copyright (C) 2016 The CyanogenMod Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

set -e


# Load extractutils and do some sanity checks
MY_DIR="${BASH_SOURCE%/*}"
if [[ ! -d "$MY_DIR" ]]; then MY_DIR="$PWD"; fi

MK_ROOT="$MY_DIR"/../../..

HELPER="$MK_ROOT"/vendor/lineage/build/tools/extract_utils.sh
if [ ! -f "$HELPER" ]; then
    echo "Unable to find helper script at $HELPER"
    exit 1
fi
. "$HELPER"

# Default to sanitizing the vendor folder before extraction
CLEAN_VENDOR=true

while [ "$1" != "" ]; do
    case $1 in
        -n | --no-cleanup )     CLEAN_VENDOR=false
                                ;;
        -s | --section )        shift
                                SECTION=$1
                                CLEAN_VENDOR=false
                                ;;
        * )                     SRC=$1
                                ;;
    esac
    shift
done

if [ -z "$SRC" ]; then
    SRC=adb
fi

# Initialize the helper for common device
setup_vendor "$DEVICE_COMMON" "$VENDOR" "$MK_ROOT" true "$CLEAN_VENDOR"

extract "$MY_DIR"/common-proprietary-files.txt "$SRC" "$SECTION"

# AudioSystem moved out of libmedia; preserve the callback ABI preload shim.
RIL_BLOB="$MK_ROOT/vendor/$VENDOR/$DEVICE_COMMON/proprietary/vendor/lib/libril-qc-qmi-1.so"
if [ -f "$RIL_BLOB" ]; then
    python3 "$MY_DIR/fixups/ril-audio-dependency.py" "$RIL_BLOB" \
        --patchelf "$MK_ROOT/prebuilts/extract-tools/linux-x86/bin/patchelf-0_18"
fi

# Reapply the firmware-specific exit cleanup correction after extraction.
NETMGRD_BLOB="$MK_ROOT/vendor/$VENDOR/$DEVICE_COMMON/proprietary/vendor/bin/netmgrd"
if [ -f "$NETMGRD_BLOB" ]; then
    python3 "$MY_DIR/fixups/netmgrd-cleanup.py" "$NETMGRD_BLOB"
fi

WIDEVINE_BLOB="$MK_ROOT/vendor/$VENDOR/$DEVICE_COMMON/proprietary/vendor/lib/mediadrm/libwvdrmengine.so"
if [ -f "$WIDEVINE_BLOB" ]; then
    python3 "$MY_DIR/fixups/widevine-protobuf.py" "$WIDEVINE_BLOB" --patchelf "${PATCHELF:-patchelf}"
fi

# Reinitialize the helper for device
setup_vendor "$DEVICE" "$VENDOR" "$MK_ROOT" false "$CLEAN_VENDOR"

extract "$MY_DIR"/proprietary-files.txt "$SRC" "$SECTION"
extract "$MY_DIR"/../$DEVICE/device-proprietary-files.txt "$SRC" "$SECTION"

"$MY_DIR"/setup-makefiles.sh
