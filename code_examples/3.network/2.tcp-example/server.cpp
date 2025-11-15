#include <stdlib.h>         /* atoi */
#include <iostream>         /* std::cout */
#include <string.h>         /* memset */
#include <map>              /* std::map */    
#if defined (WIN32)
#   include <winsock2.h>    /* socket */
#else
#   include <sys/socket.h>  /* socket */
#   include <netinet/in.h>  /* socket */
#   include <arpa/inet.h>   /* socket */
#   include <poll.h>        /* poll() */
#   include <errno.h>       /* errno() */
#   include <unistd.h>
#   include <signal.h>      /* for signal() call for SIGPIPE ignore  */
#   define SOCKET int
#   define INVALID_SOCKET -1
#   define SOCKET_ERROR -1
#endif
#ifndef MIN
#   define MIN(a,b) ((a < b) ? (a) : (b))
#endif

#define POLL_ARRAY_GROW_STEP 20
#define BUFFER_SIZE 4096

int GetSocketErorCode()
{
#if defined (WIN32)
    return WSAGetLastError();
#else
    return errno;
#endif
}

// Класс для работы с клиентом
class Client
{
public:
    Client(struct pollfd* pollstr):m_pollstr(pollstr){ClearBuffers();}
    bool Read()
    {
        // Прочтем данные
        int readd = recv(m_pollstr->fd,m_input_data,sizeof(m_input_data)-m_input_data_size,0);
        if(readd == SOCKET_ERROR) {
            std::cout << "Failed to receive data for client: " << GetSocketErorCode() << std::endl;
            return false;
        }
        m_input_data_size+=readd;
        // Парсим данные и формируем ответ. В этом примере - просто переносим все, что получили, в выходной буфер
        int wrote = WriteData(m_input_data,m_input_data_size);
        if (wrote == m_input_data_size)
            m_input_data_size = 0;
        else {
            memmove(m_input_data,m_input_data+wrote,m_input_data_size-wrote);
            m_input_data_size-=wrote;
        }
        return true;
    }
    bool Write()
    {
        if (!m_output_data_size)
            return true;
        int sended = send(m_pollstr->fd,m_output_data,m_output_data_size,0);
        if (sended == 0 || sended == SOCKET_ERROR) {
            std::cout << "Failed to send data to client: " << GetSocketErorCode() << std::endl;
            return false;
        }
        m_output_data_size-=sended;
        if (!m_output_data_size)
            m_pollstr->events&= ~POLLOUT;
        return true;
    }
    size_t WriteData(const char* str)
    {
        return WriteData(str,strlen(str)+1);
    }
    size_t WriteData(const char* data, size_t data_size)
    {
        size_t tocopy = MIN(data_size,BUFFER_SIZE-m_output_data_size);
        memcpy(m_output_data+m_output_data_size, data,tocopy);
        m_output_data_size+=tocopy;
        m_pollstr->events|=POLLOUT;
        // Сразу ответим клиенту
        Write();
        return tocopy;
    }
    void ClearBuffers()
    {
        memset(m_input_data,0,sizeof(m_input_data));
        m_input_data_size = 0;
        memset(m_output_data,0,sizeof(m_output_data));
        m_output_data_size = 0;
    }
private:
    struct pollfd* m_pollstr;
    char    m_input_data[BUFFER_SIZE];
    size_t  m_input_data_size;
    char    m_output_data[BUFFER_SIZE];
    size_t  m_output_data_size;
};

