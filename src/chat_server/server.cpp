#include "server.hpp"
#include <iostream>
#include <sys/socket.h>  
#include <netinet/in.h>  
#include <arpa/inet.h>   
#include <unistd.h>      
#include <cstring>       
#include <stdexcept>
#include <nlohmann/json.hpp> 
#include <ctime>

using namespace std;
using json = nlohmann::json;

// --- FUNÇÕES AUXILIARES DE PROTOCOLO ---

bool Server::sendToClient(int sockfd, const string& json_message) {
    string message_with_framing = json_message + "\n";
    const char* buffer = message_with_framing.c_str();
    size_t total_sent = 0;
    size_t len = message_with_framing.size();

    while (total_sent < len) {
        ssize_t bytesSent = send(sockfd, buffer + total_sent, len - total_sent, 0);
        if (bytesSent < 0) {
            return false;
        }
        total_sent += bytesSent;
    }
    return true;
}

string receiveLine(int sockfd) {
    string buffer;
    char ch;
    while (true) {
        ssize_t bytes = recv(sockfd, &ch, 1, 0);
        
        if (bytes == 0) {
            throw runtime_error("Conexão fechada pelo cliente.");
        }
        if (bytes < 0) {
            throw runtime_error("Erro de rede/queda de conexão.");
        }
        
        if (ch == '\n') break;
        buffer += ch;
    }
    return buffer;
}


// --- CONSTRUTOR/DESTRUTOR ---

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

// --- FASE 1: INICIALIZAÇÃO (socket, bind, listen) ---

