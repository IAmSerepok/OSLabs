#include <stdlib.h>         /* atoi */
#include <iostream>         /* std::cout */
#include <string.h>         /* memset */
#if defined (WIN32)
#   include <winsock2.h>    /* socket */
#else
#   include <sys/socket.h>  /* socket */
#   include <netinet/in.h>  /* socket */
#   include <arpa/inet.h>   /* socket */
#   include <unistd.h>      
#   define SOCKET int
#   define INVALID_SOCKET -1
#   define SOCKET_ERROR -1
#endif

SOCKET __msocket;
void close_socket();

int main (int argc, char** argv)
{
    if (argc < 3) {
        std::cout << "Usage: server ip port" << std::endl;
        return -1;
    }
    int ip_addr = inet_addr(argv[1]);
    int port = htons(atoi(argv[2]));

    // Инициализируем библиотеку сокетов (под Windows)
#if defined (WIN32)
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2,2), &wsaData);
#endif
    // Создаем сокет
    __msocket = socket(AF_INET,SOCK_DGRAM,0);
    if (__msocket == INVALID_SOCKET) {
        std::cout << "Cant open socket!" << std::endl;
        return -1;
    }
    // Биндим сокет на адрес и порт
    sockaddr_in local_addr;
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = ip_addr;
    local_addr.sin_port = port;
    if (bind(__msocket,(struct sockaddr*)&local_addr, sizeof(local_addr))) {
        std::cout << "Failed to bind!" << std::endl;
        close_socket();
        return -1;
    }
    // Создаем буферы для входных данных
    sockaddr_in remote_addr;
#if defined (WIN32)    
    int addrlen = sizeof(remote_addr);
#else
    unsigned int addrlen = sizeof(remote_addr);
#endif
    memset(&remote_addr, 0, sizeof(remote_addr));
    char rec_buf[255];
    rec_buf[254] = '\0';
    for (;;) {
        // Получаем данные от клиента
        int readd = recvfrom(__msocket,rec_buf,sizeof(rec_buf)-1,0,(struct sockaddr *)&remote_addr,&addrlen);
        if(readd == SOCKET_ERROR)
            std::cout << "Failed to receive!" << std::endl;
        // Получили команду на завершение работы сервера
        if (strlen(rec_buf) == 4 && !strncmp(rec_buf,"exit",4)) {
            std::cout << "Command to exit!" << std::endl;
            break;
        }
        // Отвечаем клиенту
        if (readd > 0) {
            int sended = sendto(__msocket,rec_buf,readd,0,(sockaddr*)&remote_addr,addrlen);
            if (sended == 0)
                std::cout << "Failed to send: no one listens!" << std::endl;
            if (sended == SOCKET_ERROR)
                std::cout << "Error while send!" << std::endl;
        }
    }
    close_socket();
}

void close_socket()
{
#if defined (WIN32)
    shutdown(__msocket,SD_BOTH);
    closesocket(__msocket);
    WSACleanup();
#else
    shutdown(__msocket,SHUT_RDWR);
    close(__msocket);
#endif
}
