#ifndef DATABASE_HPP
#define DATABASE_HPP

#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include <memory>
#include <iostream>
#include <iomanip>
#include <ctime>
#include "sqlite3.h"

class Database {
private:
    sqlite3* db;
    std::mutex db_mutex;
    
public:
    struct TemperatureRecord {
        std::string timestamp;
        double temperature;
        std::string date;
        std::string hour;
        
        TemperatureRecord() : temperature(0.0) {}
        TemperatureRecord(const std::string& ts, double temp, 
                         const std::string& d, const std::string& h)
            : timestamp(ts), temperature(temp), date(d), hour(h) {}
    };
    
    struct Statistics {
        double avg_temp;
        double min_temp;
        double max_temp;
        int sample_count;
        
        Statistics() : avg_temp(0), min_temp(0), max_temp(0), sample_count(0) {}
        Statistics(double avg, double min, double max, int count) 
            : avg_temp(avg), min_temp(min), max_temp(max), sample_count(count) {}
    };
    
    // Конструктор
    Database() : db(nullptr) {}
    
    // Деструктор
    ~Database() { 
        close(); 
    }
    
    bool open(const std::string& filename) {
        std::lock_guard<std::mutex> lock(db_mutex);
        
        if (db) return true; 
        
        int rc = sqlite3_open(filename.c_str(), &db);
        if (rc != SQLITE_OK) {
            std::cerr << sqlite3_errmsg(db) << std::endl;
            sqlite3_close(db);
            db = nullptr;
            return false;
        }
        
        std::cout << "Database opened\n";
        
        execute("PRAGMA foreign_keys = ON");
        execute("PRAGMA journal_mode = WAL");
        execute("PRAGMA synchronous = NORMAL");
        
        createTables();
        
        return true;
    }
    
    void close() {
        std::lock_guard<std::mutex> lock(db_mutex);
        
        if (db) {
            sqlite3_close(db);
            db = nullptr;
            std::cout << "Database closed\n";
        }
    }
    
    bool execute(const std::string& sql) {
        if (!db) {
            std::cerr << "Database not opened\n";
            return false;
        }
        
        char* errMsg = nullptr;
        int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errMsg);
        
        if (rc != SQLITE_OK) {
            std::cerr << errMsg << "\n";
            if (errMsg) sqlite3_free(errMsg);
            return false;
        }
        
