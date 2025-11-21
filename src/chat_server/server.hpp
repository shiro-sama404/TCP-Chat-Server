#pragma once

#include <string>
#include <unordered_map>
#include <queue>
#include <mutex>
#include <thread>
#include <atomic>
#include <vector>

// Tipo de mensagem para a Fila Online (Store-and-Forward)
using MessageQueue = std::queue<std::string>;

// Dados de cada usuário registrado
struct UserData {
    std::string fullName; // Nome completo do usuário
    bool isLogged = false; // Status de login
};

class Server {
public:
    Server(int port);
    ~Server();

    bool start(); // Inicializa socket, bind, listen
    void run(); // Executa o loop principal

private:
    // --- Lógica de Sockets e Concorrência ---
    void acceptorLoop(); // Thread principal: bloqueia em accept()
    void handleClient(int client_sockfd, const std::string& client_ip); // Worker Thread por Cliente
    
    // --- Lógica de Protocolo e Estado ---
    std::string processCommand(const std::string& raw_message, int client_sockfd);
    bool sendToClient(int sockfd, const std::string& json_message);
    void cleanupSession(int client_sockfd); // Limpa o estado em caso de desconexão
    void deliverPendingMessages(int client_sockfd, const std::string& nickname);

    // --- Estruturas de Estado Thread-Safe ---
    std::mutex stateMutex;
    
    std::unordered_map<std::string, UserData> users;         // Apelido -> Dados do usuário
    std::unordered_map<std::string, int> sessions;           // Apelido -> File Descriptor (sockfd)
    std::unordered_map<std::string, MessageQueue> messageQueues; // Apelido -> Fila de mensagens pendentes
    std::unordered_map<int, std::string> fdToNickname;       // Mapeamento reverso (sockfd -> Apelido)

    // --- Variáveis de Sistema ---
    int port;
    int server_sockfd;
    std::atomic<bool> isRunning;
    std::thread acceptorThread;
};