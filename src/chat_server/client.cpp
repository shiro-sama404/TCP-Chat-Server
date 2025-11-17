#include "client.hpp"
#include <sys/socket.h>  
#include <arpa/inet.h>   
#include <netdb.h>       
#include <unistd.h>      
#include <cstring>       
#include <iostream>  


using namespace std;


Client::Client()
    : sockfd(-1), connected(false), receiving(false) {}

Client::~Client() {
    stopReceiverThread();
    disconnect();
}

bool Client::connectToServer(const string& host, int port) {
    if (connected) {
        cerr << "[Client] Já conectado a um servidor." << endl;
        return false;
    }

    struct sockaddr_in serv_addr;
    struct hostent* server = gethostbyname(host.c_str());
    if (!server) {
        cerr << "[Client] Host não encontrado: " << host << endl;
        return false;
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("[Client] Erro ao criar socket");
        return false;
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    serv_addr.sin_port = htons(port);

    if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("[Client] Erro ao conectar");
        close(sockfd);
        return false;
    }

    connected = true;
    cout << "[Client] Conectado ao servidor em " << host << ":" << port << endl;
    return true;
}

bool Client::sendJson(const string& json) {
    if (!connected) {
        cerr << "[Client] Não está conectado." << endl;
        return false;
    }

    string message = json + "\n"; // framing por linha
    ssize_t bytesSent = send(sockfd, message.c_str(), message.size(), 0);

    if (bytesSent < 0) {
        perror("[Client] Erro ao enviar mensagem");
        return false;
    }

    return true;
}

optional<string> Client::receiveJson() {
    if (!connected) return nullopt;

    string buffer;
    char ch;
    while (true) {
        ssize_t bytes = recv(sockfd, &ch, 1, 0);
        if (bytes <= 0) {
            connected = false;
            return nullopt;
        }
        if (ch == '\n') break;
        buffer += ch;
    }
    return buffer;
}

void Client::disconnect() {
    if (connected) {
        close(sockfd);
        connected = false;
        cout << "[Client] Desconectado do servidor." << endl;
    }
}

void Client::startReceiverThread() {
    if (!connected || receiving) return;

    receiving = true;
    receiverThread = thread(&Client::receiverLoop, this);
}

void Client::stopReceiverThread() {
    if (receiving) {
        receiving = false;
        if (receiverThread.joinable()) receiverThread.join();
    }
}

void Client::receiverLoop() {
    while (receiving && connected) {
        auto msg = receiveJson();
        if (!msg) break;

        lock_guard<std::mutex> lock(queueMutex);
        messageQueue.push(*msg);
    }
}

optional<string> Client::popReceivedMessage() {
    lock_guard<mutex> lock(queueMutex);
    if (messageQueue.empty()) return nullopt;

    string msg = messageQueue.front();
    messageQueue.pop();
    return msg;
}
