// Простенький эмулятор устройства, публикующего
// температуру по серийному порту

#include <QObject>

// forward declaration
class QSerialPort;
class QTimer;

// Драйвер эмулятора
class TempDeviceEmulator: public QObject
{
    Q_OBJECT
public:
    TempDeviceEmulator(const QString& com_port, QObject* parent = NULL);
    virtual ~TempDeviceEmulator(){}
	bool IsConnected();
public slots:
    void PublishEmulatedTemp();
private:
	QTimer*       m_tmr;
    QSerialPort*  m_port;
    double        m_temp;
};
