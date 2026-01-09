#include <iostream>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <atomic>
#include <csignal>
#include <vector>
#include <string>

#include "my_serial.hpp"

using namespace cplib;
using namespace std;

// Глобальный флаг для ctr + C
atomic<bool> g_running(true);

void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        g_running = false;
    }
}

// Глобальная переменная для последней температуры
float last_temp = 20.0f;

// Гипер реалистичная генерация - все тз
float generate_temperature() {
    float change = (rand() % 100 - 50) / 100.0f;
    last_temp += change;
    
    if (last_temp < -10.0f) last_temp = -10.0f;
    if (last_temp > 30.0f) last_temp = 30.0f;

    return last_temp;
}

string get_current_time() {
    auto now = chrono::system_clock::now();
    auto now_time_t = chrono::system_clock::to_time_t(now);
    auto now_ms = chrono::duration_cast<chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    stringstream ss;
    ss << put_time(localtime(&now_time_t), "%Y-%m-%d %H:%M:%S") << '.' << setfill('0') << setw(3) << now_ms.count();
    return ss.str();
}

// Функция эмуляции датчика
void emulate_temperature_sensor(SerialPort& port, int interval_ms) {   
    while (g_running) {
        float temperature = generate_temperature();
        string timestamp = get_current_time();
        
        // Формируем сообщение в формате JSON с контрольной суммой
        stringstream message;
        message << fixed << setprecision(2);
        message << "{";
        message << "\"temperature\": " << temperature << ", ";
        message << "\"timestamp\": \"" << timestamp << "\", ";
        message << "\"checksum\": " << temperature; // допустим это контрольная сумма
        message << "}\r\n"; 
        
        string message_str = message.str();
        
        // Отправляем данные в порт
        int result = port.Write(message_str);
        
        if (result == SerialPort::RE_OK) {
            cout << "message send\n"; 
        } else {
            cout << "error\n";
        }
        
        this_thread::sleep_for(chrono::milliseconds(interval_ms));
    }
}

int main(int argc, char* argv[]) {
    // гспч
    srand(static_cast<unsigned int>(time(nullptr)));
    
    // включаем обработку сигналов
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    string port_name;
    if (argc > 1) {
        port_name = argv[1];
    } else {
        return 1; 
    }

    try {
        SerialPort port;
        
        SerialPort::Parameters params(SerialPort::BAUDRATE_115200);
        params.timeout = 1.0; 
        
        // Открываем порт
        int result = port.Open(port_name, params);
        
        if (result != SerialPort::RE_OK) {
            cout << "Error while opening\n"; 
            return 1;
        }
        
        // Запускаем эмуляцию датчика
        int interval_ms = 1000; 
        emulate_temperature_sensor(port, interval_ms);
        
        port.Close();

    } catch(...) {
        cout << "Error while opening\n"; 
        return 1;
    }
    
    return 0;
}