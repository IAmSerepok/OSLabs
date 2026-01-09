#ifndef TEMPERATURE_MONITOR_GUI_H
#define TEMPERATURE_MONITOR_GUI_H

#include <QMainWindow>
#include <QTimer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QDateTime>
#include <QPushButton>
#include <QLabel>
#include <QDateEdit>
#include <QTimeEdit>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QPainter>

// Кроссплатформенные заголовки QtCharts
#if defined(QT_CHARTS_LIB)
    #if QT_VERSION_MAJOR == 6
        #include <QChartView>
        #include <QChart>
        #include <QLineSeries>
        #include <QValueAxis>
        #include <QDateTimeAxis>
    #elif QT_VERSION_MAJOR == 5
        #include <QtCharts>
        QT_CHARTS_USE_NAMESPACE
    #endif
#else
    class QChartView;
    class QChart;
    class QLineSeries;
    class QValueAxis;
    class QDateTimeAxis;
#endif

class TemperatureMonitorGUI : public QMainWindow {
    Q_OBJECT
    
public:
    explicit TemperatureMonitorGUI(QWidget *parent = nullptr);
    ~TemperatureMonitorGUI();
    
private slots:
    void updateCurrentTemperature();
    void fetchHistoryData();
    void onDataReceived(QNetworkReply* reply);
    
private:
    void setupUI();
    void setupConnections();
    void setupCharts();
    void parseCurrentTemperature(const QJsonObject& json);
    void parseHistoryData(const QJsonArray& array);
    QString formatTemperature(double temp) const;
    
    // UI
    QChartView *chartView;
    QChart *temperatureChart;
    QLineSeries *temperatureSeries;
    QLineSeries *avgSeries;

    QDateEdit *startDateEdit;
    QDateEdit *endDateEdit;
    QTimeEdit *startTimeEdit;
    QTimeEdit *endTimeEdit;
    QPushButton *refreshButton;
    
    QLabel *currentTempLabel;
    QLabel *avgTempLabel;
    
    // Таймер
    QTimer *updateTimer;
    
    // Сетевой менеджер
    QNetworkAccessManager *networkManager;
    
    // Конфигурация
    QString serverUrl;
};

#endif