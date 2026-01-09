@echo off

if not exist build mkdir build
cd build

cmake .. -G "MinGW Makefiles"
cmake --build .

cd bin

start "Emulator" cmd /k "emulator.exe COM3"
start "Logger" cmd /k "logger.exe COM4"
