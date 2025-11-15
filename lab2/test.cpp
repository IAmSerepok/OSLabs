#include <iostream>
#include "background.hpp"


void test1() {
    std::cout << "=== Test 1 ===" << std::endl;
    
    #ifdef _WIN32
        std::cout << "Starting Hellow World" << std::endl; 
        int success = BackgroundLauncher::launch("../../../lab1/build/output/main.exe"); // Путь до собранной программы из 1 лабы
    #else
        std::cout << "Starting 'sh -c exit 5'" << std::endl;
        int success = BackgroundLauncher::launch("sh", {"-c", "exit 5"});
    #endif
    
    if (success){
        std::cout << "Ok" << std::endl << std::endl;
    } else {
        std::cout << "Failed" << std::endl << std::endl;
    }
}

void test2() {
    std::cout << "=== Test 2 ===" << std::endl;
    
    #ifdef _WIN32
        std::cout << "Starting 'cmd /c exit 5' + waining" << std::endl;
        int exitCode = BackgroundLauncher::launchAndWait("cmd.exe", {"/c", "exit", "5"});
    #else
        std::cout << "Starting 'sh -c exit 5' + waining" << std::endl;
        int exitCode = BackgroundLauncher::launchAndWait("sh", {"-c", "exit 5"});
    #endif
    
    std::cout << "Exit code: " << exitCode << std::endl << std::endl;
}

void test3(int n) {
    std::cout << "=== Test 3 ===" << std::endl;
    
    for(int i = 0; i < n; i++){
        #ifdef _WIN32
            BackgroundLauncher::launch("cmd.exe", {"/c", "timeout", "3"});
        #else
            BackgroundLauncher::launch("sh", {"-c", "sleep 3"});
        #endif
    }
    
    std::cout << "Started " << n << " processes" << std::endl;
    
    BackgroundLauncher::waitForAll();
    
    std::cout << "All proc completed" << std::endl << std::endl;
}

void test4(int n) {
    std::cout << "=== Test 4 ===" << std::endl;

    for(int i = 0; i < n; i++){
        #ifdef _WIN32
            BackgroundLauncher::launch("cmd.exe", {"/c", "timeout", "1"});
        #else
            BackgroundLauncher::launch("sh", {"-c", "sleep 1"});
        #endif

        std::cout << "Proc " << i << " start" << std::endl;
        BackgroundLauncher::waitForAll();
        std::cout << "Proc " << i << " end" << std::endl << std::endl;
    }
    
    std::cout << "All proc completed" << std::endl;
    std::cout << std::endl;
}

void test5() {
    std::cout << "=== Test 5 ===" << std::endl;
    
    bool success = BackgroundLauncher::launch("suschtsch", {"arg1", "arg2"});
    
    if (success) {
        std::cout << "Ok" << std::endl;
    } else {
        std::cout << "Error" << std::endl;
    }
    
    int exitCode = BackgroundLauncher::launchAndWait("schtsch");
    std::cout << "Exit code: " << exitCode << std::endl << std::endl;
}


int main() {
    #ifdef _WIN32
        std::cout << "Windows" << std::endl;
    #else
        std::cout << "POSIX" << std::endl;
    #endif
    std::cout << std::endl;
    
    test1();
    test2();
    test3(3);
    test4(3);
    test5();
    
    return 0;
}