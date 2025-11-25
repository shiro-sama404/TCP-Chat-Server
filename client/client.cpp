#include "client.hpp"
#include "socket_utils.hpp"
#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>

using namespace std;

Client::Client()
    : sockfd(-1), connected(false), receiving(false) {}

Client::~Client()
{
    stopReceiverThread();
    disconnect();
}

bool Client::connectToServer(const string& host, int port)
{
    if (connected)
    {
        cerr << "[Client] Já conectado ao servidor." << endl;
        return false;
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        perror("[Client] Erro ao criar socket");
        return false;
    }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    // Converte IP string para binário
    if (inet_pton(AF_INET, host.c_str(), &serv_addr.sin_addr) <= 0)
    {
        cerr << "[Client] Endereço IP inválido: " << host << endl;
        close(sockfd);
        sockfd = -1;
        return false;
    }

    cout << "[Client] Conectando em " << host << ":" << port << "..." << endl;

    if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("[Client] Erro ao conectar");
        close(sockfd);
        sockfd = -1;
        return false;
    }

    // Define socket como não-bloqueante para a thread receptora
    if (!SocketUtils::setNonBlocking(sockfd))
    {
        cerr << "[Client] Erro ao configurar socket não-bloqueante" << endl;
        close(sockfd);
        sockfd = -1;
        return false;
    }

    connected = true;
    cout << "[Client] Conectado ao servidor!" << endl;
    return true;
}

bool Client::sendJson(const string& json)
{
    if (!connected)
    {
        cerr << "[Client] Não está conectado ao servidor." << endl;
        return false;
    }

    return SocketUtils::sendMessage(sockfd, json);
}

optional<string> Client::receiveJson()
{
    if (!connected) return nullopt;
    return SocketUtils::receiveMessage(sockfd, receiveBuffer);
}

void Client::disconnect()
{
    if (connected)
    {
        connected = false;
        SocketUtils::closeSocket(sockfd);
        cout << "[Client] Desconectado do servidor." << endl;
    }
}

// ==================== THREAD RECEPTORA ====================

void Client::startReceiverThread()
{
    if (!connected || receiving) return;

    receiving = true;
    receiverThread = thread(&Client::receiverLoop, this);
}

void Client::stopReceiverThread()
{
    if (receiving)
    {
        receiving = false;
        if (receiverThread.joinable())
            receiverThread.join();
    }
}

void Client::receiverLoop()
{
    while (receiving && connected)
    {
        auto msg = receiveJson();
        
        if (msg)
        {
            lock_guard<mutex> lock(queueMutex);
            messageQueue.push(*msg);
        }
        
        // Evita busy-wait: aguarda 10ms entre tentativas
        this_thread::sleep_for(chrono::milliseconds(10));
    }
}

optional<string> Client::popReceivedMessage()
{
    lock_guard<mutex> lock(queueMutex);
    
    if (messageQueue.empty()) return nullopt;

    string msg = messageQueue.front();
    messageQueue.pop();
    return msg;
}