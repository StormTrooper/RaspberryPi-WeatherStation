#!/bin/sh
# Copy this file to /etc/init.d/gpio and run 
#   update-rc.d -f gpio defaults
# to run it on every boot.
# This code is for Raspberry Pi
# Do what you want with this script

for i in 0 1 4 7 8 9 10 11 14 15 17 18 21 22 23 24 25
do
    echo $i > /sys/class/gpio/export
    chmod 777 /sys/class/gpio/gpio$i/*
>>>>>>> 5bba45f7a06ff02cb0a84c9174c761d616b61730
done

exit 0