bool Server::start() {
    // 1. CHAMA socket()
    server_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sockfd < 0) {
        perror("[Server] Erro ao criar socket");
        return false;
    }
    
    // --- SETSOCKOPT REMOVIDO PARA CONFORMIDADE ESTRITA ---
    
    sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY; 
    serv_addr.sin_port = htons(port); // htons mantido, pois é função de conversão de ordem de bytes

    // 2. CHAMA bind()
    if (bind(server_sockfd, (sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("[Server] Erro ao dar bind na porta");
        close(server_sockfd);
        return false;
    }

    // 3. CHAMA listen()
    if (listen(server_sockfd, 10) < 0) { 
        perror("[Server] Erro ao dar listen");
        close(server_sockfd);
        return false;
    }

    isRunning = true;
    cout << "[Server] Mensageiro pronto na porta TCP: " << port << endl;

    acceptorThread = thread(&Server::acceptorLoop, this);
    return true;
}

void Server::run() {
    if (start()) {
        acceptorThread.join();
    }
}

// --- FASE 2: ACCEPTOR LOOP (accept) ---

void Server::acceptorLoop() {
    sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    while (isRunning) {
        int client_sockfd = accept(server_sockfd, (sockaddr*)&client_addr, &client_len);

        if (client_sockfd < 0) {
            if (isRunning)
                perror("[Server] Erro em accept");
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
        // inet_ntop (chamada utilitária) é mantida para fins de debug no console
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN); 
        cout << "[Server] Nova conexão aceita (FD: " << client_sockfd << ") de: " << client_ip << endl;

        // Cria a thread worker (handleClient)
        thread client_handler(&Server::handleClient, this, client_sockfd, string(client_ip));
        client_handler.detach(); 
    }
}

// --- FASE 3: WORKER E LIMPEZA DE SESSÃO (handleClient) ---

void Server::handleClient(int client_sockfd, const string& client_ip) {
    string raw_message;
    
    try {
        while (isRunning) {
            raw_message = receiveLine(client_sockfd); 

            string response = processCommand(raw_message, client_sockfd);

            if (!response.empty()) {
                sendToClient(client_sockfd, response);
            }
        }
    } catch (const exception& e) {
        cerr << "[Server-Worker] Cliente (FD: " << client_sockfd << ") desconectado. Motivo: " << e.what() << endl;
    }

    cleanupSession(client_sockfd);
}

void Server::cleanupSession(int client_sockfd) {
    lock_guard<mutex> lock(stateMutex); 
    
    auto fd_it = fdToNickname.find(client_sockfd);
    if (fd_it != fdToNickname.end()) {
        string nickname = fd_it->second;

        sessions.erase(nickname);
        fdToNickname.erase(fd_it);
        
        auto user_it = users.find(nickname);
        if (user_it != users.end()) {
            user_it->second.isLogged = false;
        }

        cout << "[Server] Sessão limpa para: " << nickname << endl;
    }

    // Fecha o socket do cliente (Chamada de baixo nível)
    close(client_sockfd);
}

void Server::deliverPendingMessages(int client_sockfd, const std::string& nickname) {
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


// --- FASE 4: LÓGICA DE APLICAÇÃO (processCommand) ---

string Server::processCommand(const string& raw_message, int client_sockfd) {
    lock_guard<mutex> lock(stateMutex); 
    json request;

    try {
        request = json::parse(raw_message);
        string command_type = request.value("type", "UNKNOWN");
        
        string nickname;
        if (fdToNickname.count(client_sockfd)) {
            nickname = fdToNickname.at(client_sockfd); 
        }
        
        // --- ROTAS DE COMANDO (Registro, Login, Mensagens, etc.) ---

        if (command_type == "REGISTER") {
            string apelido = request["payload"].value("nickname", "");
            string fullname = request["payload"].value("fullname", "");

            if (apelido.empty() || fullname.empty())
                return R"({"type":"ERROR","payload":{"message":"BAD_FORMAT"}})";
            if (users.count(apelido))
                return R"({"type":"ERROR","payload":{"message":"NICK_TAKEN"}})";
            if (!nickname.empty()) 
                return R"({"type":"ERROR","payload":{"message":"BAD_STATE"}})";

            users[apelido] = {fullname, false};
            return R"({"type":"OK"})";
        }
        
        if (command_type == "LOGIN") {
            string apelido = request["payload"].value("nickname", "");

            if (apelido.empty())
                return R"({"type":"ERROR","payload":{"message":"BAD_FORMAT"}})";
            if (users.find(apelido) == users.end())
                return R"({"type":"ERROR","payload":{"message":"NO_SUCH_USER"}})";
            if (sessions.count(apelido))
                return R"({"type":"ERROR","payload":{"message":"ALREADY_ONLINE"}})";
            if (!nickname.empty())
                return R"({"type":"ERROR","payload":{"message":"BAD_STATE"}})";

            sessions[apelido] = client_sockfd;
            fdToNickname[client_sockfd] = apelido;
            users[apelido].isLogged = true;

            deliverPendingMessages(client_sockfd, apelido);
            
            return R"({"type":"LOGIN_OK","payload":{"nickname":")" + apelido + R"("}})";
        }
        
        if (command_type == "LOGOUT") {
            if (nickname.empty())
                return R"({"type":"ERROR","payload":{"message":"BAD_STATE"}})";

            // Fechar o socket de forma limpa
            shutdown(client_sockfd, SHUT_RDWR);
            return ""; 
        }

        if (command_type == "SEND_MSG") {
            if (nickname.empty())
                return R"({"type":"ERROR","payload":{"message":"UNAUTHORIZED"}})";
            
            string to_nick = request["payload"].value("to", "");
            string text = request["payload"].value("text", "");

            if (to_nick.empty() || text.empty())
                return R"({"type":"ERROR","payload":{"message":"BAD_FORMAT"}})";
            if (users.find(to_nick) == users.end())
                return R"({"type":"ERROR","payload":{"message":"NO_SUCH_USER"}})";

            time_t now = time(nullptr);
            string deliver_msg_json = R"({"type":"DELIVER_MSG", "from":")" + nickname + R"(", "payload":{"text":")" + text + R"(", "ts":)" + to_string(now) + R"(}})";

            if (sessions.count(to_nick)) {
                sendToClient(sessions.at(to_nick), deliver_msg_json);
            } else {
                messageQueues[to_nick].push(deliver_msg_json);
            }

            return R"({"type":"OK"})";
        }
        
        if (command_type == "LIST_USERS") {
            json user_list = json::array();
            for (const auto& pair : users) {
                user_list.push_back({
                    {"nick", pair.first},
                    {"online", pair.second.isLogged},
                    {"name", pair.second.fullName}
                });
            }
            return R"({"type":"USERS","payload":{"users":)" + user_list.dump() + R"(}})";
        }

        if (command_type == "DELETE_USER") {
            string apelido = request["payload"].value("nickname", "");
            
            if (apelido.empty() || apelido != nickname)
                 return R"({"type":"ERROR","payload":{"message":"UNAUTHORIZED"}})";
            if (users.find(apelido) == users.end())
                 return R"({"type":"ERROR","payload":{"message":"NO_SUCH_USER"}})";
            if (users.at(apelido).isLogged)
                return R"({"type":"ERROR","payload":{"message":"BAD_STATE"}})"; 

            users.erase(apelido);
            messageQueues.erase(apelido);
            return R"({"type":"OK"})";
        }

        return R"({"type":"ERROR","payload":{"message":"UNKNOWN_COMMAND"}})";
        
    } catch (const json::parse_error& e) {
        return R"({"type":"ERROR","payload":{"message":"BAD_FORMAT"}})";
    } catch (const exception& e) {
        return R"({"type":"ERROR","payload":{"message":"INTERNAL_SERVER_ERROR"}})";
    }
}