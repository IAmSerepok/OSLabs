cmake_minimum_required(VERSION 3.10)
project(TemperatureMonitor)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Включаем модуль для проверки функций
include(CheckIncludeFile)
include(CheckFunctionExists)

# Определяем платформу
if(WIN32)
    add_definitions(-DWIN32_LEAN_AND_MEAN)
    add_definitions(-D_WIN32_WINNT=0x0601)
    set(PLATFORM_SOURCES "")
    set(PLATFORM_LIBRARIES "")
else()
    add_definitions(-D_POSIX_C_SOURCE=200809L)
    
    # Проверяем необходимые заголовочные файлы
    check_include_file(termios.h HAVE_TERMIOS_H)
    check_include_file(fcntl.h HAVE_FCNTL_H)
    check_include_file(unistd.h HAVE_UNISTD_H)
    check_include_file(sys/stat.h HAVE_SYS_STAT_H)
    
    if(NOT HAVE_TERMIOS_H) # Потры
        message(WARNING "termios.h not found")
    endif()
    
    if(NOT HAVE_FCNTL_H) # Файлы
        message(WARNING "fcntl.h not found")
    endif()
    
    # Находим pthread
    find_package(Threads REQUIRED)
    set(PLATFORM_LIBRARIES Threads::Threads)
endif()

# Настройки для разных конфигураций
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Исполняемые файлы
add_executable(emulator 
    emulator.cpp
)

add_executable(logger 
    logger.cpp
)

# Подключаем библиотеки
target_link_libraries(emulator ${PLATFORM_LIBRARIES})
target_link_libraries(logger ${PLATFORM_LIBRARIES})

# Флаги компилятора
if(MSVC)
    target_compile_options(emulator PRIVATE /W4 /WX)
    target_compile_options(logger PRIVATE /W4 /WX)
else()
    target_compile_options(emulator PRIVATE -Wall -Wextra -pedantic)
    target_compile_options(logger PRIVATE -Wall -Wextra -pedantic)
    
    # Добавляем флаги для Linux
    target_compile_options(emulator PRIVATE -pthread)
    target_compile_options(logger PRIVATE -pthread)
endif()

# Проверка наличия заголовочных файлов
target_include_directories(emulator PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(logger PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
