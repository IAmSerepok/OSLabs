#!/bin/bash

socat -d -d pty,raw,echo=0,link=/tmp/virtual_com1 pty,raw,echo=0,link=/tmp/virtual_com2 2>/tmp/socat
./build/output/logger /tmp/virtual_com1
./build/output/emulator /tmp/virtual_com2
