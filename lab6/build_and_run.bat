@echo off

cd ..\lab5

if not exist build mkdir build
cd build

cmake .. -G "MinGW Makefiles"
cmake --build .

cd build
start "Emulator" cmd /k "emulator.exe COM3"
start "Server" cmd /k "temp_server.exe COM4"

cd ..\..\lab6

if not exist build mkdir build
cd build

cmake .. -G "MinGW Makefiles"
cmake --build .

if not exist platforms mkdir platforms

call temperature_monitor_gui.exe
