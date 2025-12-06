#include <string>
#include <vector>
#include <iostream>

#ifdef _WIN32
#   include <windows.h>
#else
#   include <unistd.h>
#   include <sys/wait.h>
#   include <sys/types.h>
#   include <cstring>
#   include <errno.h>
#   include <fcntl.h>
#endif


class BackgroundLauncher {
private:  // Для хранения айдишек процессов
    #ifdef _WIN32
        static std::vector<HANDLE> processes;
    #else
        static std::vector<pid_t> processes;
    #endif

    static std::string buildCommandLine(const std::string& program, const std::vector<std::string>& args) {
        std::string commandLine;
        
        #ifdef _WIN32
        // Обрабатываем пробелы в путях на винде
        if (program.find(' ') != std::string::npos) {
            commandLine = "\"" + program + "\"";
        } else {
            commandLine = program;
        }
        #else
            commandLine = program;
        #endif
        
        for (const auto& arg : args) {
            commandLine += " ";
            #ifdef _WIN32
                if (arg.find(' ') != std::string::npos) {
                    commandLine += "\"" + arg + "\"";
                } else {
                    commandLine += arg;
                }
            #else
                commandLine += arg;
            #endif
        }
        
        return commandLine;
    }

    #ifdef _WIN32
        static HANDLE launchWindows(const std::string& program, const std::vector<std::string>& args) {
            std::string commandLine = buildCommandLine(program, args);
            
            // Структурки из winAPI для хранения инфы
            STARTUPINFOA startupInfo; // Инфа об окне, дескрипторы стандартных потоков ввода и вывода
            PROCESS_INFORMATION processInfo; // Интедификаторы процесса и потока
            
            ZeroMemory(&startupInfo, sizeof(startupInfo)); // Для инициализации нулями
            startupInfo.cb = sizeof(startupInfo);
            ZeroMemory(&processInfo, sizeof(processInfo)); // Для инициализации нулями
            
            if (CreateProcessA(
                NULL, // Используем консоль
                const_cast<LPSTR>(commandLine.c_str()),
                NULL,
                NULL, 
                FALSE, 
                CREATE_NO_WINDOW, // Запускаем без окна (в фоне)
                NULL, 
                NULL, 
                &startupInfo, // STARTUPINFO
                &processInfo  // PROCESS_INFORMATION
            )) {
                CloseHandle(processInfo.hThread); 
                return processInfo.hProcess; // Возвращаем дескриптор процесса
            }
            
            return NULL;
        }
    #else
        static pid_t launchUnix(const std::string& program, const std::vector<std::string>& args) {
            pid_t pid = fork(); // pid=0: мы в дочернем, pid>0: мы в родителе, pid<0: ошибка

            int pipefd[2];
            if (pipe(pipefd) == -1) {
                return -1;
            }
            
            if (pid == 0) {
                // Дочерний процесс
                close(pipefd[0]);

                std::vector<char*> argv;
                argv.push_back(const_cast<char*>(program.c_str()));
                
                for (const auto& arg : args) {
                    argv.push_back(const_cast<char*>(arg.c_str()));
                }
                argv.push_back(nullptr);
                
                execvp(program.c_str(), argv.data()); // Меняем текущий дочерний процесс на нужную нам программу

                exit(EXIT_FAILURE);

            } else if (pid > 0) {
                return pid;
            } else {
                return -1;
            }
        }
    #endif

public:
    // Запуск программы в фоновом режиме
    static bool launch(const std::string& program, const std::vector<std::string>& args = {}) {
        #ifdef _WIN32
            HANDLE process = launchWindows(program, args);
            if (process != NULL) {
                processes.push_back(process);
                return true;
            }
        #else
            pid_t pid = launchUnix(program, args);
            if (pid > 0) {
                processes.push_back(pid);
                return true;
            }
        #endif
        return false;
    }
    
    // Запуск программы и ожидание её завершения
    static int launchAndWait(const std::string& program, const std::vector<std::string>& args = {}) {
        #ifdef _WIN32
            HANDLE process = launchWindows(program, args);
            if (process == NULL) {
                return -1;
            }
            
            // Ожидаем завершения и получаем код возврата
            DWORD exitCode;
            WaitForSingleObject(process, INFINITE);
            GetExitCodeProcess(process, &exitCode);
            CloseHandle(process);
            
            return static_cast<int>(exitCode);
        #else
            pid_t pid = launchUnix(program, args);
            if (pid < 0) {
                return -1;
            }
            
            // Ожидаем завершения и получаем код возврата
            int status;
            waitpid(pid, &status, 0);
            
            if (WIFEXITED(status)) {
                return WEXITSTATUS(status);
            } else {
                return -1;
            }
        #endif
    }
    
    // Ожидание завершения всех запущенных процессов
    static void waitForAll() {
        #ifdef _WIN32
            if (!processes.empty()) {
                WaitForMultipleObjects(static_cast<DWORD>(processes.size()), processes.data(), TRUE, INFINITE);
                for (HANDLE process : processes) {
                    CloseHandle(process);
                }
                processes.clear();
            }
        #else
            for (pid_t pid : processes) {
                int status;
                waitpid(pid, &status, 0);
            }
            processes.clear();
        #endif
    }
};

#ifdef _WIN32
    std::vector<HANDLE> BackgroundLauncher::processes;
#else
    std::vector<pid_t> BackgroundLauncher::processes;
#endif
