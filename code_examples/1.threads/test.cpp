#include "my_thread.hpp"
#include <iostream>
#include <string>

using namespace cplib;

// Мои сообщения для обмена с потоками
enum MyMessages
{
	MESSAGE_NEW_STRING = 0,
	MESSAGE_NEW_INT,
	MESSAGE_NEW_DOUBLE
};

// Общая память, разделяемая между всеми потоками
class SharedMemory
{
public:
	SharedMemory() :_num(0), _rnum(0.0) {}
	virtual ~SharedMemory() {}
	void SetStr(const std::string& str) {
		AutoMutex mt(_mut);
		_str = str;
	}
	void GetStr(std::string& str) {
		AutoMutex mt(_mut);
		str = _str;
	}

	void SetNum(const int& num) {
		AutoMutex mt(_mut);
		_num = num;
	}
	void GetNum(int& num) {
		AutoMutex mt(_mut);
		num = _num;
	}

	void SetRNum(const double& rnum) {
		AutoMutex mt(_mut);
		_rnum = rnum;
	}
	void GetRnum(double& rnum) {
		AutoMutex mt(_mut);
		rnum = _rnum;
	}
private:
	Mutex       _mut;
	std::string _str;
	int         _num;
	double      _rnum;
};

// Первый поток - ждем события и выводим информацию о них
class Thread1 : public Thread
{
public:
	Thread1(SharedMemory* sh_mem) :_mem(sh_mem) {}
	virtual int MainStart() {
		std::cout << "Thread 1 starting..." << std::endl;
		_log = fopen("log.txt","w");
		if (!_log)
			return 1;
		return 0;
	}
	virtual void MainQuit() {
		std::cout << "Thread 1 stopping..." << std::endl;
		fclose(_log);
	}
	virtual void Main() {
		std::cout << "Thread 1 started!" << std::endl;
		while(true) {
			Event evt = Wait(3.0);
			if (evt.Type() == MESSAGE_NEW_STRING) {
				std::string nstr;
				_mem->GetStr(nstr);
				fprintf(_log,"Got new string: %s\r\n",nstr.c_str());
				fflush(_log);
			}
			else if (evt.Type() == MESSAGE_NEW_INT) {
				int nt;
				_mem->GetNum(nt);
				fprintf(_log,"Got new int: %d\r\n",nt);
				fflush(_log);
			}
			else if (evt.Type() == MESSAGE_NEW_DOUBLE) {
				double rt;
				_mem->GetRnum(rt);
				fprintf(_log,"Got new int: %f\r\n",rt);
				fflush(_log);
			}
		}
	}
private:
	SharedMemory* _mem;
	FILE* _log;
};

// Второй поток - меняем значение в общей памяти раз в секунду
class Thread2 : public Thread
{
public:
	Thread2(SharedMemory* sh_mem, Thread1* thr) :_mem(sh_mem), _thr(thr) {}
	virtual void Main() {
		std::cout << "Thread 2 started!" << std::endl;
		for (_iter = 0;; _iter++) {
			this->Sleep(1.0);
			_mem->SetNum(_iter);
			_thr->Notify(Event(MESSAGE_NEW_INT));
			CancelPoint();
		}
	}
private:
	SharedMemory* _mem;
	Thread1* _thr;
	int           _iter;
};

int main (int argc, char** argv) 
{
	SharedMemory mem;
	Thread1 th1(&mem);
	Thread2 th2(&mem,&th1);
	
	th1.Start();
	th2.Start();
	th1.WaitStartup();
	th2.WaitStartup();
	
	std::string cmd;
	std::string sparam;
	double      dparam;
	for (;;) {
		std::cout << "Enter command: ";
		std::cin >> cmd;
		if (cmd == "s" || cmd == "string") {
			std::cin >> sparam;
			mem.SetStr(sparam);
			th1.Notify(Event(MESSAGE_NEW_STRING));
		} else if (cmd == "d" || cmd == "double") {
			std::cin >> dparam;
			mem.SetRNum(dparam);
			th1.Notify(Event(MESSAGE_NEW_DOUBLE));
		} else if (cmd == "e" || cmd == "q" || cmd == "exit" || cmd == "quite") {
			break;
		} else
			std::cout << "! Possible commands: s | d | q" << std::endl;
	}
	th1.Stop();
	th2.Stop();
	th1.Join();
	th2.Join();
	std::cout << "Buy, buy!" << std::endl;
}