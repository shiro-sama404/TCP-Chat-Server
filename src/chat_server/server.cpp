#include "server.hpp"
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdexcept>
#include <cstring>
#include <nlohmann/json.hpp>

using namespace std;
using json = nlohmann::json;

// --- Chamadas Auxiliares de Protocolo (send com framing) ---

// Funcao para enviar a mensagem JSON completa com o framing "\n"
bool Server::sendToClient(int sockfd, const string& json_message) {
    string message_with_framing = json_message + "\n";
    ssize_t bytesSent = send(sockfd, message_with_framing.c_str(), message_with_framing.size(), 0);
    return bytesSent > 0;
}

// --- CONSTRUTOR/DESTRUTOR ---

Server::Server(int p) : port(p), server_sockfd(-1), isRunning(false) {}

Server::~Server() {
    if (isRunning) {
        isRunning = false;
        // Tenta fechar o socket para desbloquear o accept
        if (server_sockfd >= 0) {
            shutdown(server_sockfd, SHUT_RDWR); // Chamada de baixo nível
            close(server_sockfd);
        }
        if (acceptorThread.joinable()) {
            acceptorThread.join();
        }
    }
}

// --- LÓGICA DE INICIALIZAÇÃO DO SOCKET (bind, listen) ---

bool Server::start() {
    // 1. CHAMA socket() para criar o socket TCP
    server_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sockfd < 0) {
        perror("[Server] Erro ao criar socket");
        return false;
    }
    
    // Previne "Address already in use" (SO_REUSEADDR)
    int opt = 1;
    setsockopt(server_sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY; // Aceita conexões em qualquer IP
    serv_addr.sin_port = htons(port);

    // 2. CHAMA bind()
    if (bind(server_sockfd, (sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("[Server] Erro ao dar bind na porta");
        close(server_sockfd);
        return false;
    }

    // 3. CHAMA listen()
    if (listen(server_sockfd, 10) < 0) { // 10 é o backlog (fila de espera)
        perror("[Server] Erro ao dar listen");
        close(server_sockfd);
        return false;
    }

    isRunning = true;
    cout << "[Server] Mensageiro pronto na porta " << port << endl;

    // Inicia a thread acceptor
    acceptorThread = thread(&Server::acceptorLoop, this);
    return true;
}

void Server::run() {
    if (start()) {
        acceptorThread.join();
    }
}

// --- LÓGICA DE ACEITAÇÃO DE CONEXÕES (accept) ---

void Server::acceptorLoop() {
    sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    while (isRunning) {
        // 4. CHAMA accept() - BLOQUEANTE
        int client_sockfd = accept(server_sockfd, (sockaddr*)&client_addr, &client_len);

        if (client_sockfd < 0) {
            if (isRunning)
                perror("[Server] Erro em accept");
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        cout << "[Server] Nova conexão aceita (FD: " << client_sockfd << ") de: " << client_ip << endl;

        // --- MODELO DE CONCORRÊNCIA (BÔNUS) ---
        // Cria uma thread worker para o cliente e a solta (detach)
        thread client_handler(&Server::handleClient, this, client_sockfd, string(client_ip));
        client_handler.detach(); 
    }
}

// --- LÓGICA DE TRATAMENTO DO CLIENTE (recv) ---

void Server::handleClient(int client_sockfd, const string& client_ip) {
    string raw_message;
    
    // Funcao auxiliar para receber uma mensagem completa (com framing)
    auto receiveLine = [client_sockfd, client_ip, this]() -> string {
        string buffer;
        char ch;
        // Loop de baixo nível para ler byte a byte
        while (true) {
            ssize_t bytes = recv(client_sockfd, &ch, 1, 0);
            
            if (bytes == 0) {
                // Conexão fechada pelo cliente (graceful shutdown)
                throw runtime_error("Conexão fechada pelo cliente.");
            }
            if (bytes < 0) {
                // Erro de rede (ex: timeout)
                throw runtime_error("Erro de rede/queda de conexão.");
            }
            
            if (ch == '\n') break;
            buffer += ch;
        }
        return buffer;
    };


    try {
        // Loop de recepção de comandos do cliente (a thread bloqueia em recv)
        while (isRunning) {
            raw_message = receiveLine();

            // Roteia a mensagem para o processador de comandos
            string response = processCommand(raw_message, client_sockfd);

            // Envia a resposta do comando (OK, ERROR, etc.)
            if (!response.empty()) {
                sendToClient(client_sockfd, response);
            }
        }
    } catch (const exception& e) {
        cerr << "[Server-Worker] Cliente (FD: " << client_sockfd << ") desconectado. Motivo: " << e.what() << endl;
    }

    // --- LIMPEZA DE SESSÃO ---
    // Remove o socket e libera o apelido
    cleanupSession(client_sockfd);
}

void Server::cleanupSession(int client_sockfd) {
    lock_guard<mutex> lock(stateMutex);
    
    // 1. Encontra o apelido pelo File Descriptor
    auto fd_it = fdToNickname.find(client_sockfd);
    if (fd_it != fdToNickname.end()) {
        string nickname = fd_it->second;

        // 2. Remove da lista de sessões ativas
        sessions.erase(nickname);
        
        // 3. Limpa o mapeamento reverso
        fdToNickname.erase(fd_it);
        
        // 4. Atualiza o status do usuário
        auto user_it = users.find(nickname);
        if (user_it != users.end()) {
            user_it->second.isLogged = false;
        }

        cout << "[Server] Sessão limpa para: " << nickname << endl;
    }

    // 5. Fecha o socket do cliente
    close(client_sockfd);
}

// O método processCommand será implementado no próximo passo.
// ...