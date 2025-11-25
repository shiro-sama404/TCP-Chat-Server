#pragma once

#include "server.hpp"
#include <nlohmann/json.hpp>
#include <string>

/**
 * Classe CommandHandler
 * ---------------------
 * Responsável por interpretar e processar as requisições do protocolo JSON.
 * Atua como a camada de controle (Controller), mediando a interação entre
 * as mensagens recebidas via rede e o estado global do servidor.
 */

class CommandHandler
{
public:
    explicit CommandHandler(Server& server) : server(server) {}

    /**
     * Lógica de processamento de um comando do protocolo.
     * Processa um comando JSON e retorna a resposta.
     * @param raw_message 
     * @param client_sockfd 
     * @return 
     */
    std::string processCommand(const std::string& raw_message, int client_sockfd);

private:
    Server& server;

    // ==================== Handlers Individuais ====================
    std::string handleRegister(const nlohmann::json& request);
    std::string handleLogin(const nlohmann::json& request, int client_sockfd);
    std::string handleLogout(int client_sockfd);
    std::string handleSendMessage(const nlohmann::json& request, int client_sockfd);
    std::string handleListUsers();
    std::string handleDeleteUser(const nlohmann::json& request, int client_sockfd);
};