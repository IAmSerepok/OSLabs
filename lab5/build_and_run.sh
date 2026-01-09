#!/bin/bash

# Создаем директорию для сборки
if [ ! -d "build" ]; then
    echo -e "Creating build directory...${NC}"
    mkdir build
fi

cd build

cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release -j$(nproc)

cd ..

x-terminal-emulator -e "socat -d -d pty,raw,echo=0,link=/tmp/virtual_com1 pty,raw,echo=0,link=/tmp/virtual_com2"
sleep 1
x-terminal-emulator -e "./build/emulator /tmp/virtual_com1"
x-terminal-emulator -e "./build/temp_server /tmp/virtual_com2"

python3 temperature_client.py
