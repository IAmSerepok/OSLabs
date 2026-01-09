cmake_minimum_required(VERSION 3.10)
project(TemperatureMonitor)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Основной сервер
add_executable(temp_server 
    main_server.cpp
    sqlite3.c 
)
target_include_directories(temp_server PRIVATE .)

# Флаги компиляции для SQLite
target_compile_definitions(temp_server PRIVATE 
    SQLITE_THREADSAFE=1
    SQLITE_OMIT_LOAD_EXTENSION=1
)

# Системные библиотеки для сервера
if(WIN32)
    target_link_libraries(temp_server ws2_32)
else()
    find_package(Threads REQUIRED)
    target_link_libraries(temp_server Threads::Threads)
    # Для Linux добавляем необходимые определения
    target_compile_definitions(temp_server PRIVATE
        _DEFAULT_SOURCE
        _GNU_SOURCE
        _XOPEN_SOURCE=700
    )
endif()

# Эмулятор датчика температуры
add_executable(emulator
    emulator.cpp
)
target_include_directories(emulator PRIVATE .)

# Системные библиотеки для эмулятора
if(WIN32)
    target_link_libraries(emulator)
else()
    target_link_libraries(emulator Threads::Threads)
    # Для Linux добавляем необходимые определения
    target_compile_definitions(emulator PRIVATE
        _DEFAULT_SOURCE
        _GNU_SOURCE
        _XOPEN_SOURCE=700
    )
endif()

# Добавляем флаги компилятора для Linux
if(NOT WIN32)
    target_compile_options(temp_server PRIVATE -Wall -Wextra -O2)
    target_compile_options(emulator PRIVATE -Wall -Wextra -O2)
endif()