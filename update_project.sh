#!/bin/bash

echo "Стягиваем изменения..."
git pull origin main

echo "Собираем проект..."
mkdir -p build
cd build
cmake ..
cmake --build .

echo "Сборка завершена!"
