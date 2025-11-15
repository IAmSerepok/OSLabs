#include "TempDeviceEmulator.h"
#include <QtSerialPort/QSerialPort>
#include <QTimer>
#include <QRandomGenerator>

TempDeviceEmulator::TempDeviceEmulator(const QString& com_port, QObject* parent):QObject(parent)
{
    m_port = new QSerialPort(this);
    m_tmr = new QTimer(this);
    m_tmr->setInterval(1000);
    m_temp = 25.0;

    m_port->setPortName(com_port);
    m_port->setBaudRate(QSerialPort::Baud115200);
    if (m_port->open(QIODevice::ReadWrite)) {
        connect(m_tmr,SIGNAL(timeout()),this,SLOT(PublishEmulatedTemp()));
        m_tmr->start();
    }
}

bool TempDeviceEmulator::IsConnected()
{
    return m_port->isOpen();
}

void TempDeviceEmulator::PublishEmulatedTemp()
{
    m_temp += QRandomGenerator::global()->bounded(-10,10)*0.02;
    m_port->write(QString("Temp: %1 Celsius\r\n").arg(m_temp).toUtf8());
}