// Сервер
class Server
{
public:
    typedef std::map<SOCKET,Client*>   ClientMap;
    Server():m_srv_sock(INVALID_SOCKET),m_polls(NULL),m_polls_size(0),m_polls_cap(0)
    {
    }
    ~Server(){Stop();}
    bool Start(const std::string& ip_addr, short int port)
    {
        if (m_srv_sock != INVALID_SOCKET) {
            std::cout << "Server already started: " << GetSocketErorCode() << std::endl;
            return false;
        }
        // Создаем сокет
        m_srv_sock = socket(AF_INET,SOCK_STREAM,0);
        if (m_srv_sock == INVALID_SOCKET) {
            std::cout << "Cant open socket: " << GetSocketErorCode() << std::endl;
            return false;
        }
        // Биндим сокет на адрес и порт
        sockaddr_in local_addr;
        memset(&local_addr, 0, sizeof(local_addr));
        local_addr.sin_family = AF_INET;
        local_addr.sin_addr.s_addr = inet_addr(ip_addr.c_str());
        local_addr.sin_port = htons(port);
        if (bind(m_srv_sock,(struct sockaddr*)&local_addr, sizeof(local_addr))) {
            std::cout << "Failed to bind: " << GetSocketErorCode() << std::endl;
            CloseSocket(m_srv_sock);
            m_srv_sock = INVALID_SOCKET;
            return false;
        }
        // Запускаем прослушку
        if(listen(m_srv_sock, SOMAXCONN)) {
            std::cout << "Failed to start listen: " << GetSocketErorCode() << std::endl;
            CloseSocket(m_srv_sock);
            m_srv_sock = INVALID_SOCKET;
            return false;
        }
        // Помещаем сокет в Poll
        AddToPoll(m_srv_sock);
        return true;
    }
    bool Iteration()
    {
        int ret = 0;
#ifdef WIN32
        ret = WSAPoll(m_polls,m_polls_size,-1);
#else
        ret = poll(m_polls,m_polls_size,-1);
#endif
        // Если вернулась ошибка - выходим
        if (ret == SOCKET_ERROR) {
            std::cout << "Error on poll: " << GetSocketErorCode() << std::endl;
            return false;
        }
        // Проверяем, какие события получены
        for (size_t i = 0; i < m_polls_size; ++i) {
            if (!m_polls[i].revents)
                continue;
            if (m_polls[i].fd == m_srv_sock) {
                // Главный сокет - события на нем означают соединения клиентов
                if (m_polls[i].revents & POLLIN) {
                    SOCKET accsocket;
                    accsocket = accept(m_srv_sock, NULL, NULL);
                    if (accsocket == INVALID_SOCKET) {
                        std::cout << "Error connecting new client: " << GetSocketErorCode() << std::endl;
                        continue;
                    }
                    Client* cli = AddClient(accsocket);
                    cli->WriteData("Welcome to THE server!");
                    std::cout << "New client connected!" << std::endl;
                }
            } else {
                // Остальные сокеты - обслуживаем их в соответствии с логикой сервера
                ClientMap::iterator it = m_clients.find(m_polls[i].fd);
                if (it == m_clients.end())
                    continue;
                Client* cli = it->second;
                // Можно читать
                if (m_polls[i].revents & POLLIN) {
                    cli->Read();
                }
                // Можно писать
                if (m_polls[i].revents & POLLOUT) {
                    cli->Write();
                }
                // Произошла ошибка или клиент прервал соединение
                if (m_polls[i].revents & POLLERR || m_polls[i].revents & POLLHUP) {
                    std::cout << "Client disconnected!" << std::endl;
                    RemoveClient(it->first);
                    i--;
                    continue;
                }
            }
        }
        return true;
    }
    bool Stop()
    {
        if (m_srv_sock == INVALID_SOCKET) {
            std::cout << "Server already stopped!" << std::endl;
            return false;
        }
        // Закроем сокет сервера
        CloseSocket(m_srv_sock);
        m_srv_sock = INVALID_SOCKET;
        // Закроем сокеты всех клиентов и освободим память
        for (ClientMap::iterator it = m_clients.begin(); it != m_clients.end(); ++it) {
            CloseSocket(it->first);
            delete(it->second);
        }
        m_clients.clear();
        // Очистим структуру polls
        free(m_polls);
        m_polls = NULL;
        m_polls_size = 0;
        m_polls_cap = 0;
        return true;
    }
    
protected:
    // Добавляем клиента
    Client* AddClient(SOCKET& sock)
    {
        // Если такой сокет уже есть - просто очистим буферы
        ClientMap::iterator it = m_clients.find(sock);
        if (it != m_clients.end()) {
            it->second->ClearBuffers();
            return it->second;
        }
        struct pollfd* pollstr = AddToPoll(sock);
        it = m_clients.insert(ClientMap::value_type(sock,new Client(pollstr))).first;
        return it->second;
    }
    // Удаляем клиента
    void RemoveClient(SOCKET sock)
    {
        // Удаляем данные клиента
        ClientMap::iterator it = m_clients.find(sock);
        if (it != m_clients.end()) {
            CloseSocket(sock);
            delete(it->second);
            m_clients.erase(it);
        }
        // Удаляем структуру из Poll
        RemoveFromPoll(sock);
    }
private:
    struct pollfd* AddToPoll(SOCKET& sock)
    {
        // Актуализируем размер структуры poll
        if (!m_polls) {
            m_polls_cap = POLL_ARRAY_GROW_STEP;
            m_polls = (struct pollfd*)malloc(m_polls_cap * sizeof(struct pollfd));
            memset(m_polls,0,m_polls_cap * sizeof(struct pollfd));
        } else if (m_polls_cap < m_polls_size+1) {
            size_t old_cap = m_polls_cap;
            m_polls_cap += POLL_ARRAY_GROW_STEP;
            m_polls = (struct pollfd*)realloc(m_polls,sizeof(struct pollfd)*m_polls_cap);
            memset(m_polls+old_cap*sizeof(struct pollfd),0,(m_polls_cap-old_cap)*sizeof(struct pollfd));
        }
        // Записываем в структуру данные нового сокета
        m_polls[m_polls_size].fd = sock;
        m_polls[m_polls_size].events |= POLLIN;
        m_polls_size++;
        return m_polls+m_polls_size-1;
    }
    void RemoveFromPoll(SOCKET& sock)
    {
        // Удаляем структуру клиента из массива poll
        for (size_t i = 0; i < m_polls_size; ++i) {
            if (m_polls[i].fd == sock) {
                if (i < m_polls_size-1)
                    memmove(m_polls+i,m_polls+i+1,(m_polls_size-i-1)*sizeof(struct pollfd));
                else
                    memset(m_polls+i,0,sizeof(struct pollfd));
                m_polls_size--;
                // Актуализируем размер структуры poll 
                if (m_polls_cap - m_polls_size >= POLL_ARRAY_GROW_STEP*2) {
                    m_polls_cap -= POLL_ARRAY_GROW_STEP;
                    m_polls = (struct pollfd*)realloc(m_polls,m_polls_cap);
                }
            }
        }
    }
    void CloseSocket(SOCKET sock)
    {
#if defined (WIN32)
        shutdown(sock,SD_BOTH);
        closesocket(sock);
#else
        shutdown(sock,SHUT_RDWR);
        close(sock);
#endif
    }
private:
    SOCKET            m_srv_sock;
    ClientMap         m_clients;
    struct pollfd*    m_polls;
    size_t            m_polls_size;
    size_t            m_polls_cap;
};

int main (int argc, char** argv)
{
    if (argc < 3) {
        std::cout << "Usage: server ip port" << std::endl;
        return -1;
    }
    Server srv;
    // Инициализируем библиотеку сокетов (под Windows)
#if defined (WIN32)
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2,2), &wsaData);
#else
    // Игнорируем SIGPIPE сигнал
    // Чтобы программа не терминировалась при поптыке записи в закрытый сокет
    signal(SIGPIPE, SIG_IGN);
#endif
    if (!srv.Start(argv[1],atoi(argv[2]))) {
        std::cout << "Terminating...";
#if defined (WIN32)
    WSACleanup();
#endif   
        return -1;
    }
    for(;srv.Iteration(););
    srv.Stop();
#if defined (WIN32)
    WSACleanup();
#endif
    return 0;
}