#include <stdlib.h>         /* atoi */
#include <iostream>         /* std::cout */
#include <string.h>         /* memset */
#include <string>           /* std::getline() */
#if defined (WIN32)
#   include <winsock2.h>    /* socket */
#else
#   include <sys/socket.h>  /* socket */
#   include <netinet/in.h>  /* socket */
#   include <arpa/inet.h>   /* socket */
#   include <poll.h>        /* poll() */
#   include <errno.h>       /* errno() */
#   include <signal.h>      /* for signal() call for SIGPIPE ignore  */
#   include <unistd.h>      
#   define SOCKET int
#   define INVALID_SOCKET -1
#   define SOCKET_ERROR -1
#endif

#define READ_WAIT_MS 200

class Client
{
public:
    Client():m_socket(INVALID_SOCKET){}
    ~Client(){if (m_socket != INVALID_SOCKET) Disconnect();}
    bool Connect(const std::string& server_ip, short int server_port)
    {
        if (m_socket != INVALID_SOCKET) {
            std::cout << "Already connected!" << std::endl;
            return false;
        }
        // Создаем сокет
        m_socket = socket(AF_INET,SOCK_STREAM,0);
        if (m_socket == INVALID_SOCKET) {
            std::cout << "Cant open socket: " << ErrorCode() << std::endl;
            return false;
        }
        // Биндим сокет на адрес и порт по-умолчанию
        sockaddr_in local_addr;
        memset(&local_addr, 0, sizeof(local_addr));
        local_addr.sin_family = AF_INET;
        local_addr.sin_addr.s_addr = htons(INADDR_ANY);
        local_addr.sin_port = 0;
        if (bind(m_socket,(struct sockaddr*)&local_addr, sizeof(local_addr))) {
            std::cout << "Failed to bind: " << ErrorCode() << std::endl;
            CloseSocket();
            return false;
        }
        // Заполняем данные для подключения
        struct sockaddr_in remote_addr;
        remote_addr.sin_family = AF_INET;
        remote_addr.sin_addr.s_addr = inet_addr(server_ip.c_str());
        remote_addr.sin_port = htons(server_port);
        // Коннектим сокет
        if (connect(m_socket,(struct sockaddr*)&remote_addr,sizeof(remote_addr))) {
            std::cout << "Failed to connect: " << ErrorCode() << std::endl;
            CloseSocket();
            return false;
        }
        return true;
    }
    bool Disconnect()
    {
        if (m_socket == INVALID_SOCKET) {
            std::cout << "Not connected!" << std::endl;
            return false;
        }
        CloseSocket();
        return true;
    }
    int Write(const char* data, int data_size)
    {
        if (!data_size)
            return 0;
        int sended = send(m_socket, data, data_size,0);
        if (!sended || sended == SOCKET_ERROR) {
            std::cout << "Error while send: " << ErrorCode() << std::endl;
            return -1;
        }
        return sended;
    }
    int Read(char* data, size_t max_data_read)
    {
        if (!max_data_read)
            return 0;
        // Вариант работы с блокируемым сокетом
        struct pollfd polstr;
        memset(&polstr,0,sizeof(struct pollfd));
        polstr.fd = m_socket;
        polstr.events |= POLLIN;
        int ret = 0;
#ifdef WIN32
        ret = WSAPoll(&polstr,1,READ_WAIT_MS);
#else
        ret = poll(&polstr,1,READ_WAIT_MS);
#endif
        // Ошибка сокета
        if (ret == SOCKET_ERROR) {
            std::cout << "Error on poll: " << ErrorCode() << std::endl;
            return 0;
        }
        // Сработал таймер
        if (!ret)
            return 0;
        // В буфере есть данные для чтения или ошибка на сокете
        int readd = recv(m_socket,data,max_data_read,0);
        if(!readd || readd == SOCKET_ERROR) {
            std::cout << "Nothing read!" << std::endl;
            return -1;
        }
        return readd;
    }
protected:
    void CloseSocket()
    {
#if defined (WIN32)
        shutdown(m_socket,SD_BOTH);
        closesocket(m_socket);
#else
        shutdown(m_socket,SHUT_RDWR);
        close(m_socket);
#endif
        m_socket = INVALID_SOCKET;
    }
    int ErrorCode()
    {
#if defined (WIN32)
        return WSAGetLastError();
#else
        return errno;
#endif
    }
private:
    SOCKET m_socket;
};


int main (int argc, char** argv)
{
    if (argc < 3) {
        std::cout << "Usage: client ip port" << std::endl;
        return -1;
    }
    // Инициализируем библиотеку сокетов (под Windows)
#if defined (WIN32)
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2,2), &wsaData);
#else
    // Игнорируем SIGPIPE сигнал
    // Чтобы программа не терминировалась при поптыке записи в закрытый сокет
    signal(SIGPIPE, SIG_IGN);
#endif
    Client cli;
    if (!cli.Connect(argv[1],atoi(argv[2]))) {
        std::cout << "Terminating..." << std::endl;
#if defined (WIN32)
        WSACleanup();
#endif
        return -1;
    }
    std::string user_string;
    char server_buf[255];
    memset(server_buf,0,sizeof(server_buf));
    if (cli.Read(server_buf,sizeof(server_buf)-1) > 0)
        std::cout << "Server welcome message: " << server_buf << std::endl;
    for (;;) {
        std::cout << "Your request: ";
        std::getline(std::cin,user_string);
        if (user_string.empty())
            continue;
        if (user_string == "exit" || user_string == "quit")
            break;
        cli.Write(user_string.c_str(),user_string.size()+1);
        int rd = cli.Read(server_buf,sizeof(server_buf)-1);
        if (rd > 0)
            std::cout << "Server reply: " << server_buf << std::endl;
        else
            std::cout << "No server reply!" << std::endl;
    }
    // Деинициализируем библиотеку сокетов (под Windows)
#if defined (WIN32)
    WSACleanup();
#endif
    return 0;
}