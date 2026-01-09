#include "temperature_monitor_gui.h"
#include <QApplication>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    
    TemperatureMonitorGUI window;
    window.show();
    
    return app.exec();
}