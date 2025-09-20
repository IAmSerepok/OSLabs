#!/bin/bash

echo "Стягиваем изменения..."
git pull origin main

echo "Собираем проект..."
cd lab1
mkdir -p output
cd output
cmake ..
cmake --build .

echo "Сборка завершена!"
