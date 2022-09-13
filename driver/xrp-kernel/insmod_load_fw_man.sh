#!/bin/bash

mkdir /tmp/debugfs
mount -t debugfs none /tmp/debugfs
echo -n 'file xvp_main.c +p' > /tmp/debugfs/dynamic_debug/control
echo -n 'file xrp_hw_simple.c +p' > /tmp/debugfs/dynamic_debug/control
echo -n 'file xrp_firmware.c +p' > /tmp/debugfs/dynamic_debug/control
echo 8  > /proc/sys/kernel/printk
modprobe xrp_hw_comm
modprobe xrp
# echo 1 > /sys/module/xrp/parameters/loopback
#increase the timeout time 2 min
echo 120 > /sys/module/xrp/parameters/firmware_command_timeout
echo 1 > /sys/module/xrp/parameters/load_mode

memtool mw 0xffe7f3c408 0x11000000
memtool mw 0xffe7f3c40c 0x111
memtool mw 0xffe7f3c410 0x11111

modprobe xrp_hw_simple