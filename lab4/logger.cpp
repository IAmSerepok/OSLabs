#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <map>
#include <algorithm>
#include <atomic>
#include <thread>
#include <csignal>
#include <cmath>
#include <mutex>
#include <deque>
#include <sys/stat.h> 

#include "my_serial.hpp"

using namespace cplib;
using namespace std;

atomic<bool> g_running(true);

void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        g_running = false;
    }
}

struct TemperatureData {
    string timestamp;
    float temperature;
};

class TemperatureLogger {
private:
    string raw_log_file;
    string hourly_log_file;
    string daily_log_file;
    
    // Ограничение на длину файла
    static const size_t MAX_RAW_RECORDS = 24 * 60 * 60; 
    static const size_t MAX_HOURLY_RECORDS = 30 * 24; 
    static const size_t MAX_DAILY_RECORDS = 365; 
    
    // Счетчики записей в файлах
    size_t raw_records_count = 0;
    size_t hourly_records_count = 0;
    size_t daily_records_count = 0;
    
    // Флаги для периодической очистки
    atomic<bool> needs_raw_cleanup{false};
    atomic<bool> needs_hourly_cleanup{false};
    atomic<bool> needs_daily_cleanup{false};
    
    // Будем срезать старые данные перезаписывая файл не каждый раз, а раз в несколько итераций
    static const size_t CLEANUP_CHECK_INTERVAL = 100;
    
    time_t last_hour_check;
    time_t last_day_check;
    
    mutex data_mutex;
    mutex file_mutex;
    
    bool parse_json(const string& json_str, TemperatureData& data) {
        size_t temp_pos = json_str.find("\"temperature\":");
        size_t checksum_pos = json_str.find("\"checksum\":");
        size_t time_pos = json_str.find("\"timestamp\":");
        
        if (temp_pos == string::npos || checksum_pos == string::npos || time_pos == string::npos) {
            return false;
        }
        
        // Извлекаем температуру
        temp_pos += 14; // Длина "\"temperature\":"
        size_t temp_end = json_str.find(',', temp_pos);
        if (temp_end == string::npos) return false;
        
        string temp_str = json_str.substr(temp_pos, temp_end - temp_pos);
        float temperature;
        try {
            temperature = stof(temp_str);
        } catch (...) {
            return false;
        }
        
        // Извлекаем контрольную сумму
        checksum_pos += 11; // Длина "\"checksum\":"
        size_t checksum_end = json_str.find('}', checksum_pos);
        if (checksum_end == string::npos) {
            checksum_end = json_str.find(',', checksum_pos);
        }
        if (checksum_end == string::npos) return false;
        
        string checksum_str = json_str.substr(checksum_pos, checksum_end - checksum_pos);
        float checksum;
        try {
            checksum = stof(checksum_str);
        } catch (...) {
            return false;
        }
        
        // Проверяем контрольную сумму
        const float EPSILON = 0.01f;
        if (fabs(temperature - checksum) > EPSILON) {
            cout << "Checksum error\n";
            return false;
        }
        
        // Извлекаем timestamp
        time_pos += 13; // Длина "\"timestamp\":"
        size_t time_end = json_str.find('"', time_pos + 1);
        if (time_end == string::npos) return false;
        
        data.timestamp = json_str.substr(time_pos + 1, time_end - time_pos - 1);
        data.temperature = temperature;
        
        return true;
    }
    
    // Проверка существования файла
    bool file_exists(const string& filename) {
        struct stat buffer;
        return (stat(filename.c_str(), &buffer) == 0);
    }
    
    // Запись всех замеров
    void append_raw_log(const TemperatureData& data) {
        lock_guard<mutex> lock(file_mutex);
        
        // Дозаписываем в конец файла
        ofstream out_file(raw_log_file, ios::app);
        if (out_file.is_open()) {
            out_file << data.timestamp << "," 
                    << fixed << setprecision(2) << data.temperature << endl;
            raw_records_count++;
            
            // Проверяем, не нужно ли очистить старые записи
            if (raw_records_count > MAX_RAW_RECORDS) {
                needs_raw_cleanup = true;
            }
        }
    }
    
