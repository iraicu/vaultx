#!/bin/bash

# Synchronize filesystem buffers
sync

# Drop all caches
sudo sh -c 'echo 3 > /proc/sys/vm/drop_caches'

# Display memory usage
free -h
