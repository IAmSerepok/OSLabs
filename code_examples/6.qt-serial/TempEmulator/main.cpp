#include "TempDeviceEmulator.h"
#include <QCoreApplication>
#include <QStringList>
#include <QTextStream>

QTextStream& qStdOut()
{
    static QTextStream ts( stdout );
    return ts;
}

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    QStringList args = a.arguments();
    if (args.size() < 2) {
        qStdOut() << "Usage: " << a.applicationName() << " sername" << Qt::endl;
        return -1;
    }
    TempDeviceEmulator emul(args[1],&a);
    if (!emul.IsConnected()) {
        qStdOut() << "Failed to open port " << args[1] << "! Terminating..." << Qt::endl;
        return -1;
    }
    qStdOut() << "Starting emulator on port " << args[1] << Qt::endl;
    return a.exec();
}