        return true;
    }
    
    void createTables() {
        execute(R"(
            CREATE TABLE IF NOT EXISTS temperature_raw (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                timestamp TEXT NOT NULL,
                temperature REAL NOT NULL,
                date TEXT NOT NULL,
                hour TEXT NOT NULL,
                created_at DATETIME DEFAULT CURRENT_TIMESTAMP
            )
        )");
        
        execute(R"(
            CREATE TABLE IF NOT EXISTS temperature_hourly (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                timestamp TEXT NOT NULL,
                avg_temperature REAL NOT NULL,
                min_temperature REAL NOT NULL,
                max_temperature REAL NOT NULL,
                sample_count INTEGER NOT NULL,
                UNIQUE(timestamp)
            )
        )");
        
        execute(R"(
            CREATE TABLE IF NOT EXISTS temperature_daily (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                date TEXT NOT NULL,
                avg_temperature REAL NOT NULL,
                min_temperature REAL NOT NULL,
                max_temperature REAL NOT NULL,
                sample_count INTEGER NOT NULL,
                UNIQUE(date)
            )
        )");
    }
    
    bool insertRawData(const TemperatureRecord& record) {
        std::lock_guard<std::mutex> lock(db_mutex);
        
        if (!db) {
            std::cerr << "Database not opened for insert\n";
            return false;
        }
        
        sqlite3_stmt* stmt;
        const char* sql = R"(
            INSERT INTO temperature_raw (timestamp, temperature, date, hour)
            VALUES (?, ?, ?, ?)
        )";
        
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << sqlite3_errmsg(db) << "\n";
            return false;
        }
        
        sqlite3_bind_text(stmt, 1, record.timestamp.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_double(stmt, 2, record.temperature);
        sqlite3_bind_text(stmt, 3, record.date.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, record.hour.c_str(), -1, SQLITE_STATIC);
        
        int rc = sqlite3_step(stmt);
        bool success = (rc == SQLITE_DONE);
        
        if (success) {
            std::cout << "Data inserted\n";
        } else {
            std::cerr << "Failed to insert data\n";
        }
        
        sqlite3_finalize(stmt);
        return success;
    }
    
    bool insertHourlyAverage(const std::string& timestamp, double avg_temp, 
                           double min_temp, double max_temp, int count) {
        std::lock_guard<std::mutex> lock(db_mutex);
        
        if (!db) return false;
        
        sqlite3_stmt* stmt;
        const char* sql = R"(
            INSERT OR REPLACE INTO temperature_hourly 
            (timestamp, avg_temperature, min_temperature, max_temperature, sample_count)
            VALUES (?, ?, ?, ?, ?)
        )";
        
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            return false;
        }
        
        sqlite3_bind_text(stmt, 1, timestamp.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_double(stmt, 2, avg_temp);
        sqlite3_bind_double(stmt, 3, min_temp);
        sqlite3_bind_double(stmt, 4, max_temp);
        sqlite3_bind_int(stmt, 5, count);
        
        bool result = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
        
        return result;
    }
    
    bool insertDailyAverage(const std::string& date, double avg_temp,
                          double min_temp, double max_temp, int count) {
        std::lock_guard<std::mutex> lock(db_mutex);
        
        if (!db) return false;
        
        sqlite3_stmt* stmt;
        const char* sql = R"(
            INSERT OR REPLACE INTO temperature_daily 
            (date, avg_temperature, min_temperature, max_temperature, sample_count)
            VALUES (?, ?, ?, ?, ?)
        )";
        
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            return false;
        }
        
        sqlite3_bind_text(stmt, 1, date.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_double(stmt, 2, avg_temp);
        sqlite3_bind_double(stmt, 3, min_temp);
        sqlite3_bind_double(stmt, 4, max_temp);
        sqlite3_bind_int(stmt, 5, count);
        
        bool result = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
        
        return result;
    }
    
    std::vector<Database::TemperatureRecord> getRawData(const std::string& start_time, 
                                                        const std::string& end_time, 
                                                        int limit = 1000) {
        std::lock_guard<std::mutex> lock(db_mutex);
        std::vector<TemperatureRecord> results;
        
        if (!db) return results;
        
        sqlite3_stmt* stmt;
        const char* sql = R"(
            SELECT timestamp, temperature, date, hour 
            FROM temperature_raw 
            WHERE timestamp BETWEEN ? AND ?
            ORDER BY timestamp DESC
            LIMIT ?
        )";
        
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << sqlite3_errmsg(db) << "\n";
            return results;
        }
        
        sqlite3_bind_text(stmt, 1, start_time.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, end_time.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 3, limit);
        
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            TemperatureRecord record;
            record.timestamp = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            record.temperature = sqlite3_column_double(stmt, 1);
            record.date = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            record.hour = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            results.push_back(record);
        }
        
        sqlite3_finalize(stmt);
        return results;
    }
    
    std::vector<std::pair<std::string, double>> getHourlyAverages(const std::string& start_date,
                                                                 const std::string& end_date) {
        std::lock_guard<std::mutex> lock(db_mutex);
        std::vector<std::pair<std::string, double>> results;
        
        if (!db) return results;
        
        sqlite3_stmt* stmt;
        const char* sql = R"(
            SELECT timestamp, avg_temperature 
            FROM temperature_hourly 
            WHERE date(timestamp) BETWEEN ? AND ?
            ORDER BY timestamp
        )";
        
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << sqlite3_errmsg(db) << "\n";
            return results;
        }
        
        sqlite3_bind_text(stmt, 1, start_date.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, end_date.c_str(), -1, SQLITE_STATIC);
        
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            std::string timestamp = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            double avg_temp = sqlite3_column_double(stmt, 1);
            results.emplace_back(timestamp, avg_temp);
        }
        
        sqlite3_finalize(stmt);
        return results;
    }
    
    std::vector<std::pair<std::string, double>> getDailyAverages(const std::string& start_date,
                                                                const std::string& end_date) {
        std::lock_guard<std::mutex> lock(db_mutex);
        std::vector<std::pair<std::string, double>> results;
        
        if (!db) return results;
        
        sqlite3_stmt* stmt;
        const char* sql = R"(
            SELECT date, avg_temperature 
            FROM temperature_daily 
            WHERE date BETWEEN ? AND ?
            ORDER BY date
        )";
        
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << sqlite3_errmsg(db) << "\n";
            return results;
        }
        
        sqlite3_bind_text(stmt, 1, start_date.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, end_date.c_str(), -1, SQLITE_STATIC);
        
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            std::string date = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            double avg_temp = sqlite3_column_double(stmt, 1);
            results.emplace_back(date, avg_temp);
        }
        
        sqlite3_finalize(stmt);
        return results;
    }
    
    double getCurrentTemperature() {
        std::lock_guard<std::mutex> lock(db_mutex);
        
        if (!db) return 0.0;
        
        sqlite3_stmt* stmt;
        const char* sql = R"(
            SELECT temperature 
            FROM temperature_raw 
            ORDER BY timestamp DESC 
            LIMIT 1
        )";
        
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            return 0.0;
        }
        
        double result = 0.0;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            result = sqlite3_column_double(stmt, 0);
        }
        
        sqlite3_finalize(stmt);
        return result;
    }
    
    Statistics getStatistics(const std::string& start_time, const std::string& end_time) {
        std::lock_guard<std::mutex> lock(db_mutex);
        Statistics stats;
        
        if (!db) return stats;
        
        sqlite3_stmt* stmt;
        const char* sql = R"(
            SELECT 
                AVG(temperature),
                MIN(temperature),
                MAX(temperature),
                COUNT(*)
            FROM temperature_raw 
            WHERE timestamp BETWEEN ? AND ?
        )";
        
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            return stats;
        }
        
        sqlite3_bind_text(stmt, 1, start_time.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, end_time.c_str(), -1, SQLITE_STATIC);
        
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            stats.avg_temp = sqlite3_column_double(stmt, 0);
            stats.min_temp = sqlite3_column_double(stmt, 1);
            stats.max_temp = sqlite3_column_double(stmt, 2);
            stats.sample_count = sqlite3_column_int(stmt, 3);
        }
        
        sqlite3_finalize(stmt);
        return stats;
    }
    
    bool cleanupOldData(int keep_days = 30) {
        std::lock_guard<std::mutex> lock(db_mutex);
        
        if (!db) return false;
        
        std::string sql = R"(
            DELETE FROM temperature_raw 
            WHERE date < date('now', '-' || ? || ' days')
        )";
        
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            return false;
        }
        
        sqlite3_bind_int(stmt, 1, keep_days);
        bool result = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
        
        return result;
    }
};

#endif