    // Периодическая очистка старых записей
    void cleanup_raw_log() {
        lock_guard<mutex> lock(file_mutex);
        
        if (!needs_raw_cleanup || raw_records_count <= MAX_RAW_RECORDS) {
            return;
        }
        
        // Читаем все записи из файла
        ifstream in_file(raw_log_file);
        if (!in_file.is_open()) return;
        
        vector<string> records;
        string line;
        while (getline(in_file, line)) {
            if (!line.empty()) {
                records.push_back(line);
            }
        }
        in_file.close();
        
        // Если записей больше лимита, оставляем только последние MAX_RAW_RECORDS
        if (records.size() > MAX_RAW_RECORDS) {
            size_t start_index = records.size() - MAX_RAW_RECORDS;
            
            // Перезаписываем файл только с последними записями
            ofstream out_file(raw_log_file);
            if (out_file.is_open()) {
                for (size_t i = start_index; i < records.size(); ++i) {
                    out_file << records[i] << endl;
                }
                raw_records_count = records.size() - start_index;
            }
        }
        
        needs_raw_cleanup = false;
    }
    
    // Дозапись часовых средних
    void append_hourly_average(const string& timestamp, float avg_temp) {
        lock_guard<mutex> lock(file_mutex);
        
        ofstream out_file(hourly_log_file, ios::app);
        if (out_file.is_open()) {
            out_file << timestamp << "," 
                    << fixed << setprecision(2) << avg_temp << endl;
            hourly_records_count++;
            
            if (hourly_records_count > MAX_HOURLY_RECORDS) {
                needs_hourly_cleanup = true;
            }
        }
    }
    
    // Очистка старых часовых записей
    void cleanup_hourly_log() {
        lock_guard<mutex> lock(file_mutex);
        
        if (!needs_hourly_cleanup || hourly_records_count <= MAX_HOURLY_RECORDS) {
            return;
        }
        
        ifstream in_file(hourly_log_file);
        if (!in_file.is_open()) return;
        
        vector<string> records;
        string line;
        while (getline(in_file, line)) {
            if (!line.empty()) {
                records.push_back(line);
            }
        }
        in_file.close();
        
        if (records.size() > MAX_HOURLY_RECORDS) {
            size_t start_index = records.size() - MAX_HOURLY_RECORDS;
            
            ofstream out_file(hourly_log_file);
            if (out_file.is_open()) {
                for (size_t i = start_index; i < records.size(); ++i) {
                    out_file << records[i] << endl;
                }
                hourly_records_count = records.size() - start_index;
            }
        }
        
        needs_hourly_cleanup = false;
    }
    
    // Дозапись дневных средних
    void append_daily_average(const string& date, float avg_temp) {
        lock_guard<mutex> lock(file_mutex);
        
        ofstream out_file(daily_log_file, ios::app);
        if (out_file.is_open()) {
            out_file << date << "," 
                    << fixed << setprecision(2) << avg_temp << endl;
            daily_records_count++;
            
            if (daily_records_count > MAX_DAILY_RECORDS) {
                needs_daily_cleanup = true;
            }
        }
    }
    
    // Очистка старых дневных записей
    void cleanup_daily_log() {
        lock_guard<mutex> lock(file_mutex);
        
        if (!needs_daily_cleanup || daily_records_count <= MAX_DAILY_RECORDS) {
            return;
        }
        
        ifstream in_file(daily_log_file);
        if (!in_file.is_open()) return;
        
        vector<string> records;
        string line;
        while (getline(in_file, line)) {
            if (!line.empty()) {
                records.push_back(line);
            }
        }
        in_file.close();
        
        if (records.size() > MAX_DAILY_RECORDS) {
            size_t start_index = records.size() - MAX_DAILY_RECORDS;
            
            ofstream out_file(daily_log_file);
            if (out_file.is_open()) {
                for (size_t i = start_index; i < records.size(); ++i) {
                    out_file << records[i] << endl;
                }
                daily_records_count = records.size() - start_index;
            }
        }
        
        needs_daily_cleanup = false;
    }
    
