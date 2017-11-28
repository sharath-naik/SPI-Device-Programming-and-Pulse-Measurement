#!/bin/bash

rmmod spidev
rmmod led.ko
rmmod sensor.ko

insmod led.ko
insmod sensor.ko
