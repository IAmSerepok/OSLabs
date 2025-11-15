#include "TempWindow.h"
#include "ui_TempWindow.h"
#include <QtSerialPort/QSerialPort>
#include <QSerialPortInfo>
#include <QTimer>
#include <QMessageBox>
#include <QApplication>
#include <QtGlobal>
#include <QFile>

// Строка от устройства: начало и завершение
#define STR_START   "Temp:"
#define STR_END     "Celsius"

// Драйвер термометра
TempDeviceDriver::TempDeviceDriver(QObject* parent):QObject(parent),m_port(new QSerialPort(this))
{
}

bool TempDeviceDriver::Connect(const QString& port_name)
{
    if (port_name.isEmpty()) {
        emit ConnectAttempt(false);
        return false;
    }
    m_port->close();
    m_port->setPortName(port_name);
    m_port->setBaudRate(QSerialPort::Baud115200);
    bool ret = m_port->open(QIODevice::ReadOnly);;
    if (!ret) {
        QMessageBox::critical(QApplication::activeWindow(),
                              tr("Ошибка открытия COM-порта"),
                              tr("Не смогли открыть порт с ошибкой: %1").
                                arg(m_port->errorString()));
    }
    emit ConnectAttempt(ret);
    return ret;
}
QStringList TempDeviceDriver::AvailablePorts()
{
    QStringList ret;
    QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();
    for (int i = 0; i < ports.size(); ++i)
        ret.push_back(ports.at(i).portName());
#ifdef Q_OS_LINUX
    if (QFile::exists("/dev/pts/3"))
        ret.push_back("/dev/pts/3");
#endif
    return ret;
}
void TempDeviceDriver::ReadData()
{
    // Читаем с устройства линию
    QByteArray str = m_port->readLine();
    // Ищем начало строки с данными
    int str_start = str.indexOf(STR_START);
    if (str_start < 0) {
        emit DataReadResult(false);
        return;
    }
    str_start += strlen(STR_START);
    // Ищем конец строки с данными
    int str_end = str.indexOf(STR_END,str_start);
    if (str_end < 0) {
        emit DataReadResult(false);
        return;
    }
    // Парсим температуру
    double temp = str.mid(str_start,str_end-str_start-1).trimmed().toDouble();
    // Публикуем температуру
    emit TemperatureReady(temp);
    emit DataReadResult(true);
}

TempWindow::TempWindow(QWidget *parent) :
    QMainWindow(parent),
    m_ui(new Ui::TempWindow),
    m_tmr (new QTimer(this)),
    m_devdrv(new TempDeviceDriver(this))
{
    // Настроим интерфейс
    m_ui->setupUi(this);
    // Выведем доступные COM-порты в список
    m_ui->combo_serial->addItems(TempDeviceDriver::AvailablePorts());
    // Подключим все сигналы к нужным слотам
    connect(m_devdrv,SIGNAL(TemperatureReady(double)),m_ui->lcd_display,SLOT(display(double)));
    connect(m_devdrv,SIGNAL(ConnectAttempt(bool)),this,SLOT(ConnectionAttempt(bool)));
    connect(m_ui->combo_serial,SIGNAL(currentIndexChanged(QString)),m_devdrv,SLOT(Connect(QString)));
    connect(m_devdrv,SIGNAL(DataReadResult(bool)),this,SLOT(DataReadResult(bool)));
    connect(m_tmr,SIGNAL(timeout()),m_devdrv,SLOT(ReadData()));
    // Установим таймер
    m_tmr->setInterval(1000);
    // Подключимся к последнему COM-порту в списке (попытаемся)
    m_ui->combo_serial->setCurrentIndex(-1);
    m_ui->combo_serial->setCurrentIndex(m_ui->combo_serial->count()-1);
    m_prvres = false;
}

void TempWindow::ConnectionAttempt(bool res)
{
    if (!res) {
        m_tmr->stop();
        m_ui->check_connect->setChecked(false);
        m_ui->check_connect->setText(tr("Нет связи"));
        m_ui->check_connect->setStyleSheet("color: rgb(255, 0, 0)");
        return;
    }
    m_ui->check_connect->setChecked(true);
    m_ui->check_connect->setText(tr("Есть связь"));
    m_ui->check_connect->setStyleSheet("color: rgb(0, 255, 0)");
    m_tmr->start();

}

void TempWindow::DataReadResult(bool res)
{
    if (res || m_prvres) {
        m_ui->check_data->setChecked(true);
        m_ui->check_data->setText(tr("Есть данные"));
        m_ui->check_data->setStyleSheet("color: rgb(0, 255, 0)");
    } else {
        m_ui->check_data->setChecked(false);
        m_ui->check_data->setText(tr("Нет данных"));
        m_ui->check_data->setStyleSheet("color: rgb(255, 0, 0)");
    }
    m_prvres = res;
}
TempWindow::~TempWindow()
{
    delete m_ui;
}
