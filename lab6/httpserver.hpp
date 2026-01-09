#ifndef HTTPSERVER_HPP
#define HTTPSERVER_HPP

#include "database.hpp"
#include <string>
#include <map>
#include <mutex>
#include <atomic>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <ctime>
#include <cstring>

#if defined (WIN32)
#   include <winsock2.h>
#   include <ws2tcpip.h>
#   pragma comment(lib, "ws2_32.lib")
#else
#   include <sys/socket.h>
#   include <netinet/in.h>
#   include <arpa/inet.h>
#   include <unistd.h>
#   include <poll.h>
#   define SOCKET int
#   define INVALID_SOCKET -1
#   define SOCKET_ERROR -1
#endif

#define READ_WAIT_MS 50

class HTTPServer {
private:
    Database* database;
    SOCKET server_socket;
    std::string server_ip;
    int server_port;
    std::atomic<bool> running;
    char input_buf[4096];
    
    // Сетевые функции
    static void initializeNetwork() {
        #if defined (WIN32)
            static bool initialized = false;
            if (!initialized) {
                WSADATA wsaData;
                if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
                    std::cerr << "WSAStartup failed" << std::endl;
                }
                initialized = true;
            }
        #else
            signal(SIGPIPE, SIG_IGN);
        #endif
    }
    
    static void cleanupNetwork() {
        #if defined (WIN32)
            WSACleanup();
        #endif
    }
    
    static int getErrorCode() {
        #if defined (WIN32)
            return WSAGetLastError();
        #else
            return errno;
        #endif
    }
    
    void closeSocket(SOCKET sock) {
        if (sock == INVALID_SOCKET) return;
        
        #if defined (WIN32)
            shutdown(sock, SD_SEND);
            closesocket(sock);
        #else
            shutdown(sock, SHUT_WR);
            close(sock);
        #endif
    }
    
    // Парсинг URL
    static std::string getPathFromRequest(const std::string& request) {
        size_t start = request.find(' ');
        if (start == std::string::npos) return "/";
        
        size_t end = request.find(' ', start + 1);
        if (end == std::string::npos) return "/";
        
        std::string path = request.substr(start + 1, end - start - 1);
        
        size_t qmark = path.find('?');
        if (qmark != std::string::npos) {
            path = path.substr(0, qmark);
        }
        
        return path.empty() ? "/" : path;
    }
    
    static std::map<std::string, std::string> getParamsFromRequest(const std::string& request) {
        std::map<std::string, std::string> params;
        
        size_t start = request.find(' ');
        if (start == std::string::npos) return params;
        
        size_t end = request.find(' ', start + 1);
        if (end == std::string::npos) return params;
        
        std::string full_path = request.substr(start + 1, end - start - 1);
        size_t qmark = full_path.find('?');
        
        if (qmark != std::string::npos) {
            std::string query = full_path.substr(qmark + 1);
            std::istringstream iss(query);
            std::string pair;
            
            while (std::getline(iss, pair, '&')) {
                size_t eq_pos = pair.find('=');
                if (eq_pos != std::string::npos) {
                    std::string key = pair.substr(0, eq_pos);
                    std::string value = pair.substr(eq_pos + 1);
                    params[key] = value;
                }
            }
        }
        
        return params;
    }
    
    // Обработка API запросов
    std::string handleAPI(const std::string& path, const std::map<std::string, std::string>& params) {
        std::ostringstream response;
        
        if (path == "/api/current") {
            double current_temp = database->getCurrentTemperature();
            response << "{\"temperature\": " << std::fixed << std::setprecision(2) << current_temp << "}";
            
        } else if (path == "/api/statistics") {
            auto start_it = params.find("start");
            auto end_it = params.find("end");
            
            if (start_it == params.end() || end_it == params.end()) {
                response << "{\"error\": \"Missing start or end parameters\"}";
            } else {
                Database::Statistics stats = database->getStatistics(start_it->second, end_it->second);
                response << "{";
                response << "\"average\": " << std::fixed << std::setprecision(2) << stats.avg_temp << ",";
                response << "\"min\": " << stats.min_temp << ",";
                response << "\"max\": " << stats.max_temp << ",";
                response << "\"samples\": " << stats.sample_count;
                response << "}";
            }
            
        } else if (path == "/api/raw") {
            auto start_it = params.find("start");
            auto end_it = params.find("end");
            auto limit_it = params.find("limit");
            
            if (start_it == params.end() || end_it == params.end()) {
                response << "{\"error\": \"Missing start or end parameters\"}";
            } else {
                int limit = 1000;
                if (limit_it != params.end()) {
                    limit = std::stoi(limit_it->second);
                }
                
                auto records = database->getRawData(start_it->second, end_it->second, limit);
                
                response << "[";
                for (size_t i = 0; i < records.size(); ++i) {
                    response << "{\"timestamp\": \"" << records[i].timestamp << "\", ";
                    response << "\"temperature\": " << std::fixed << std::setprecision(2) << records[i].temperature << "}";
                    if (i < records.size() - 1) response << ",";
                }
                response << "]";
            }
            
        } else if (path == "/api/hourly") {
            auto start_it = params.find("start");
            auto end_it = params.find("end");
            
            if (start_it == params.end() || end_it == params.end()) {
                response << "{\"error\": \"Missing start or end parameters\"}";
            } else {
                auto averages = database->getHourlyAverages(start_it->second, end_it->second);
                
                response << "[";
                for (size_t i = 0; i < averages.size(); ++i) {
                    response << "{\"timestamp\": \"" << averages[i].first << "\", ";
                    response << "\"temperature\": " << std::fixed << std::setprecision(2) << averages[i].second << "}";
                    if (i < averages.size() - 1) response << ",";
                }
                response << "]";
            }
            
        } else if (path == "/api/daily") {
            auto start_it = params.find("start");
            auto end_it = params.find("end");
            
            if (start_it == params.end() || end_it == params.end()) {
                response << "{\"error\": \"Missing start or end parameters\"}";
            } else {
                auto averages = database->getDailyAverages(start_it->second, end_it->second);
                
                response << "[";
                for (size_t i = 0; i < averages.size(); ++i) {
                    response << "{\"date\": \"" << averages[i].first << "\", ";
                    response << "\"temperature\": " << std::fixed << std::setprecision(2) << averages[i].second << "}";
                    if (i < averages.size() - 1) response << ",";
                }
                response << "]";
            }
            
        } else if (path == "/") {
            response << "{\"status\": \"running\"}";
            
        } else {
            response << "{\"error\": \"Unknown url\"}";
        }
        
        return response.str();
    }
    
    std::string handleRequest(const std::string& request) {
        std::string path = getPathFromRequest(request);
        auto params = getParamsFromRequest(request);
        
        std::string content_type = "application/json";
        std::string response_body = handleAPI(path, params);
        
        std::ostringstream response;
        response << "HTTP/1.1 200 OK\r\n"
                 << "Content-Type: " << content_type << "\r\n"
                 << "Content-Length: " << response_body.length() << "\r\n"
                 << "Access-Control-Allow-Origin: *\r\n"
                 << "Connection: close\r\n"
                 << "\r\n"
                 << response_body;
        
        return response.str();
    }
    
