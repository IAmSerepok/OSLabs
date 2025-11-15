# Эмулятор устройства
QT       += core serialport

CONFIG += console
TARGET = TempDevEmul
TEMPLATE = app

SOURCES += \
        main.cpp \
        TempDeviceEmulator.cpp
		
HEADERS += \
        TempDeviceEmulator.h

# Установка программы
win32{
	DESTDIR         = $$PWD/../_bin_win32
    INSTALLS += QT_DLL
    QT_DLL.path = $$DESTDIR
    # утилита windeployqt доустанавливает все недостающие qt dll,
    # необходимые для запуска приложений из папки DESTDIR
    QT_DLL.extra += $$shell_path($$[QT_INSTALL_BINS]/windeployqt.exe --no-translations \
                    --plugindir $$DESTDIR/plugins $$DESTDIR/$${TARGET}.exe)
}
linux{
    DESTDIR         = $$PWD/../_bin_linux
}