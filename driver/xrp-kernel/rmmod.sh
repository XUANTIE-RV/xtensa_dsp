#!/bin/bash

# echo 1 > /sys/module/xrp/parameters/loopback
rmmod xrp_hw_simple.ko
rmmod xrp.ko
rmmod xrp_hw_comm.ko