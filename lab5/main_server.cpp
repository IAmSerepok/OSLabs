#include "temperature_logger.hpp"
#include "httpserver.hpp"
#include <iostream>
#include <csignal>
#include <atomic>
#include <thread>

std::atomic<bool> running(true);
TemperatureLogger* logger = nullptr;
HTTPServer* http_server = nullptr;

void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) running = false;
}

void server_loop() {
    while (running) {
        if (http_server) http_server->processClients();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        return 1;
    }
    
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    std::string serial_port = argv[1];
    std::string db_file = "temperature.db";
    
    try {
        // Инициализируем логгер
        logger = new TemperatureLogger();
        if (!logger->initialize(db_file, serial_port)) {
            std::cerr << "Failed to initialize logger\n";
            delete logger;
            return 1;
        }
        
        // Инициализируем HTTP сервер
        http_server = new HTTPServer(&logger->getDatabase(), "0.0.0.0", 8080);
        if (!http_server->start()) {
            std::cerr << "Failed to start HTTP server\n";
            delete http_server;
            delete logger;
            return 1;
        }
        
        // Запускаем логгер
        logger->start();
        
        // Запускаем поток для обработки HTTP запросов
        std::thread server_thread(server_loop);
        
        // Главный цикл
        while (running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        // Останавливаем все компоненты
        logger->stop();
        http_server->stop();
        
        if (server_thread.joinable()) {
            server_thread.join();
        }
        
        delete http_server;
        delete logger;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        if (http_server) delete http_server;
        if (logger) delete logger;
        return 1;
    }
    
    return 0;
}