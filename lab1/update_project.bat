@echo off
chcp 65001 > nul

set PROJECT_DIR=.\lab1

cd ..
git checkout -- .
git clean -fd
git fetch origin
git checkout main
git pull origin main

cd "%PROJECT_DIR%"
if not exist output (
    mkdir output
)
cd output

cmake ..

cmake --build .

echo Done!
pause
