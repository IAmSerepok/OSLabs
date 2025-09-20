@echo off
echo Стягиваем изменения...
git pull origin main

echo Собираем проект...
mkdir build 2>nul
cd build
cmake ..
cmake --build .

echo Сборка завершена!
pause
