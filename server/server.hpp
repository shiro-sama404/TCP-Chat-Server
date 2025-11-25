#pragma once

#include <atomic>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

class CommandHandler;

// ==================== ESTRUTURAS DE DADOS ====================

/**
 * Alias para a fila de mensagens pendentes (offline)
 */
using MessageQueue = std::queue<std::string>;

/**
 * Estrutura UserData
 * ------------------
 * Armazena as informações persistentes de um usuário registrado no servidor.
 */
struct UserData
{
    std::string fullName;
    bool isLogged = false;
};

// ==================== CLASSE SERVER ====================

/**
 * Classe Server
 * -------------
 * Gerencia o servidor TCP multithread.
 * Responsável por aceitar conexões, manter o estado global dos usuários,
 * gerenciar sessões e orquestrar o roteamento de mensagens.
 */
class Server
{
public:
    /**
     * Construtor do servidor.
     * @param port Porta TCP onde o servidor irá escutar conexões
     */
    explicit Server(int port);
    ~Server();

    /**
     * Inicializa o servidor.
     * Realiza a criação do socket, bind e listen.
     * @return true se a inicialização for bem-sucedida, false caso contrário
     */
    bool start();
    
    /**
     * Executa o loop principal do servidor.
     * Inicia a thread de aceitação (acceptor) e bloqueia até o encerramento.
     */
    void run();

    // ==================== ACESSO A DADOS (Thread-Safe via Mutex) ====================
    std::unordered_map<std::string, UserData>& getUsers()             { return users;         }
    std::unordered_map<std::string, int>& getSessions()               { return sessions;      }
    std::unordered_map<std::string, MessageQueue>& getMessageQueues() { return messageQueues; }
    std::unordered_map<int, std::string>& getFdToNickname()           { return fdToNickname;  }
    std::mutex& getStateMutex() { return stateMutex; }

    // ==================== OPERAÇÕES AUXILIARES ====================
    bool sendToClient(int sockfd, const std::string& json_message);
    void deliverPendingMessages(int client_sockfd, const std::string& nickname);

private:
    // Variáveis de sistema
    int port;
    int server_sockfd;
    std::atomic<bool> isRunning;
    std::thread acceptorThread;

    // Estruturas de estado (thread-safe via stateMutex)
    std::mutex stateMutex;
    std::unordered_map<std::string, UserData> users;
    std::unordered_map<std::string, int> sessions;
    std::unordered_map<std::string, MessageQueue> messageQueues;
    std::unordered_map<int, std::string> fdToNickname;

    /**
     * Loop principal de aceitação de conexões (Thread Acceptor).
     * Aceita novas conexões TCP e cria uma thread worker para cada cliente.
     */
    void acceptorLoop();
    
    /**
     * Lógica de processamento para um cliente conectado (Thread Worker).
     * Mantém um loop de leitura de comandos enquanto o cliente estiver conectado.
     * @param client_sockfd Socket do cliente conectado
     * @param client_ip Endereço IP do cliente (para log)
     */
    void handleClient(int client_sockfd, const std::string& client_ip);
    
    /**
     * Realiza a limpeza dos dados da sessão quando um cliente desconecta.
     * Remove entradas dos mapas de sessão e FD.
     * @param client_sockfd Socket do cliente que desconectou
     */
    void cleanupSession(int client_sockfd);

    // Command handler (friend pra acesso aos dados)
    friend class CommandHandler;
};