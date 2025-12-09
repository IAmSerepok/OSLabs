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
    
    vector<TemperatureData> hourly_data;
    map<string, vector<float>> daily_data;
    time_t last_hour_check;
    time_t last_day_check;
    
    mutex data_mutex;
    mutex file_mutex;
    
    bool parse_json(const string& json_str, TemperatureData& data) {
        size_t temp_pos = json_str.find("\"temperature\":");
        size_t time_pos = json_str.find("\"timestamp\":");
        
        if (temp_pos == string::npos || time_pos == string::npos) {
            return false;
        }
        
        temp_pos += 14; // Длина "\"temperature\":"
        size_t temp_end = json_str.find(',', temp_pos);
        if (temp_end == string::npos) return false;
        
        string temp_str = json_str.substr(temp_pos, temp_end - temp_pos);
        try {
            data.temperature = stof(temp_str);
        } catch (...) {
            return false;
        }
        
        time_pos += 13; // Длина "\"timestamp\":"
        size_t time_end = json_str.find('"', time_pos + 1);
        if (time_end == string::npos) return false;
        
        data.timestamp = json_str.substr(time_pos + 1, time_end - time_pos - 1);
        
        return true;
    }
    
    void write_raw_log(const TemperatureData& data) {
        lock_guard<mutex> lock(file_mutex);
        
        // Читаем старые записи и оставляем только последние 24 часа
        vector<string> valid_entries;
        ifstream in_file(raw_log_file);
        string line;
        
        if (in_file.is_open()) {
            while (getline(in_file, line)) {
                if (!line.empty() && is_entry_within_24h(line)) {
                    valid_entries.push_back(line);
                }
            }
            in_file.close();
        }
        
        // Добавляем новую запись
        valid_entries.push_back(data.timestamp + "," + to_string(data.temperature));
        
        // Сохраняем обратно
        ofstream out_file(raw_log_file);
        if (out_file.is_open()) {
            for (const auto& entry : valid_entries) {
                out_file << entry << endl;
            }
            out_file.close();
        }
    }
    
    bool is_entry_within_24h(const string& entry) {
        size_t comma_pos = entry.find(',');
        if (comma_pos == string::npos) return false;
        
        string timestamp_str = entry.substr(0, comma_pos);
        
        tm tm_time = {};
        istringstream ss(timestamp_str);
        ss >> get_time(&tm_time, "%Y-%m-%d %H:%M:%S");
        if (ss.fail()) {
            size_t dot_pos = timestamp_str.find('.');
            if (dot_pos != string::npos) {
                timestamp_str = timestamp_str.substr(0, dot_pos);
                ss.clear();
                ss.str(timestamp_str);
                ss >> get_time(&tm_time, "%Y-%m-%d %H:%M:%S");
            }
            if (ss.fail()) return false;
        }
        
        time_t entry_time = mktime(&tm_time);
        time_t current_time = time(nullptr);
        
        return difftime(current_time, entry_time) <= 24 * 60 * 60; // 24 часа
    }
    
    void process_hourly_average() {
        lock_guard<mutex> lock(data_mutex);
        
        if (hourly_data.empty()) return;
        
        // Вычисляем среднюю температуру за час
        float sum = 0;
        for (const auto& data : hourly_data) {
            sum += data.temperature;
        }
        float avg_temp = sum / hourly_data.size();
        
        // Получаем timestamp первого измерения в часе
        string hour_timestamp = hourly_data.front().timestamp.substr(0, 13) + ":00:00.000";
        
        // Записываем в файл
        lock_guard<mutex> file_lock(file_mutex);
        
        // Читаем старые записи и оставляем только последний месяц
        vector<string> valid_entries;
        ifstream in_file(hourly_log_file);
        string line;
        
        if (in_file.is_open()) {
            while (getline(in_file, line)) {
                if (!line.empty() && is_entry_within_month(line)) {
                    valid_entries.push_back(line);
                }
            }
            in_file.close();
        }
        
        // Добавляем новую запись
        stringstream ss;
        ss << fixed << setprecision(2);
        ss << avg_temp;
        valid_entries.push_back(hour_timestamp + "," + ss.str());
        
        // Сохраняем обратно
        ofstream out_file(hourly_log_file);
        if (out_file.is_open()) {
            for (const auto& entry : valid_entries) {
                out_file << entry << endl;
            }
            out_file.close();
        }
        
        // Очищаем данные за час
        hourly_data.clear();
        
        cout << "Hourly average calculated\n";
    }
    
    bool is_entry_within_month(const string& entry) {
        size_t comma_pos = entry.find(',');
        if (comma_pos == string::npos) return false;
        
        string timestamp_str = entry.substr(0, comma_pos);
        
        // Парсим timestamp
        tm tm_time = {};
        istringstream ss(timestamp_str);
        ss >> get_time(&tm_time, "%Y-%m-%d %H:%M:%S");
        if (ss.fail()) {
            size_t dot_pos = timestamp_str.find('.');
            if (dot_pos != string::npos) {
                timestamp_str = timestamp_str.substr(0, dot_pos);
                ss.clear();
                ss.str(timestamp_str);
                ss >> get_time(&tm_time, "%Y-%m-%d %H:%M:%S");
            }
            if (ss.fail()) return false;
        }
        
        time_t entry_time = mktime(&tm_time);
        time_t current_time = time(nullptr);
        
        return difftime(current_time, entry_time) <= 30 * 24 * 60 * 60; // ну в среднем месяц
    }
    
    void process_daily_average() {
        lock_guard<mutex> lock(data_mutex);
        
        // Получаем вчерашнюю дату
        time_t now = time(nullptr);
        tm* tm_now = localtime(&now);
        tm_now->tm_mday -= 1;
        mktime(tm_now);
        
        string date_key;
        {
            ostringstream oss;
            oss << put_time(tm_now, "%Y-%m-%d");
            date_key = oss.str();
        }
        
        auto it = daily_data.find(date_key);
        if (it == daily_data.end() || it->second.empty()) {
            return;
        }
        
        // Вычисляем среднюю температуру за день
        float sum = 0;
        for (float temp : it->second) {
            sum += temp;
        }
        float avg_temp = sum / it->second.size();
        
        // Записываем в файл
        lock_guard<mutex> file_lock(file_mutex);
        
        ofstream out_file(daily_log_file, ios::app);
        if (out_file.is_open()) {
            stringstream ss;
            ss << fixed << setprecision(2);
            ss << avg_temp;
            out_file << date_key << "," << ss.str() << endl;
            out_file.close();
        }

        
        // Удаляем обработанные данные
        daily_data.erase(it);
        
        cout << "Daily average calculated\n";
    }
    
