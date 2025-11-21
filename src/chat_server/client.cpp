#include "client.hpp"
#include <sys/socket.h>  
#include <netinet/in.h> 
#include <unistd.h>      
#include <cstring>       
#include <iostream>  
#include <stdexcept>
#include <sstream>
#include <algorithm> // Para std::reverse ou std::copy


using namespace std;


// 1. SUBSTITUIÇÃO MANUAL PARA htons()
// Converte um valor de 16 bits (porta) para big-endian (Network Byte Order).
uint16_t manual_htons(uint16_t port) {
    // Implementação simples e explícita do big-endian (byte-swapping para little-endian hosts)
    // Se a arquitetura do host for big-endian, o valor já está correto.
    // Se for little-endian, é necessário inverter os bytes.
    // Para simplificar e manter a portabilidade mínima, usamos bit-shifting:
    return (port >> 8) | (port << 8);
}

// 2. SUBSTITUIÇÃO MANUAL PARA inet_pton(AF_INET, ...)
// Converte a string IP "A.B.C.D" para o formato binário de 32 bits (in_addr_t)
in_addr_t manual_inet_pton(const char* ip_str) {
    stringstream ss(ip_str);
    string segment;
    in_addr_t result = 0;
    int octet_count = 0;

    while (getline(ss, segment, '.')) {
        if (octet_count >= 4) return 0; // Mais de 4 octetos

        int octet = 0;
        try {
            octet = stoi(segment);
        } catch (...) {
            return 0; // Falha na conversão de número
        }

        if (octet < 0 || octet > 255) return 0; // Octeto inválido

        // Combina o octeto ao resultado (em Network Byte Order - Big Endian)
        result = (result << 8) | (octet & 0xFF);
        octet_count++;
    }

    if (octet_count != 4) return 0; // IP incompleto

    return result; 
}


// --- IMPLEMENTAÇÃO DA CLASSE CLIENT  ---

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
    
    // 1. Criação do Socket (Chamada de baixo nível)
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("[Client] Erro ao criar socket");
        return false;
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    
    // SUBSTITUIÇÃO 1: htons()
    serv_addr.sin_port = manual_htons(port);

    // SUBSTITUIÇÃO 2: inet_pton()
    in_addr_t ip_binario = manual_inet_pton(host.c_str());
    
    // Verifica se o parsing manual falhou
    if (ip_binario == 0) {
        cerr << "[Client] Endereço de IP inválido. Use um IP, ex: 127.0.0.1" << endl;
        close(sockfd);
        return false;
    }

    // Copia o valor binário do IP para o s_addr (requer tratamento de Endianness)
    // Para simplificar no contexto do POSIX, onde a struct espera a ordem de rede,
    // o valor montado (ip_binario) é usado.
    serv_addr.sin_addr.s_addr = ip_binario;


    // 3. Conexão (Chamada de baixo nível)
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