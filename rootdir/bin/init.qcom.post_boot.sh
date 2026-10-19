#!/vendor/bin/sh
# Copyright (c) 2009-2012, The Linux Foundation. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#     * Neither the name of The Linux Foundation nor
#       the names of its contributors may be used to endorse or promote
#       products derived from this software without specific prior written
#       permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
# NON-INFRINGEMENT ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

target=`getprop ro.board.platform`
result=0

for node in sampling_rate sampling_down_factor io_is_busy; do
    path=/sys/devices/system/cpu/cpufreq/ondemand/$node
    if [ -e "$path" ]; then
        chown -h system "$path" || result=1
    fi
done

emmc_boot=`getprop ro.boot.emmc`
case "$emmc_boot"
    in "true")
        for controller in rs300000a7.65536 rs300100a7.65536; do
            for node in force_sync sync_sts; do
                path=/sys/devices/platform/$controller/$node
                if [ -e "$path" ]; then
                    chown -h system "$path" || result=1
                fi
            done
        done
    ;;
esac

case "$target" in
    "msm8960" )
        if [ -e /sys/devices/platform/msm_sdcc.3/idle_timeout ]; then
            echo 10 > /sys/devices/platform/msm_sdcc.3/idle_timeout || result=1
        fi
        ;;
esac

# Post-setup services
case "$target" in
    "msm8960")
        soc_id=`cat /sys/devices/system/soc/soc0/id`
        case "$soc_id" in
             "153") #8064 v2
                 start thermal-engine || result=1
             ;;

	     *) #all targets except 8064 v2
		 start thermald || result=1
	     ;;
        esac
    ;;
esac

case "$target" in
    "msm8960" )
        stop mpdecision || result=1
    ;;
esac


# The Aries kernel builds FastRPC in. Other msm8960 variants may ship a module.
if [ ! -e /dev/adsprpc-smd ] && [ -f /vendor/lib/modules/adsprpc.ko ]; then
    insmod /vendor/lib/modules/adsprpc.ko || result=1
fi
if [ -e /dev/adsprpc-smd ]; then
    chown -h system:system /dev/adsprpc-smd || result=1
    chmod 666 /dev/adsprpc-smd || result=1
fi
exit "$result"
