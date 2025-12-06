#include <iostream>
#include "background.hpp"


void test1() {
    std::cout << "=== Test 1 ===" << "\n";
    
    #ifdef _WIN32
        std::cout << "Starting Hellow World" << "\n"; 
        int success = BackgroundLauncher::launch("../../../lab1/build/output/main.exe"); // Путь до собранной программы из 1 лабы
    #else
        std::cout << "Starting 'sh -c exit 5'" << "\n";
        int success = BackgroundLauncher::launch("sh", {"-c", "exit 5"});
    #endif
    
    if (success){
        std::cout << "Ok" << "\n\n";
    } else {
        std::cout << "Failed" << "\n\n";
    }
}

void test2() {
    std::cout << "=== Test 2 ===" << "\n";
    
    #ifdef _WIN32
        std::cout << "Starting 'cmd /c exit 5' + waining" << "\n";
        int exitCode = BackgroundLauncher::launchAndWait("cmd.exe", {"/c", "exit", "5"});
    #else
        std::cout << "Starting 'sh -c exit 5' + waining" << "\n";
        int exitCode = BackgroundLauncher::launchAndWait("sh", {"-c", "exit 5"});
    #endif
    
    std::cout << "Exit code: " << exitCode << "\n\n";
}

void test3(int n) {
    std::cout << "=== Test 3 ===" << "\n";
    
    for(int i = 0; i < n; i++){
        #ifdef _WIN32
            BackgroundLauncher::launch("cmd.exe", {"/c", "timeout", "3"});
        #else
            BackgroundLauncher::launch("sh", {"-c", "sleep 3"});
        #endif
    }
    
    std::cout << "Started " << n << " processes" << "\n";
    
    BackgroundLauncher::waitForAll();
    
    std::cout << "All proc completed" << "\n\n";
}

void test4(int n) {
    std::cout << "=== Test 4 ===" << "\n";

    for(int i = 0; i < n; i++){
        #ifdef _WIN32
            BackgroundLauncher::launch("cmd.exe", {"/c", "timeout", "1"});
        #else
            BackgroundLauncher::launch("sh", {"-c", "sleep 1"});
        #endif

        std::cout << "Proc " << i << " start" << "\n";
        BackgroundLauncher::waitForAll();
        std::cout << "Proc " << i << " end" << "\n\n";
    }
    
    std::cout << "All proc completed" << "\n";
    std::cout << "\n";
}

void test5() {
    std::cout << "=== Test 5 ===" << "\n";
    
    bool success = BackgroundLauncher::launch("suschtsch", {"arg1", "arg2"});
    
    if (success) {
        std::cout << "Ok" << "\n";
    } else {
        std::cout << "Error" << "\n";
    }
    
    int exitCode = BackgroundLauncher::launchAndWait("schtsch");
    std::cout << "Exit code: " << exitCode << "\n\n";
}


int main() {
    #ifdef _WIN32
        std::cout << "Windows" << "\n";
    #else
        std::cout << "POSIX" << "\n";
    #endif
    std::cout << "\n";
    
    test1();
    test2();
    test3(3);
    test4(3);
    test5();
    
    return 0;
}