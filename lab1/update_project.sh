#!/bin/bash

PROJECT_DIR="./lab1"

echo "Стягиваем изменения..."
cd ..
git checkout -- .
git clean -fd
git fetch origin
git checkout main
git pull origin main

echo "Собираем проект..."
cd "$PROJECT_DIR"
mkdir -p build
cd build
cmake ..
cmake --build .

echo "Сборка завершена!"
