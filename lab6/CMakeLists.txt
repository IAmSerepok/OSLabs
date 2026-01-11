# Собираем только оконное, а сервер с эмулятором в lab5

cmake_minimum_required(VERSION 3.16)
project(TemperatureMonitorGUI)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(WIN32)
    set(QT_PATH "D:/Qt/6.10.1/mingw_64" CACHE PATH "Path to Qt installation")
    set(CMAKE_PREFIX_PATH ${QT_PATH})

    find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network Charts)

    add_executable(temperature_monitor_gui
        main_gui.cpp
        temperature_monitor_gui.cpp
        temperature_monitor_gui.h
    )

    target_link_libraries(temperature_monitor_gui
        Qt6::Core
        Qt6::Widgets
        Qt6::Network
        Qt6::Charts
    )

    target_compile_definitions(temperature_monitor_gui 
        PRIVATE 
            QT_CHARTS_LIB
            QT_VERSION_MAJOR=6
    )

    set(QT_BIN_DIR "${QT_PATH}/bin")
    
    set(QT_DLLS
        "${QT_BIN_DIR}/Qt6Core.dll"
        "${QT_BIN_DIR}/Qt6Widgets.dll"
        "${QT_BIN_DIR}/Qt6Network.dll"
        "${QT_BIN_DIR}/Qt6Charts.dll"
        "${QT_BIN_DIR}/Qt6Gui.dll"
        "${QT_BIN_DIR}/Qt6OpenGL.dll"
        "${QT_BIN_DIR}/Qt6OpenGLWidgets.dll"
    )

    set(QT_PLUGINS_DIR "${QT_PATH}/plugins")
    
    # Создаем директорию для плагинов
    add_custom_command(TARGET temperature_monitor_gui POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:temperature_monitor_gui>/platforms"
    )

    # Копируем плагин windows
    if(EXISTS "${QT_PLUGINS_DIR}/platforms/qwindows.dll")
        add_custom_command(TARGET temperature_monitor_gui POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${QT_PLUGINS_DIR}/platforms/qwindows.dll"
                "$<TARGET_FILE_DIR:temperature_monitor_gui>/platforms/"
        )
    endif()

    foreach(DLL ${QT_DLLS})
        if(EXISTS ${DLL})
            add_custom_command(TARGET temperature_monitor_gui POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${DLL}"
                    $<TARGET_FILE_DIR:temperature_monitor_gui>
                COMMENT "Copying ${DLL}"
            )
        else()
            message(WARNING "File not found: ${DLL}")
        endif()
    endforeach()
    
    file(GLOB ICU_DLLS "${QT_BIN_DIR}/icu*.dll")
    foreach(ICU_DLL ${ICU_DLLS})
        add_custom_command(TARGET temperature_monitor_gui POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${ICU_DLL}"
                $<TARGET_FILE_DIR:temperature_monitor_gui>
            COMMENT "Copying ICU DLL: ${ICU_DLL}"
        )
    endforeach()

    find_program(MINGW_GCC NAMES gcc g++.exe PATHS 
        "C:/Qt/Tools/mingw*"
        "D:/Qt/Tools/mingw*"
        "C:/mingw64/bin"
        NO_DEFAULT_PATH)
    
    if(MINGW_GCC)
        get_filename_component(MINGW_BIN_DIR ${MINGW_GCC} DIRECTORY)
        message(STATUS "Found MinGW at: ${MINGW_BIN_DIR}")
        
        set(MINGW_DLLS
            "${MINGW_BIN_DIR}/libstdc++-6.dll"
            "${MINGW_BIN_DIR}/libgcc_s_seh-1.dll"
            "${MINGW_BIN_DIR}/libwinpthread-1.dll"
        )
        
        foreach(MINGW_DLL ${MINGW_DLLS})
            if(EXISTS ${MINGW_DLL})
                add_custom_command(TARGET temperature_monitor_gui POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${MINGW_DLL}"
                        $<TARGET_FILE_DIR:temperature_monitor_gui>
                    COMMENT "Copying MinGW DLL: ${MINGW_DLL}"
                )
            endif()
        endforeach()
    else()
        message(WARNING "MinGW not found.")
    endif()
else()
    # Используем Qt5 вместо Qt6 (qt6 отказывается ставится)
    find_package(Qt5 REQUIRED COMPONENTS Core Widgets Network)

    # Поиск Qt Charts
    find_package(Qt5Charts REQUIRED)

    add_executable(temperature_monitor_gui
        main_gui.cpp
        temperature_monitor_gui.cpp
        temperature_monitor_gui.h
    )

    target_link_libraries(temperature_monitor_gui
        Qt5::Core
        Qt5::Widgets
        Qt5::Network
        Qt5::Charts
    )

    target_compile_definitions(temperature_monitor_gui PRIVATE IS_LINUX)
endif()