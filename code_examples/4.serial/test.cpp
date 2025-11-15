#include "my_serial.hpp"
#include <sstream>              // std::stringstream
#include <iostream>             // std::cout

#if !defined (WIN32)
#	include <unistd.h>          // pause()
#	include <time.h>            // nanosleep()
#endif
// Сконвертировать любой базовый тип в строку
template<class T> std::string to_string(const T& v)
{
    std::ostringstream ss;
    ss << v;
    return ss.str();
}

void csleep(double timeout) {
#if defined (WIN32)
	if (timeout <= 0.0)
        ::Sleep(INFINITE);
    else
        ::Sleep((DWORD)(timeout * 1e3));
#else
    if (timeout <= 0.0)
        pause();
    else {
        struct timespec t;
        t.tv_sec = (int)timeout;
        t.tv_nsec = (int)((timeout - t.tv_sec)*1e9);
        nanosleep(&t, NULL);
    }
#endif
}

int main(int argc, char** argv)
{
	if (argc < 3) {
		std::cout << "Usage: sertest [port] [regime], where [regime] can be 'read' or 'write'" << std::endl;
		return -1;
	}
	bool read = true;
	if (!strcmp(argv[2],"write"))
		read = false;
	
	cplib::SerialPort smport(std::string(argv[1]),cplib::SerialPort::BAUDRATE_115200);
	if (!smport.IsOpen()) {
		std::cout << "Failed to open port '" << argv[1] << "'! Terminating..." << std::endl;
		return -2;
	}
	std::string mystr;
	if (read) {
		smport.SetTimeout(1.0);
		for (;;) {
			smport >> mystr;
			std::cout << "Got: " << (mystr.empty()?"nothing": mystr) << std::endl; 
		}
	} else {
		for (int i = 0;;i++) {
			mystr = std::string("Iteration ") + to_string(i);
			smport << mystr;
			csleep(1.0);
		}
	}
    return 0;
}