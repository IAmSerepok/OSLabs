#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <memory>
#include <iomanip>

#ifdef _WIN32
#   include <windows.h>
#   include <tchar.h>
#   include <processthreadsapi.h>
#else
#   include <sys/types.h>
#   include <unistd.h>
#   include <sys/wait.h>
#   include <errno.h>
#   include <signal.h>
#endif

#include "shmem.hpp" // Используем библиотеку из code_examples


// Структура для общих данных
struct SharedData {
    int counter = 0;
    bool hasMaster = false;
#ifdef _WIN32
    DWORD masterPid = 0;
#else
    pid_t masterPid = 0;
#endif
};

class ProcessManager {
private:
    std::atomic<bool> running;
    std::atomic<bool> isMaster;
    std::thread timerThread;
    std::thread oneSecondThread;
    std::thread threeSecondsThread;
    std::string logfile;
    cplib::SharedMem<SharedData> sharedMemory;
    
    #ifdef _WIN32
        DWORD pid;
        std::vector<HANDLE> childProcesses;
    #else
        pid_t pid;
        std::vector<pid_t> childProcesses;
    #endif

public:
    ProcessManager() : 
        running(true), 
        isMaster(false),
        logfile("program.log"),
        sharedMemory("global_counter", true) {
        
        #ifdef _WIN32
            pid = GetCurrentProcessId();
        #else
            pid = getpid();
        #endif
        
        // Проверяем и устанавливаем статус мастера
        checkMasterStatus();
        
        // Инициализируем счетчик если мы мастер
        if (isMaster && sharedMemory.IsValid()) {
            sharedMemory.Lock();
            SharedData* data = sharedMemory.Data();
            data->counter = 0;
            sharedMemory.Unlock();
        }
    }

    ~ProcessManager() {
        running = false;
        if (timerThread.joinable()) timerThread.join();
        if (oneSecondThread.joinable()) oneSecondThread.join();
        if (threeSecondsThread.joinable()) threeSecondsThread.join();
        
        // Мастер складывает полномочия
        if (isMaster && sharedMemory.IsValid()) {
            sharedMemory.Lock();
            SharedData* data = sharedMemory.Data();
            if (data->masterPid == pid) {
                data->hasMaster = false;
                data->masterPid = 0;
            }
            sharedMemory.Unlock();
        }
        
        cleanupChildProcesses();
    }

    // Получаем строку с временем
    std::string getCurrentTime() {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
        
        char timeBuffer[100];
        std::tm* timeinfo = std::localtime(&time_t);
        std::strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S", timeinfo);
        
        // Добавляем мс
        char result[110];
        std::snprintf(result, sizeof(result), "%s.%03d", timeBuffer, static_cast<int>(ms.count()));
        return std::string(result);
    }

    // Запись сообщения в лог файл
    void writeToLog(const std::string& message) {
        static cplib::SharedMem<int> mutex("mutex", true);
        
        if (mutex.IsValid()) {
            mutex.Lock();
            std::ofstream file(logfile, std::ios::app);
            if (file.is_open()) {
                file << getCurrentTime() << "; PID: " << pid << "; message: " << message << std::endl;
            }
            file.close();
            mutex.Unlock();
        } 
    }

    void checkMasterStatus() {
        if (sharedMemory.IsValid()) {
            sharedMemory.Lock();
            SharedData* data = sharedMemory.Data();
            
            if (!data->hasMaster) { // Если мастера нет - перехватываем полномочия
                data->hasMaster = true;
                data->masterPid = pid;
                isMaster = true;
            } 
            else isMaster = (data->masterPid == pid); // Записываем id мастера
            
            sharedMemory.Unlock();
        }
    }

#ifdef _WIN32
    void createChildProcess(const std::string& command) {
        STARTUPINFOA si; // 3
        PROCESS_INFORMATION pi; // o
        
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        ZeroMemory(&pi, sizeof(pi));
        
        char exePath[MAX_PATH];
        GetModuleFileNameA(NULL, exePath, MAX_PATH); // Берем путь к текущей программе
        
        std::string cmd = std::string("\"") + exePath + "\" " + command;
        
        // Запускаем нашу копию с нужными флагами
        if (CreateProcessA(NULL, &cmd[0], NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) { 
            childProcesses.push_back(pi.hProcess);
            CloseHandle(pi.hThread);
        }
    }
    
    void cleanupChildProcesses() {
        for (auto hProcess : childProcesses) {
            DWORD exitCode;
            if (GetExitCodeProcess(hProcess, &exitCode) && exitCode == STILL_ACTIVE) {
                TerminateProcess(hProcess, 0);
            }
            CloseHandle(hProcess);
        }
        childProcesses.clear();
    }
    
    bool isChildProcessRunning(HANDLE hProcess) {
        DWORD exitCode;
        if (GetExitCodeProcess(hProcess, &exitCode)) {
            return exitCode == STILL_ACTIVE;
        }
        return false;
    }
#else
    bool createChildProcess(const std::string& command) {
        pid_t childPid = fork();
        
        if (childPid == 0) {
            // Дочерний процесс
            char* argv[] = {(char*)"program", (char*)command.c_str(), nullptr};
            execv("/proc/self/exe", argv);
            execv("./program", argv);
            _exit(1); // Если exec не удался
        } else if (childPid > 0) {
            // Родительский процесс
            childProcesses.push_back(childPid);
            return true;
        } 
        return false; 
    }
    
    void cleanupChildProcesses() {
        for (auto childPid : childProcesses) {
            kill(childPid, SIGTERM);
            int status;
            waitpid(childPid, &status, 0);
        }
        childProcesses.clear();
    }
    
    bool isChildProcessRunning(pid_t childPid) {
        return kill(childPid, 0) == 0;
    }
#endif

    // Увеличиваем счетчика раз в 300 мс
    void startTimer() {
        timerThread = std::thread([this]() {
            while (running) {
                std::this_thread::sleep_for(std::chrono::milliseconds(300));
                if (sharedMemory.IsValid()) {
                    sharedMemory.Lock();
                    SharedData* data = sharedMemory.Data();
                    data->counter++;
                    sharedMemory.Unlock();
                }
            }
        });
    }

    // Раз в секунду логируем значение счетчика
    void startOneSecondLogger() {
        oneSecondThread = std::thread([this]() {
            while (running) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                if (sharedMemory.IsValid() && isMaster) {
                    sharedMemory.Lock();
                    SharedData* data = sharedMemory.Data();
                    writeToLog("Counter: " + std::to_string(data->counter));
                    sharedMemory.Unlock();
                }
            }
        });
    }

