#ifndef TEMPWINDOW_H
#define TEMPWINDOW_H

#include <QMainWindow>

namespace Ui {
class TempWindow;
}

// forward declaration
class QSerialPort;
class QTimer;

// Драйвер термометра
class TempDeviceDriver: public QObject
{
    Q_OBJECT
public:
    TempDeviceDriver(QObject* parent);
    virtual ~TempDeviceDriver(){}
    static QStringList AvailablePorts();
public slots:
    void ReadData();
    bool Connect(const QString& port_name);
signals:
    void TemperatureReady(double temp);
    void ConnectAttempt(bool res);
    void DataReadResult(bool res);
private:
    QSerialPort* m_port;
};

// Главное окно программы
class TempWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit TempWindow(QWidget *parent = 0);
    ~TempWindow();
public slots:
    void ConnectionAttempt(bool res);
    void DataReadResult(bool res);
private:
    Ui::TempWindow*   m_ui;
    QTimer*           m_tmr;
    TempDeviceDriver* m_devdrv;
    bool              m_prvres;
};

#endif // TEMPWINDOW_H