    // если логер упадет то новый запуск подгрузит данные из файла
    size_t count_file_lines(const string& filename) {
        if (!file_exists(filename)) return 0;
        
        ifstream in_file(filename);
        if (!in_file.is_open()) return 0;
        
        size_t count = 0;
        string line;
        while (getline(in_file, line)) {
            if (!line.empty()) count++;
        }
        
        return count;
    }
    
    // Данные для расчета средних
    vector<TemperatureData> hourly_calc_buffer;
    map<string, vector<float>> daily_calc_buffer;
    
    void process_hourly_average() {
        lock_guard<mutex> lock(data_mutex);
        
        if (hourly_calc_buffer.empty()) return;
        
        // Вычисляем среднюю температуру за час
        float sum = 0;
        for (const auto& data : hourly_calc_buffer) {
            sum += data.temperature;
        }
        float avg_temp = sum / hourly_calc_buffer.size();
        
        // Получаем timestamp первого измерения в часе
        string hour_timestamp = hourly_calc_buffer.front().timestamp.substr(0, 13) + ":00:00.000";
        
        // Сохраняем результат
        append_hourly_average(hour_timestamp, avg_temp);
        
        // Очищаем буфер для расчета
        hourly_calc_buffer.clear();

        // Периодически проверяем необходимость очистки
        static size_t cleanup_counter = 0;
        if (++cleanup_counter >= CLEANUP_CHECK_INTERVAL) {
            cleanup_counter = 0;
            if (needs_hourly_cleanup) {
                cleanup_hourly_log();
            }
        }
    }
    
    void process_daily_average() {
        lock_guard<mutex> lock(data_mutex);
        
        // Получаем вчерашнюю дату
        time_t now = time(nullptr);
        tm* tm_now = localtime(&now);
        tm_now->tm_mday -= 1;
        mktime(tm_now);
        
        string date_key;

        ostringstream oss;
        oss << put_time(tm_now, "%Y-%m-%d");
        date_key = oss.str();
        
        auto it = daily_calc_buffer.find(date_key);
        if (it == daily_calc_buffer.end() || it->second.empty()) {
            return;
        }
        
        // Вычисляем среднюю температуру за день
        float sum = 0;
        for (float temp : it->second) {
            sum += temp;
        }
        float avg_temp = sum / it->second.size();
        
        // Сохраняем результат (дозапись в конец файла)
        append_daily_average(date_key, avg_temp);
        
        // Удаляем обработанные данные
        daily_calc_buffer.erase(it);
        
        // Периодически проверяем необходимость очистки
        static size_t cleanup_counter = 0;
        if (++cleanup_counter >= CLEANUP_CHECK_INTERVAL) {
            cleanup_counter = 0;
            if (needs_daily_cleanup) {
                cleanup_daily_log();
            }
        }
    }
    
public:
    TemperatureLogger(const string& raw_log, const string& hourly_log, const string& daily_log)
        : raw_log_file(raw_log), hourly_log_file(hourly_log), daily_log_file(daily_log) {
        last_hour_check = time(nullptr);
        last_day_check = time(nullptr);
        
        // Подсчитываем количество записей в файлах при старте
        raw_records_count = count_file_lines(raw_log_file);
        hourly_records_count = count_file_lines(hourly_log_file);
        daily_records_count = count_file_lines(daily_log_file);
        
        // Устанавливаем флаги очистки если нужно
        if (raw_records_count > MAX_RAW_RECORDS) needs_raw_cleanup = true;
        if (hourly_records_count > MAX_HOURLY_RECORDS) needs_hourly_cleanup = true;
        if (daily_records_count > MAX_DAILY_RECORDS) needs_daily_cleanup = true;
        
        // Делаем очистку если нужно
        if (needs_raw_cleanup) cleanup_raw_log();
        if (needs_hourly_cleanup) cleanup_hourly_log();
        if (needs_daily_cleanup) cleanup_daily_log();

    }
    
