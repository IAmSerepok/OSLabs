#include "temperature_monitor_gui.h"
#include <QGridLayout>
#include <QMessageBox>

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
#endif

TemperatureMonitorGUI::TemperatureMonitorGUI(QWidget *parent)
    : QMainWindow(parent)
    , chartView(nullptr)
    , temperatureChart(nullptr)
    , temperatureSeries(nullptr)
    , avgSeries(nullptr)
    , serverUrl("http://localhost:8080")
{
    setupUI();
    setupConnections();
    setupCharts();
    
    // Устанавливаем размер окна
    resize(1200, 800);
    setWindowTitle("Мониторинг температуры");
    
    updateTimer->start(5000); // Обновление каждые 5 секунд
    
    // Первоначальное обновление
    updateCurrentTemperature();
    fetchHistoryData();
}

TemperatureMonitorGUI::~TemperatureMonitorGUI() {
    if (updateTimer) {
        updateTimer->stop();
    }
}

void TemperatureMonitorGUI::setupUI() {
    // Центральный виджет
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    
    // Основной макет
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    
    // Панель управления
    QGroupBox *controlGroup = new QGroupBox("Выбор временного диапазона");
    QGridLayout *controlLayout = new QGridLayout(controlGroup);
    controlLayout->setSpacing(10);
    
    // Даты
    controlLayout->addWidget(new QLabel("Начальная дата:"), 0, 0);
    startDateEdit = new QDateEdit(QDate::currentDate().addDays(-1));
    startDateEdit->setCalendarPopup(true);
    startDateEdit->setDisplayFormat("dd.MM.yyyy");
    startDateEdit->setMinimumWidth(120);
    controlLayout->addWidget(startDateEdit, 0, 1);
    
    controlLayout->addWidget(new QLabel("Конечная дата:"), 0, 2);
    endDateEdit = new QDateEdit(QDate::currentDate());
    endDateEdit->setCalendarPopup(true);
    endDateEdit->setDisplayFormat("dd.MM.yyyy");
    endDateEdit->setMinimumWidth(120);
    controlLayout->addWidget(endDateEdit, 0, 3);
    
    // Время
    controlLayout->addWidget(new QLabel("Начальное время:"), 1, 0);
    startTimeEdit = new QTimeEdit(QTime(0, 0));
    startTimeEdit->setDisplayFormat("HH:mm");
    startTimeEdit->setMinimumWidth(80);
    controlLayout->addWidget(startTimeEdit, 1, 1);
    
    controlLayout->addWidget(new QLabel("Конечное время:"), 1, 2);
    endTimeEdit = new QTimeEdit(QTime(23, 59));
    endTimeEdit->setDisplayFormat("HH:mm");
    endTimeEdit->setMinimumWidth(80);
    controlLayout->addWidget(endTimeEdit, 1, 3);
    
    // Кнопка обновления
    refreshButton = new QPushButton("Обновить график");
    refreshButton->setStyleSheet("padding: 5px 15px; font-weight: bold;");
    controlLayout->addWidget(refreshButton, 2, 0, 1, 4);
    
    controlLayout->setColumnStretch(4, 1);
    
    mainLayout->addWidget(controlGroup);
    
    // Информационная панель
    QWidget *infoPanel = new QWidget();
    QHBoxLayout *infoLayout = new QHBoxLayout(infoPanel);
    
    currentTempLabel = new QLabel("Текущая температура: --.-- °C");
    currentTempLabel->setStyleSheet("font-weight: bold; font-size: 20px; color: #4FC3F7;");
    infoLayout->addWidget(currentTempLabel);
    
    infoLayout->addSpacing(30);
    
    avgTempLabel = new QLabel("Средняя температура: --.-- °C");
    avgTempLabel->setStyleSheet("font-size: 18px;");
    infoLayout->addWidget(avgTempLabel);
    
    infoLayout->addStretch(1);
    
    mainLayout->addWidget(infoPanel);
    
    // Группа для графика
    QGroupBox *chartGroup = new QGroupBox("График температуры");
    chartGroup->setStyleSheet("QGroupBox { font-weight: bold; }");
    QVBoxLayout *chartLayout = new QVBoxLayout(chartGroup);
    chartLayout->setContentsMargins(5, 20, 5, 5); // Уменьшаем отступы
    
    // Устанавливаем растяжение для группы графика
    mainLayout->addWidget(chartGroup, 1);
    
    // Таймер
    updateTimer = new QTimer(this);
    
    // Сетевой менеджер
    networkManager = new QNetworkAccessManager(this);
}

