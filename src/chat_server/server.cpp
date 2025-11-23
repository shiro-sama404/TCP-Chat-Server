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
#include <fcntl.h>
#include <errno.h>
#include <thread>
#include <chrono>

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

    // (Opcional) permitir reutilizar a porta rapidamente
    int opt = 1;
    setsockopt(server_sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);

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
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        cout << "[Server] Nova conexão aceita (FD: " << client_sockfd << ") de: " << client_ip << endl;

        // Cria a thread worker (handleClient)
        thread client_handler(&Server::handleClient, this, client_sockfd, string(client_ip));
        client_handler.detach();
    }
}

// --- FASE 3: WORKER E LIMPEZA DE SESSÃO (handleClient) ---

void Server::handleClient(int client_sockfd, const string& client_ip) {
    // Colocamos o socket em modo não-bloqueante para gerenciar leitura incremental
    int flags = fcntl(client_sockfd, F_GETFL, 0);
    if (flags >= 0) {
        fcntl(client_sockfd, F_SETFL, flags | O_NONBLOCK);
    }

    string inbuf;

    try {
        while (isRunning) {
            // Tentar ler dados disponíveis
            char buf[4096];
            ssize_t n = recv(client_sockfd, buf, sizeof(buf), 0);

            if (n > 0) {
                inbuf.append(buf, buf + n);

                // Processa todas as linhas completas (terminadas em '\n')
                size_t pos;
                while ((pos = inbuf.find('\n')) != string::npos) {
                    string line = inbuf.substr(0, pos);
                    // remove a linha do buffer, inclusive o '\n'
                    inbuf.erase(0, pos + 1);

                    // Ignora linhas vazias
                    if (line.empty()) continue;

                    string response;
                    try {
                        response = processCommand(line, client_sockfd);
                    } catch (const exception& e) {
                        // Em caso de erro inesperado no processamento, retorna um erro genérico
                        json err = {
                            {"type", "ERROR"},
                            {"payload", { {"message", "INTERNAL_SERVER_ERROR"} }}
                        };
                        response = err.dump();
                    }

                    if (!response.empty()) {
                        // tenta enviar; se falhar, vamos fechar a conexão
                        if (!sendToClient(client_sockfd, response)) {
                            throw runtime_error("Erro ao enviar resposta ao cliente.");
                        }
                    }
                }
            }
            else if (n == 0) {
                // Conexão fechada pelo cliente
                throw runtime_error("Conexão fechada pelo cliente.");
            }
            else { // n < 0
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // Nenhum dado no momento; relaxa um pouco
                    this_thread::sleep_for(chrono::milliseconds(5));
                    continue;
                } else if (errno == EINTR) {
                    continue;
                } else {
                    // Erro de rede
                    throw runtime_error(string("Erro de rede (recv): ") + strerror(errno));
                }
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

    // Fecha o socket do cliente
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
                return json({{"type","ERROR"},{"payload", {{"message","BAD_FORMAT"}}}}).dump();
            if (users.count(apelido))
                return json({{"type","ERROR"},{"payload", {{"message","NICK_TAKEN"}}}}).dump();
            if (!nickname.empty())
                return json({{"type","ERROR"},{"payload", {{"message","BAD_STATE"}}}}).dump();

            users[apelido] = {fullname, false};
            return json({{"type","OK"}}).dump();
        }

        if (command_type == "LOGIN") {
            string apelido = request["payload"].value("nickname", "");

            if (apelido.empty())
                return json({{"type","ERROR"},{"payload", {{"message","BAD_FORMAT"}}}}).dump();
            if (users.find(apelido) == users.end())
                return json({{"type","ERROR"},{"payload", {{"message","NO_SUCH_USER"}}}}).dump();
            if (sessions.count(apelido))
                return json({{"type","ERROR"},{"payload", {{"message","ALREADY_ONLINE"}}}}).dump();
            if (!nickname.empty())
                return json({{"type","ERROR"},{"payload", {{"message","BAD_STATE"}}}}).dump();

            sessions[apelido] = client_sockfd;
            fdToNickname[client_sockfd] = apelido;
            users[apelido].isLogged = true;

            deliverPendingMessages(client_sockfd, apelido);

            json resp = {
                {"type","LOGIN_OK"},
                {"payload", { {"nickname", apelido} }}
            };
            return resp.dump();
        }

        if (command_type == "LOGOUT") {
            if (nickname.empty())
                return json({{"type","ERROR"},{"payload", {{"message","BAD_STATE"}}}}).dump();

            // Atualiza estado do usuário sem fechar o socket imediatamente
            users[nickname].isLogged = false;
            sessions.erase(nickname);
            fdToNickname.erase(client_sockfd);

            json resp = {{"type","OK"}};
            return resp.dump();
        }

        if (command_type == "SEND_MSG") {
            if (nickname.empty())
                return json({{"type","ERROR"},{"payload", {{"message","UNAUTHORIZED"}}}}).dump();

            string to_nick = request["payload"].value("to", "");
            string text = request["payload"].value("text", "");

            if (to_nick.empty() || text.empty())
                return json({{"type","ERROR"},{"payload", {{"message","BAD_FORMAT"}}}}).dump();
            if (users.find(to_nick) == users.end())
                return json({{"type","ERROR"},{"payload", {{"message","NO_SUCH_USER"}}}}).dump();

            time_t now = time(nullptr);
            json deliver_msg = {
                {"type","DELIVER_MSG"},
                {"from", nickname},
                {"payload", { {"text", text}, {"ts", now} }}
            };

            if (sessions.count(to_nick)) {
                sendToClient(sessions.at(to_nick), deliver_msg.dump());
            } else {
                messageQueues[to_nick].push(deliver_msg.dump());
            }

            return json({{"type","OK"}}).dump();
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
            json resp = { {"type","USERS"}, {"payload", { {"users", user_list} }} };
            return resp.dump();
        }

        if (command_type == "DELETE_USER") {
            string apelido = request["payload"].value("nickname", "");

            if (apelido.empty())
                return json({{"type","ERROR"},{"payload", {{"message","BAD_FORMAT"}}}}).dump();
            if (users.find(apelido) == users.end())
                return json({{"type","ERROR"},{"payload", {{"message","NO_SUCH_USER"}}}}).dump();
            if (users.at(apelido).isLogged)
                return json({{"type","ERROR"},{"payload", {{"message","BAD_STATE"}}}}).dump();

            // Remove o usuário de todas as estruturas
            users.erase(apelido);
            messageQueues.erase(apelido);
            sessions.erase(apelido);
            for (auto it = fdToNickname.begin(); it != fdToNickname.end(); ) {
                if (it->second == apelido)
                    it = fdToNickname.erase(it);
                else
                    ++it;
            }

            return json({{"type","OK"}}).dump();
        }


        return json({{"type","ERROR"},{"payload", {{"message","UNKNOWN_COMMAND"}}}}).dump();

    } catch (const json::parse_error& e) {
        return json({{"type","ERROR"},{"payload", {{"message","BAD_FORMAT"}}}}).dump();
    } catch (const exception& e) {
        return json({{"type","ERROR"},{"payload", {{"message","INTERNAL_SERVER_ERROR"}}}}).dump();
    }
}