    void add_data(const TemperatureData& data) {
        append_raw_log(data);
        
        lock_guard<mutex> lock(data_mutex);
        hourly_calc_buffer.push_back(data);
        
        // Добавляем в дневные данные
        string date_key = data.timestamp.substr(0, 10); 
        daily_calc_buffer[date_key].push_back(data.temperature);

        // Периодически проверяем необходимость очистки лога
        static size_t raw_cleanup_counter = 0;
        if (++raw_cleanup_counter >= CLEANUP_CHECK_INTERVAL) {
            raw_cleanup_counter = 0;
            if (needs_raw_cleanup) {
                cleanup_raw_log();
            }
        }
        
        // Проверяем, не прошел ли час
        time_t current_time = time(nullptr);
        if (difftime(current_time, last_hour_check) >= 60 * 60) {
            process_hourly_average();
            last_hour_check = current_time;
        }
        
        // Проверяем, не прошел ли день
        if (difftime(current_time, last_day_check) >= 24 * 60 * 60) {
            process_daily_average();
            last_day_check = current_time;
        }
    }
    
    void cleanup() {
        // При завершении программы обрабатываем оставшиеся данные
        process_hourly_average();
        process_daily_average();
        
        // Финальная очистка если нужно
        if (needs_raw_cleanup) cleanup_raw_log();
        if (needs_hourly_cleanup) cleanup_hourly_log();
        if (needs_daily_cleanup) cleanup_daily_log();
    }
    
    // Парсер джейсончика
    bool parse_and_add_data(const string& json_str) {
        TemperatureData data;
        if (parse_json(json_str, data)) {
            add_data(data);
            return true;
        }
        return false;
    }
};

void read_from_port(SerialPort& port, TemperatureLogger& logger) {
    string buffer;
    char read_buf[1024];
    
    while (g_running) {
        size_t bytes_read = 0;
        int result = port.Read(read_buf, sizeof(read_buf) - 1, &bytes_read);
        
        if (result == SerialPort::RE_OK && bytes_read > 0) {
            read_buf[bytes_read] = '\0';
            buffer += read_buf;

            size_t pos = 0;
            while ((pos = buffer.find("}\r\n")) != string::npos) {
                string json_str = buffer.substr(0, pos + 1);
                buffer.erase(0, pos + 3); // +3 для "}\r\n"
                
                if (logger.parse_and_add_data(json_str)) {
                    cout << "Received data\n";
                } else {
                    cout << "error\n";
                }
            }
        } else if (result != SerialPort::RE_OK) {
            this_thread::sleep_for(chrono::milliseconds(100));
        }
        
        this_thread::sleep_for(chrono::milliseconds(10));
    }
}

int main(int argc, char* argv[]) {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    if (argc < 1) {
        return 1;
    }
    
    string port_name = argv[1];
    
    // Инициализация логгера
    TemperatureLogger logger("raw.log", 
                           "hourly_avg.log", 
                           "daily_avg.log");
    
    try {
        SerialPort port;

        string baud_rate = "115200";
        SerialPort::Parameters params(baud_rate.c_str());

        params.timeout = 1.0;
        
        int result = port.Open(port_name, params);
        if (result != SerialPort::RE_OK) {
            cout << "Failed to open port\n";
            return 1;
        }
        
        read_from_port(port, logger);
        
        port.Close();
        
    } catch (const exception& e) {
        cout << "Error" << e.what() << endl;
        return 1;
    }
    
    logger.cleanup();
    return 0;
}