void TemperatureMonitorGUI::setupConnections() {
    connect(updateTimer, &QTimer::timeout, this, &TemperatureMonitorGUI::updateCurrentTemperature);
    connect(refreshButton, &QPushButton::clicked, this, &TemperatureMonitorGUI::fetchHistoryData);
    
    connect(networkManager, &QNetworkAccessManager::finished,
            this, &TemperatureMonitorGUI::onDataReceived);
}

void TemperatureMonitorGUI::setupCharts() {
    // Создаем график только если Charts доступен
    #ifdef QT_CHARTS_LIB
    temperatureChart = new QChart();
    temperatureChart->setTitle("Изменение температуры");
    
    // Установка темы для навый кьюти
    #if QT_VERSION >= QT_VERSION_CHECK(5, 7, 0)
        temperatureChart->setTheme(QChart::ChartThemeDark);
    #endif
    
    temperatureChart->legend()->setVisible(true);
    temperatureChart->legend()->setAlignment(Qt::AlignBottom);
    
    chartView = new QChartView(temperatureChart);
    chartView->setRenderHint(QPainter::Antialiasing);
    
    // Настройка размеров
    chartView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    chartView->setMinimumHeight(400); 
    
    // Ищем группу графика по имени
    QGroupBox *chartGroup = nullptr;
    QList<QGroupBox*> groups = findChildren<QGroupBox*>();
    for (QGroupBox *group : groups) {
        if (group->title() == "График температуры") {
            chartGroup = group;
            break;
        }
    }
    
    if (chartGroup && chartGroup->layout()) {
        // Очищаем 
        QLayoutItem* item;
        while ((item = chartGroup->layout()->takeAt(0)) != nullptr) {
            delete item->widget();
            delete item;
        }
        
        chartGroup->layout()->addWidget(chartView);
    }
    
    temperatureSeries = new QLineSeries();
    temperatureSeries->setName("Температура");
    temperatureSeries->setColor(QColor(79, 195, 247));
    temperatureSeries->setPen(QPen(QColor(79, 195, 247), 2));
    temperatureSeries->setPointsVisible(true);
    
    avgSeries = new QLineSeries();
    avgSeries->setName("Средняя");
    avgSeries->setColor(QColor(255, 193, 7));
    avgSeries->setPen(QPen(QColor(255, 193, 7), 2, Qt::DashLine));
    
    temperatureChart->addSeries(temperatureSeries);
    temperatureChart->addSeries(avgSeries);
    
    // ООХ
    QDateTimeAxis *axisX = new QDateTimeAxis();
    axisX->setTitleText("Время");
    axisX->setFormat("dd.MM HH:mm");
    axisX->setTickCount(10);
    axisX->setLabelsAngle(-45);
    axisX->setGridLineVisible(true);
    axisX->setGridLineColor(QColor(100, 100, 100, 100));
    temperatureChart->addAxis(axisX, Qt::AlignBottom);
    temperatureSeries->attachAxis(axisX);
    avgSeries->attachAxis(axisX);
    
    // ОУ
    QValueAxis *axisY = new QValueAxis();
    axisY->setTitleText("Температура, °C");
    axisY->setLabelFormat("%.1f");
    axisY->setRange(-20, 40);
    axisY->setTickCount(13);
    axisY->setGridLineVisible(true);
    axisY->setGridLineColor(QColor(100, 100, 100, 100));
    temperatureChart->addAxis(axisY, Qt::AlignLeft);
    temperatureSeries->attachAxis(axisY);
    avgSeries->attachAxis(axisY);
    
    // отступы
    temperatureChart->setMargins(QMargins(10, 10, 10, 10));
    #endif
}

void TemperatureMonitorGUI::updateCurrentTemperature() {
    QUrl url(serverUrl + "/api/current");
    QNetworkRequest request(url);
    networkManager->get(request);
}

