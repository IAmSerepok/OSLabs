#!/bin/bash

x-terminal-emulator -e "socat -d -d pty,raw,echo=0,link=/tmp/virtual_com1 pty,raw,echo=0,link=/tmp/virtual_com2"
sleep 1
x-terminal-emulator -e "./build/output/logger /tmp/virtual_com1"
x-terminal-emulator -e "./build/output/emulator /tmp/virtual_com2"
