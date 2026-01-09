#!/bin/bash

mkdir -p build
cd build

cmake .. -DCMAKE_BUILD_TYPE=Release
if [ $? -ne 0 ]; then
    exit 1
fi

make -j$(nproc)
if [ $? -ne 0 ]; then
    exit 1
fi

# Запуск теста
cd ..
x-terminal-emulator -e "socat -d -d pty,raw,echo=0,link=/tmp/virtual_com1 pty,raw,echo=0,link=/tmp/virtual_com2"
sleep 1
x-terminal-emulator -e "./build/bin/logger /tmp/virtual_com1"
x-terminal-emulator -e "./build/bin/emulator /tmp/virtual_com2"

