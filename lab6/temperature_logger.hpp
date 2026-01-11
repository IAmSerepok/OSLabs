#ifndef TEMPERATURE_LOGGER_HPP
#define TEMPERATURE_LOGGER_HPP

#include "database.hpp"
#include "my_serial.hpp"

#include <string>
#include <vector>
#include <map>
#include <atomic>
#include <mutex>
#include <thread>
#include <chrono>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <ctime>


class TemperatureLogger {
private:
    Database db;
    cplib::SerialPort* serial_port;
    
    std::atomic<bool> running;
    std::thread read_thread;
    std::mutex data_mutex;
    
    // Буферы для расчета средних
    std::vector<Database::TemperatureRecord> hourly_buffer;
    std::map<std::string, std::vector<double>> daily_buffer;
    
    time_t last_hour_check;
    time_t last_day_check;
    time_t last_cleanup_check;
    
    bool parse_json(const std::string& json_str, Database::TemperatureRecord& record) {
        size_t temp_pos = json_str.find("\"temperature\":");
        size_t checksum_pos = json_str.find("\"checksum\":");
        size_t time_pos = json_str.find("\"timestamp\":");
        
        if (temp_pos == std::string::npos || checksum_pos == std::string::npos || time_pos == std::string::npos) {
            temp_pos = json_str.find("\"temp\":");
            checksum_pos = json_str.find("\"chk\":");
            time_pos = json_str.find("\"time\":");
            
            if (temp_pos == std::string::npos || checksum_pos == std::string::npos || time_pos == std::string::npos) {
                std::cerr << "Invalid JSON\n";
                return false;
            }
            
            temp_pos += 7;
            checksum_pos += 7;
            time_pos += 7;
        } else {
            temp_pos += 14;
            checksum_pos += 11;
            time_pos += 13;
        }
        
        size_t temp_end = json_str.find_first_of(",}", temp_pos);
        if (temp_end == std::string::npos) return false;
        
        std::string temp_str = json_str.substr(temp_pos, temp_end - temp_pos);
        double temperature;
        try {
            temperature = std::stod(temp_str);
        } catch (...) {
            std::cerr << "Failed to parse temperature\n";
            return false;
        }
        
        size_t checksum_end = json_str.find_first_of(",}", checksum_pos);
        if (checksum_end == std::string::npos) return false;
        
        std::string checksum_str = json_str.substr(checksum_pos, checksum_end - checksum_pos);
        double checksum;
        try {
            checksum = std::stod(checksum_str);
        } catch (...) {
            std::cerr << "Failed to parse checksum\n";
            return false;
        }
        
        const double EPSILON = 0.01;
        if (std::fabs(temperature - checksum) > EPSILON) {
            std::cerr << "Checksum error\n";
            return false;
        }
        
        size_t time_end = json_str.find('"', time_pos + 1);
        if (time_end == std::string::npos) {
            time_end = json_str.find_first_of(",}", time_pos);
            if (time_end == std::string::npos) return false;
            record.timestamp = json_str.substr(time_pos, time_end - time_pos);
        } else {
            record.timestamp = json_str.substr(time_pos + 1, time_end - time_pos - 1);
        }
        
        record.temperature = temperature;
        
        if (record.timestamp.length() >= 10) {
            record.date = record.timestamp.substr(0, 10);
        } else {
            time_t now = time(nullptr);
            struct tm* tm_info = localtime(&now);
            char date_buffer[11];
            strftime(date_buffer, sizeof(date_buffer), "%Y-%m-%d", tm_info);
            record.date = date_buffer;
        }
        
        if (record.timestamp.length() >= 13) {
            record.hour = record.timestamp.substr(0, 13) + ":00:00.000";
        } else {
            time_t now = time(nullptr);
            struct tm* tm_info = localtime(&now);
            char hour_buffer[14];
            strftime(hour_buffer, sizeof(hour_buffer), "%Y-%m-%d %H:00:00.000", tm_info);
            record.hour = hour_buffer;
        }
        
        return true;
    }
    
    void processHourlyBuffer() {
        std::lock_guard<std::mutex> lock(data_mutex);
        
        if (hourly_buffer.empty()) return;
        
        std::map<std::string, std::vector<double>> hourly_data;
        for (const auto& record : hourly_buffer) {
            hourly_data[record.hour].push_back(record.temperature);
        }
        
        for (const auto& pair : hourly_data) {
            const std::string& hour = pair.first;
            const std::vector<double>& temps = pair.second;
            
            if (temps.empty()) continue;
            
            double sum = 0;
            double min_temp = temps[0];
            double max_temp = temps[0];
            
            for (double temp : temps) {
                sum += temp;
                if (temp < min_temp) min_temp = temp;
                if (temp > max_temp) max_temp = temp;
            }
            
            double avg_temp = sum / temps.size();
            db.insertHourlyAverage(hour, avg_temp, min_temp, max_temp, temps.size());
            
            std::cout << "Hourly average calculated\n";
        }
        
        hourly_buffer.clear();
    }
    
