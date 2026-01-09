#!/bin/bash

# Собираем нужные вещи из lab5
cd ../lab5

if [ ! -d "build" ]; then
    mkdir build
fi
cd build

cmake ..
cmake --build .

x-terminal-emulator -e "socat -d -d pty,raw,echo=0,link=/tmp/virtual_com1 pty,raw,echo=0,link=/tmp/virtual_com2"
sleep 1
x-terminal-emulator -e "./emulator /tmp/virtual_com1"
x-terminal-emulator -e "./temp_server /tmp/virtual_com2"

# Возврат и переход в lab6
cd ../../lab6

# Создание и переход в директорию сборки
if [ ! -d "build" ]; then
    mkdir build
fi
cd build

# Сборка проекта lab6
cmake ..
cmake --build .

# Создание директории platforms если нужно (на винде точно без нее не пашет)
if [ ! -d "platforms" ]; then
    mkdir platforms
fi

./temperature_monitor_gui