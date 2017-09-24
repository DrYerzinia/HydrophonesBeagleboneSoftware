#!/bin/bash
# Unload PRU firmware

echo "4a334000.pru0" > /sys/bus/platform/drivers/pru-rproc/unbind