public:
    TemperatureLogger(const string& raw_log, const string& hourly_log, const string& daily_log)
        : raw_log_file(raw_log), hourly_log_file(hourly_log), daily_log_file(daily_log) {
        last_hour_check = time(nullptr);
        last_day_check = time(nullptr);
    }
    
    void add_data(const TemperatureData& data) {
        lock_guard<mutex> lock(data_mutex);
        hourly_data.push_back(data);
        
        // Добавляем в дневные данные
        string date_key = data.timestamp.substr(0, 10); // YYYY-MM-DD
        daily_data[date_key].push_back(data.temperature);
        
        // Записываем в raw лог
        write_raw_log(data);
        
        // Проверяем, не прошел ли час
        time_t current_time = time(nullptr);
        if (difftime(current_time, last_hour_check) >= 3600) {
            process_hourly_average();
            last_hour_check = current_time;
        }
        
        // Проверяем, не прошел ли день
        if (difftime(current_time, last_day_check) >= 86400) {
            process_daily_average();
            last_day_check = current_time;
        }
    }
    
    void cleanup() {
        // При завершении программы обрабатываем оставшиеся данные
        if (!hourly_data.empty()) {
            process_hourly_average();
        }
        
        // Обрабатываем данные за вчерашний день
        tm* tm_now = localtime(&last_day_check);
        tm_now->tm_mday -= 1;
        mktime(tm_now);
        
        string date_key;
        ostringstream oss;
        oss << put_time(tm_now, "%Y-%m-%d");
        date_key = oss.str();
        
        lock_guard<mutex> lock(data_mutex);
        auto it = daily_data.find(date_key);
        if (it != daily_data.end() && !it->second.empty()) {
            daily_data.erase(it); // Очищаем старые данные
        }
    }
    
    // Публичный метод для парсинга JSON
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
                    cout << "Failed\n";
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
        
        cout << "Port opened successfully\n";
        
        read_from_port(port, logger);
        
        port.Close();
        
    } catch (const exception& e) {
        cout << "Error" << e.what() << endl;
        return 1;
    }
    
    logger.cleanup();
    return 0;
}