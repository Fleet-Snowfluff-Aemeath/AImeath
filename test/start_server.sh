#!/bin/bash
cd /home/huanli/lab
LD_LIBRARY_PATH=build/output/lib ./build/output/gameserver &
echo $!
