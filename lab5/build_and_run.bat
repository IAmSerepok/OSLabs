@echo off

if not exist build mkdir build
cd build

cmake .. -G "MinGW Makefiles"
cmake --build .

cd ..
pip install uv
uv venv venv

call venv\Scripts\activate
uv pip install -r requirements.txt

cd build
start "Emulator" cmd /k "emulator.exe COM3"
start "Server" cmd /k "temp_server.exe COM4"

cd ..
python temperature_client.py