void TemperatureMonitorGUI::fetchHistoryData() {
    QDateTime startDateTime = QDateTime(startDateEdit->date(), startTimeEdit->time());
    QDateTime endDateTime = QDateTime(endDateEdit->date(), endTimeEdit->time());
    
    // Проверка корректности диапазона
    if (startDateTime > endDateTime) {
        QMessageBox::warning(this, "Ошибка", 
            "Начальная дата должн быть раньше конечной даты.");
        return;
    }
    
    QString startStr = startDateTime.toString("yyyy-MM-dd HH:mm:ss");
    QString endStr = endDateTime.toString("yyyy-MM-dd HH:mm:ss");
    
    QUrl url(serverUrl + "/api/raw");
    QUrlQuery query;
    query.addQueryItem("start", startStr);
    query.addQueryItem("end", endStr);
    query.addQueryItem("limit", "1000");
    url.setQuery(query);
    
    QNetworkRequest request(url);
    networkManager->get(request);
}

void TemperatureMonitorGUI::onDataReceived(QNetworkReply* reply) {
    if (reply->error() != QNetworkReply::NoError) {
        QMessageBox::warning(this, "Ошибка", "Не удалось получить данные: " + reply->errorString());
        reply->deleteLater();
        return;
    }
    
    QByteArray data = reply->readAll();
    QUrl url = reply->url();
    QString path = url.path();
    
    reply->deleteLater();
    
    QJsonDocument doc = QJsonDocument::fromJson(data);
    
    if (path.contains("/api/current")) {
        parseCurrentTemperature(doc.object());
    } else if (path.contains("/api/raw")) {
        parseHistoryData(doc.array());
    }
}

void TemperatureMonitorGUI::parseCurrentTemperature(const QJsonObject& json) {
    if (json.contains("temperature")) {
        double temp = json["temperature"].toDouble();
        currentTempLabel->setText(QString("Текущая температура: %1").arg(formatTemperature(temp)));
    }
}

void TemperatureMonitorGUI::parseHistoryData(const QJsonArray& array) {
    #ifdef QT_CHARTS_LIB
    if (temperatureSeries && avgSeries) {
        temperatureSeries->clear();
        avgSeries->clear();
    }
    #endif
    
    if (array.isEmpty()) {
        avgTempLabel->setText("Средняя температура: нет данных");
        return;
    }
    
    double sum = 0;
    QDateTime minTime, maxTime;
    int validCount = 0;
    
    for (const QJsonValue& value : array) {
        QJsonObject obj = value.toObject();
        QDateTime timestamp = QDateTime::fromString(obj["timestamp"].toString(), 
                                                   "yyyy-MM-dd HH:mm:ss.zzz");
        double temperature = obj["temperature"].toDouble();
        
        if (timestamp.isValid()) {
            qint64 ts = timestamp.toMSecsSinceEpoch();
            
            #ifdef QT_CHARTS_LIB
            if (temperatureSeries) {
                temperatureSeries->append(ts, temperature);
            }
            #endif
            
            sum += temperature;
            validCount++;
            
            if (!minTime.isValid() || timestamp < minTime) minTime = timestamp;
            if (!maxTime.isValid() || timestamp > maxTime) maxTime = timestamp;
        }
    }
    
    if (validCount == 0) {
        avgTempLabel->setText("Средняя температура: нет данных");
        return;
    }
    
    double avgTemp = sum / validCount;
    avgTempLabel->setText(QString("Средняя температура: %1").arg(formatTemperature(avgTemp)));
    
    #ifdef QT_CHARTS_LIB
    // Добавляем линию среднего значения
    if (temperatureSeries && avgSeries && temperatureSeries->count() > 0) {
        double firstX = temperatureSeries->at(0).x();
        double lastX = temperatureSeries->at(temperatureSeries->count()-1).x();
        
        avgSeries->append(firstX, avgTemp);
        avgSeries->append(lastX, avgTemp);
    }
    
    // Обновляем оси графика
    if (temperatureChart && minTime.isValid() && maxTime.isValid()) {
        QList<QAbstractAxis*> axes = temperatureChart->axes(Qt::Horizontal);
        if (!axes.isEmpty()) {
            QDateTimeAxis *axisX = qobject_cast<QDateTimeAxis*>(axes.first());
            if (axisX) {
                axisX->setRange(minTime, maxTime);
            }
        }
        
        axes = temperatureChart->axes(Qt::Vertical);
        if (!axes.isEmpty()) {
            QValueAxis *axisY = qobject_cast<QValueAxis*>(axes.first());
            if (axisY) {
                axisY->setRange(-20, 40);
            }
        }
    }
    #endif
}

QString TemperatureMonitorGUI::formatTemperature(double temp) const {
    return QString::number(temp, 'f', 2) + " °C";
}