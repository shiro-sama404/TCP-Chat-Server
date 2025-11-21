#include "client.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <algorithm>
#include <fcntl.h>       // ← necessário para O_NONBLOCK
#include <errno.h>

using namespace std;

// =============================
// SUBSTITUTOS MANUAIS
// =============================

// htons manual
uint16_t manual_htons(uint16_t port) {
    return (port >> 8) | (port << 8);
}

// inet_pton manual
in_addr_t manual_inet_pton(const char* ip_str) {
    stringstream ss(ip_str);
    string segment;
    in_addr_t result = 0;
    int octet_count = 0;

    while (getline(ss, segment, '.')) {
        if (octet_count >= 4)
            return (in_addr_t)(-1);

        int octet = 0;
        try {
            octet = stoi(segment);
        } catch (...) {
            return (in_addr_t)(-1);
        }

        if (octet < 0 || octet > 255)
            return (in_addr_t)(-1);

        result = (result << 8) | (octet & 0xFF);
        octet_count++;
    }

    if (octet_count != 4)
        return (in_addr_t)(-1);

    return result;
}

// =============================
// CLIENTE
// =============================

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

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("[Client] Erro ao criar socket");
        return false;
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = manual_htons(port);

    // in_addr_t ip_binario = manual_inet_pton(host.c_str());
    // if (ip_binario == (in_addr_t)(-1)) {
    //     cerr << "[Client] Endereço de IP inválido." << endl;
    //     close(sockfd);
    //     return false;
    // }

    // serv_addr.sin_addr.s_addr = ip_binario;

    serv_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    cout << "[DEBUG] Conectando em " << host << ":" << port << endl;

    if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("[Client] Erro ao conectar");
        close(sockfd);
        return false;
    }

    // ============
    //  MODO NÃO BLOQUEANTE
    // ============
    int flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

    connected = true;
    cout << "[Client] Conectado ao servidor em " << host << ":" << port << endl;

    return true;
}

bool Client::sendJson(const string& json) {
    if (!connected) {
        cerr << "[Client] Não está conectado." << endl;
        return false;
    }

    string message = json + "\n";
    ssize_t bytesSent = send(sockfd, message.c_str(), message.size(), 0);

    if (bytesSent < 0) {
        perror("[Client] Erro ao enviar mensagem");
        return false;
    }

    return true;
}

// =============================
// RECEBIMENTO NÃO BLOQUEANTE
// =============================
optional<string> Client::receiveJson() {
    if (!connected) return nullopt;

    string buffer;
    char ch;

    while (true) {
        ssize_t bytes = recv(sockfd, &ch, 1, 0);

        if (bytes < 0) {
            // Nenhum dado disponível agora → cliente continua
            if (errno == EWOULDBLOCK || errno == EAGAIN)
                return nullopt;

            // Outro erro → desconecta
            connected = false;
            return nullopt;
        }

        if (bytes == 0) {
            // Servidor fechou a conexão
            connected = false;
            return nullopt;
        }

        if (ch == '\n')
            break;

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

// =============================
// THREAD DE RECEPÇÃO
// =============================

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
    while (receiving) {

        if (!connected) {
            this_thread::sleep_for(chrono::milliseconds(10));
            continue;
        }
        
        
        auto msg = receiveJson();
        
        if (msg) {

            lock_guard<mutex> lock(queueMutex);
            messageQueue.push(*msg);
        }

        // evitar busy loop excessivo
        this_thread::sleep_for(chrono::milliseconds(5));
    }
}

optional<string> Client::popReceivedMessage() {
    lock_guard<mutex> lock(queueMutex);
    if (messageQueue.empty()) return nullopt;

    string msg = messageQueue.front();
    messageQueue.pop();
    return msg;
}
