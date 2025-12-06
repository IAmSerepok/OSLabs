cmake_minimum_required(VERSION 3.10)
project(ProgramProject VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCES main.cpp)

# Создаем исполняемый файл
add_executable(main ${SOURCES})

# Устанавливаем выходную папку
set_target_properties(main PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output
)

if(WIN32)
    target_compile_definitions(main PRIVATE _WIN32)
else()
    find_package(Threads REQUIRED)
    target_link_libraries(main Threads::Threads)
    
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_link_libraries(main rt)
    endif()
endif()