public:
    HTTPServer(Database* db, const std::string& ip = "0.0.0.0", int port = 8080)
        : database(db), server_socket(INVALID_SOCKET), 
          server_ip(ip), server_port(port), running(false) {
        
        initializeNetwork();
        memset(input_buf, 0, sizeof(input_buf));
    }
    
    ~HTTPServer() {
        stop();
        cleanupNetwork();
    }
    
    bool start() {
        if (running) return true;
        
        server_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (server_socket == INVALID_SOCKET) {
            std::cerr << "Can't open socket" << getErrorCode() << std::endl;
            return false;
        }
        
        int opt = 1;
        #if defined (WIN32)
            setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
        #else
            setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        #endif
        
        sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = inet_addr(server_ip.c_str());
        server_addr.sin_port = htons(server_port);
        
        if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
            std::cerr << "Failed to bind: " << getErrorCode() << std::endl;
            closeSocket(server_socket);
            server_socket = INVALID_SOCKET;
            return false;
        }
        
        if (listen(server_socket, SOMAXCONN) == SOCKET_ERROR) {
            std::cerr << "Failed to listen: " << getErrorCode() << std::endl;
            closeSocket(server_socket);
            server_socket = INVALID_SOCKET;
            return false;
        }
        
        running = true;
        return true;
    }
    
    void stop() {
        running = false;
        if (server_socket != INVALID_SOCKET) {
            closeSocket(server_socket);
            server_socket = INVALID_SOCKET;
        }
    }
    
    void processClients() {
        if (!running || server_socket == INVALID_SOCKET) return;
        
        struct pollfd pfd;
        memset(&pfd, 0, sizeof(pfd));
        pfd.fd = server_socket;
        pfd.events = POLLIN;
        
        #if defined(WIN32)
            int ret = WSAPoll(&pfd, 1, READ_WAIT_MS);
        #else
            int ret = poll(&pfd, 1, READ_WAIT_MS);
        #endif
        
        if (ret > 0 && (pfd.revents & POLLIN)) {
            SOCKET client_socket = accept(server_socket, NULL, NULL);
            if (client_socket == INVALID_SOCKET) {
                std::cerr << "Error: " << getErrorCode() << std::endl;
                return;
            }
            
            struct pollfd client_pfd;
            memset(&client_pfd, 0, sizeof(client_pfd));
            client_pfd.fd = client_socket;
            client_pfd.events = POLLIN;
            
            #if defined(WIN32)
                int poll_ret = WSAPoll(&client_pfd, 1, READ_WAIT_MS);
            #else
                int poll_ret = poll(&client_pfd, 1, READ_WAIT_MS);
            #endif
            
            if (poll_ret <= 0) {
                closeSocket(client_socket);
                return;
            }
            
            int bytes_received = recv(client_socket, input_buf, sizeof(input_buf) - 1, 0);
            if (bytes_received == SOCKET_ERROR || bytes_received == 0) {
                closeSocket(client_socket);
                return;
            }
            
            input_buf[bytes_received] = '\0';
            
            std::string request(input_buf);
            std::string response = handleRequest(request);
            
            send(client_socket, response.c_str(), response.length(), 0);
            closeSocket(client_socket);
        }
    }
};

#endif