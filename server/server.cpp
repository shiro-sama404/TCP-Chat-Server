#include "command_handler.hpp"
#include "server.hpp"
#include "socket_utils.hpp"
#include <arpa/inet.h>
#include <chrono>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

using namespace std;

// ==================== CONSTRUTOR/DESTRUTOR ====================

Server::Server(int p) : port(p), server_sockfd(-1), isRunning(false) {}

Server::~Server() {
    if (isRunning) {
        isRunning = false;
        if (server_sockfd >= 0) {
            shutdown(server_sockfd, SHUT_RDWR);
            close(server_sockfd);
        }
        if (acceptorThread.joinable()) {
            acceptorThread.join();
        }
    }
}

// ==================== INICIALIZAÇÃO ====================

bool Server::start() {
    // 1. Cria socket
    server_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sockfd < 0) {
        perror("[Server] Erro ao criar socket");
        return false;
    }

    // Permite reutilizar porta
    int opt = 1;
    setsockopt(server_sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 2. Configura endereço
    sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);

    // 3. Bind
    if (bind(server_sockfd, (sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("[Server] Erro ao fazer bind");
        close(server_sockfd);
        return false;
    }

    // 4. Listen
    if (listen(server_sockfd, 10) < 0) {
        perror("[Server] Erro ao fazer listen");
        close(server_sockfd);
        return false;
    }

    isRunning = true;
    cout << "[Server] Mensageiro iniciado na porta TCP: " << port << endl;

    return true;
}

void Server::run() {
    if (!start()) {
        cerr << "[Server] Falha ao iniciar servidor" << endl;
        return;
    }

    // Thread acceptor
    acceptorThread = thread(&Server::acceptorLoop, this);
    acceptorThread.join();
}

// ==================== ACCEPTOR LOOP ====================

void Server::acceptorLoop() {
    sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    while (isRunning) {
        int client_sockfd = accept(server_sockfd, (sockaddr*)&client_addr, &client_len);

        if (client_sockfd < 0) {
            if (isRunning) {
                perror("[Server] Erro em accept");
            }
            continue;
        }

        // Obtém IP do cliente
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        
        cout << "[Server] Nova conexão aceita (FD: " << client_sockfd 
             << ") de: " << client_ip << endl;

        // Cria thread worker para este cliente
        thread worker(&Server::handleClient, this, client_sockfd, string(client_ip));
        worker.detach();
    }
}

// ==================== CLIENT HANDLER ====================

void Server::handleClient(int client_sockfd, const string& client_ip) {
    // Configura socket como não-bloqueante
    if (!SocketUtils::setNonBlocking(client_sockfd)) {
        cerr << "[Server] Erro ao configurar socket não-bloqueante" << endl;
        close(client_sockfd);
        return;
    }

    CommandHandler handler(*this);
    string buffer;

    try {
        while (isRunning) {
            // Tenta receber mensagem
            auto msg_opt = SocketUtils::receiveMessage(client_sockfd, buffer);
            
            if (msg_opt) {
                // Mensagem completa recebida
                string response = handler.processCommand(*msg_opt, client_sockfd);
                
                if (!response.empty()) {
                    if (!sendToClient(client_sockfd, response)) {
                        throw runtime_error("Erro ao enviar resposta");
                    }
                }
            }
            else {
                // Sem dados ou erro
                // Verifica se houve desconexão
                char test;
                ssize_t n = recv(client_sockfd, &test, 1, MSG_PEEK | MSG_DONTWAIT);
                
                if (n == 0) {
                    // Conexão fechada
                    throw runtime_error("Conexão fechada pelo cliente");
                }
                else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                    // Erro real
                    throw runtime_error(string("Erro de rede: ") + strerror(errno));
                }
                
                // Sem dados disponíveis: aguarda
                this_thread::sleep_for(chrono::milliseconds(10));
            }
        }
    }
    catch (const exception& e) {
        cerr << "[Server] Cliente (FD: " << client_sockfd << ", IP: " << client_ip 
             << ") desconectado. Motivo: " << e.what() << endl;
    }

    cleanupSession(client_sockfd);
}

// ==================== LIMPEZA DE SESSÃO ====================

void Server::cleanupSession(int client_sockfd) {
    lock_guard<mutex> lock(stateMutex);

    auto it = fdToNickname.find(client_sockfd);
    if (it != fdToNickname.end()) {
        string nickname = it->second;

        // Remove sessão
        sessions.erase(nickname);
        fdToNickname.erase(it);

        // Marca usuário como offline
        if (users.count(nickname)) {
            users[nickname].isLogged = false;
        }

        cout << "[Server] Sessão limpa para: " << nickname << endl;
    }

    // Fecha socket
    SocketUtils::closeSocket(client_sockfd);
}

// ==================== OPERAÇÕES AUXILIARES ====================

bool Server::sendToClient(int sockfd, const string& json_message) {
    return SocketUtils::sendMessage(sockfd, json_message);
}

void Server::deliverPendingMessages(int client_sockfd, const string& nickname) {
    // Nota: chamador deve ter lock do stateMutex
    
    if (messageQueues.count(nickname)) {
        MessageQueue& queue = messageQueues[nickname];

        while (!queue.empty()) {
            string msg = queue.front();
            queue.pop();

            sendToClient(client_sockfd, msg);
            cout << "[Server] Mensagem pendente entregue a " << nickname << endl;
        }
    }
}