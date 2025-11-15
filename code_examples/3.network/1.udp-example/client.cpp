#include <stdlib.h>         /* atoi */
#include <iostream>         /* std::cout */
#include <string.h>         /* memset */
#if defined (WIN32)
#   include <winsock2.h>    /* socket */
#else
#   include <sys/socket.h>  /* socket */
#   include <netinet/in.h>  /* socket */
#   include <arpa/inet.h>   /* socket */
#   include <fcntl.h>       /* fcntl */
#   include <unistd.h>      
#   define SOCKET int
#   define INVALID_SOCKET -1
#   define SOCKET_ERROR -1
#endif

SOCKET __msocket = INVALID_SOCKET;
void close_socket();

int main (int argc, char** argv)
{
    if (argc < 3) {
        std::cout << "Usage: client ip port" << std::endl;
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
    // Делаем его неблокируемым
#if defined (WIN32)
    unsigned long yes = 1;
    if (ioctlsocket(__msocket, FIONBIO, &yes) == SOCKET_ERROR) {
        std::cout << "Failed to set socket non-blocking!" << std::endl;
        close_socket();
        return -1;
    }
#else
    int flags;
    // Получаем флаги доступа
    if((flags = fcntl(__msocket, F_GETFL)) == SOCKET_ERROR) {
        std::cout << "Failed to get socket flags!" << std::endl;
        close_socket();
        return -1;
    }
    // Добавляем к флагам O_NONBLOCK и записываем их
    if(fcntl(__msocket, F_SETFL, flags | O_NONBLOCK ) == SOCKET_ERROR) {
        std::cout << "Failed to set socket flags!" << std::endl;
        close_socket();
        return -1;
    }
#endif
    // Биндим сокет на адрес и порт по-умолчанию
    sockaddr_in local_addr;
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = htons(INADDR_ANY);
    local_addr.sin_port = 0;
    if (bind(__msocket,(struct sockaddr*)&local_addr, sizeof(local_addr))) {
        std::cout << "Failed to bind!" << std::endl;
        return -1;
    }
    // Заполняем данные для подключения
    struct sockaddr_in remote_addr;
    remote_addr.sin_family = AF_INET;
    remote_addr.sin_addr.s_addr = ip_addr;
    remote_addr.sin_port = port;
    // Заполняем буферы для получения данных
    char rec_buf[255];
    rec_buf[254] = '\0';
    char out_buf[100];
    // Отправляем данные серверу
    for (int i = 0;i < 10;i++) {
        sprintf(out_buf,"Message step: %d",i);
        std::cout << "Sending '" << out_buf << "' message..." << std::endl;
        int sended = sendto(__msocket, out_buf, strlen(out_buf)+1,0,(sockaddr*)&remote_addr,sizeof(remote_addr));
        if (sended == 0 || sended == SOCKET_ERROR)
            std::cout << "Error while send!" << std::endl;
    }
    // Получаем данные от сервера
    for (;;) {
        int readd = recv(__msocket,rec_buf,sizeof(rec_buf)-1,0);
        if(readd == SOCKET_ERROR || !readd)
            break;
        else
            std::cout << "Received '" << rec_buf << "' message" << std::endl;
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