    void processDailyBuffer() {
        std::lock_guard<std::mutex> lock(data_mutex);
        
        if (daily_buffer.empty()) return;
        
        for (const auto& pair : daily_buffer) {
            const std::string& date = pair.first;
            const std::vector<double>& temps = pair.second;
            
            if (temps.empty()) continue;
            
            double sum = 0;
            double min_temp = temps[0];
            double max_temp = temps[0];
            
            for (double temp : temps) {
                sum += temp;
                if (temp < min_temp) min_temp = temp;
                if (temp > max_temp) max_temp = temp;
            }
            
            double avg_temp = sum / temps.size();
            db.insertDailyAverage(date, avg_temp, min_temp, max_temp, temps.size());
            
            std::cout << "Daily average calculated\n";
        }
        
        daily_buffer.clear();
    }
    
    void cleanupOldData() {
        db.cleanupOldData(30);
    }
    
public:
    TemperatureLogger() 
        : serial_port(nullptr), running(false),
          last_hour_check(time(nullptr)),
          last_day_check(time(nullptr)),
          last_cleanup_check(time(nullptr)) {}
    
    ~TemperatureLogger() {
        stop();
    }
    
    bool initialize(const std::string& db_file, const std::string& port_name) {
        if (!db.open(db_file)) {
            std::cerr << "Failed to open database\n";
            return false;
        }
        
        serial_port = new cplib::SerialPort();
        cplib::SerialPort::Parameters params("115200");
        params.timeout = 1.0;
        
        int result = serial_port->Open(port_name, params);
        if (result != cplib::SerialPort::RE_OK) {
            std::cerr << "Failed to open serial port\n";
            delete serial_port;
            serial_port = nullptr;
            return false;
        }

        return true;
    }
    
    void start() {
        if (running) return;
        
        running = true;
        
        read_thread = std::thread([this]() {
            std::string buffer;
            char read_buf[1024];
            
            while (running) {
                size_t bytes_read = 0;
                int result = serial_port->Read(read_buf, sizeof(read_buf) - 1, &bytes_read);
                
                if (result == cplib::SerialPort::RE_OK && bytes_read > 0) {
                    read_buf[bytes_read] = '\0';
                    buffer += read_buf;
                    
                    size_t pos = 0;
                    while ((pos = buffer.find('\n')) != std::string::npos) {
                        std::string line = buffer.substr(0, pos);
                        buffer.erase(0, pos + 1);
                        
                        while (!line.empty() && (line[0] == '\r' || line[0] == '\n' || line[0] == ' ' || line[0] == '\t')) {
                            line.erase(0, 1);
                        }
                        
                        if (line.empty()) continue;
                        
                        Database::TemperatureRecord record;
                        if (parse_json(line, record)) {
                            if (db.insertRawData(record)) {
                                {
                                    std::lock_guard<std::mutex> lock(data_mutex);
                                    hourly_buffer.push_back(record);
                                    daily_buffer[record.date].push_back(record.temperature);
                                }
                                std::cout << "Data received\n";
                            }
                        } else {
                            std::cerr << "Failed to parse JSON\n";
                        }
                    }
                } else if (result != cplib::SerialPort::RE_OK) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
                
                time_t current_time = time(nullptr);
                
                if (difftime(current_time, last_hour_check) >= 60 * 60) {
                    processHourlyBuffer();
                    last_hour_check = current_time;
                }
                
                if (difftime(current_time, last_day_check) >= 24 * 60 * 60) {
                    processDailyBuffer();
                    last_day_check = current_time;
                }
                
                if (difftime(current_time, last_cleanup_check) >= 24 * 60 * 60) {
                    cleanupOldData();
                    last_cleanup_check = current_time;
                }
                
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });
    }
    
    void stop() {
        running = false;
        
        if (read_thread.joinable()) {
            read_thread.join();
        }
        
        processHourlyBuffer();
        processDailyBuffer();
        
        if (serial_port) {
            serial_port->Close();
            delete serial_port;
            serial_port = nullptr;
        }
        
        db.close();
    }
    
    // Статистика
    double getCurrentTemperature() { return db.getCurrentTemperature(); }
    Database::Statistics getStatistics(const std::string& start, const std::string& end) {
        return db.getStatistics(start, end);
    }
    
    // Для доступа к базе данных из других компонентов
    Database& getDatabase() { return db; }
};

#endif