    // Создание дочерних процессов раз в 3 секунды
    void startThreeSecondsProcessCreator() {
        threeSecondsThread = std::thread([this]() {
            while (running) {
                std::this_thread::sleep_for(std::chrono::seconds(3));
                
                if (!isMaster) continue;
                
                // Очищаем завершенные процессы
                for (auto it = childProcesses.begin(); it != childProcesses.end();) {
                    if (!isChildProcessRunning(*it)) {
                        it = childProcesses.erase(it);
                    } else {
                        ++it;
                    }
                }
                
                // Проверяем есть ли незавершенные копии
                if (!childProcesses.empty()) {
                    continue;
                }
                
                // Запускаем копии
                createChildProcess("cp1"); 
                createChildProcess("cp2"); 
            }
        });
    }

    // Запуск копии на +10
    void runCopy1() {
        writeToLog("Copy +10 started");
        if (sharedMemory.IsValid()) {
            sharedMemory.Lock();
            SharedData* data = sharedMemory.Data();
            data->counter += 10;
            sharedMemory.Unlock();
        }
        writeToLog("Copy +10 finished");
    }

    // Запуск копии на х2
    void runCopy2() {
        writeToLog("COPY x2 finished");
        if (sharedMemory.IsValid()) {
            sharedMemory.Lock();
            SharedData* data = sharedMemory.Data();
            int old_value = data->counter;
            data->counter = old_value * 2;
            sharedMemory.Unlock();
            
            std::this_thread::sleep_for(std::chrono::seconds(2));
            
            sharedMemory.Lock();
            data = sharedMemory.Data();
            int current = data->counter;
            data->counter = current / 2;
            sharedMemory.Unlock();
        }
        writeToLog("Copy x2 finished");
    }

    // Получаем значение счетчика
    int getCurrentCounter() {
        if (sharedMemory.IsValid()) {
            sharedMemory.Lock();
            SharedData* data = sharedMemory.Data();
            int value = data->counter;
            sharedMemory.Unlock();
            return value;
        }
        return -1;
    }

    // Устанавливаем значение счетчика
    void setCounter(int value) {
        if (sharedMemory.IsValid()) {
            sharedMemory.Lock();
            SharedData* data = sharedMemory.Data();
            data->counter = value;
            sharedMemory.Unlock();
        }
    }

    // Обработка пользовательских команд
    void handleUserInput() {
        std::string input;
        
        while (running) {
            if (!std::getline(std::cin, input)) {
                break;
            }
            
            if (input == "g") {
                std::cout << "Current counter: " << getCurrentCounter() << std::endl;
            }
            else if (input.find("s ") == 0) {
                std::string value_str = input.substr(2);
                int value = std::stoi(value_str);
                setCounter(value);
            }
            else if (!input.empty()) {
                std::cout << "Unknown command" << std::endl;
            }
        }
    }

    void run(int argc, char* argv[]) {
        // Обработка запуска нужной копии
        if (argc > 1) {
            std::string mode = argv[1];
            if (mode == "cp1") {
                runCopy1();
                return;
            } else if (mode == "cp2") {
                runCopy2();
                return;
            }
        }
        
        // Основной процесс
        writeToLog("Process started" + std::string(isMaster ? " as MASTER" : " as SLAVE"));
        writeToLog("Initial counter: " + std::to_string(getCurrentCounter()));
        
        // Запускаем потоки
        startTimer();
        startOneSecondLogger();
        if (isMaster) {
            startThreeSecondsProcessCreator();
        }
        
        // Обрабатываем пользовательский ввод
        handleUserInput();
    }
};

int main(int argc, char* argv[]) {
    ProcessManager manager;
    manager.run(argc, argv);
    return 0;
}