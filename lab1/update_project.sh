#!/bin/bash

PROJECT_DIR="./"

echo "Стягиваем изменения..."
git checkout -- .
git clean -fd
git fetch origin
git checkout main
git pull origin main

echo "Собираем проект..."
cd "$PROJECT_DIR"
mkdir -p output
cd output
cmake ..
cmake --build .

echo "Сборка завершена!"
