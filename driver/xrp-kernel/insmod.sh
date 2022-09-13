#!/bin/bash

# mkdir /tmp/debugfs
# mount -t debugfs none /tmp/debugfs
# echo -n 'file xvp_main.c +p' > /tmp/debugfs/dynamic_debug/control
# echo -n 'file xrp_hw_simple.c +p' > /tmp/debugfs/dynamic_debug/control
# echo -n 'file xrp_firmware.c +p' > /tmp/debugfs/dynamic_debug/control
# echo 8  > /proc/sys/kernel/printk
modprobe xrp_hw_comm
modprobe xrp
# echo 1 > /sys/module/xrp/parameters/loopback
modprobe xrp_hw_simple