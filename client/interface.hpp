#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "client.hpp"

/**
 * Enumeração dos tipos de comandos disponíveis na interface
 */
enum class CommandType
{
    Register,   // register <apelido> "<Nome Completo>"
    Login,      // login <apelido>
    List,       // list
    Msg,        // msg <destinatário> <texto>
    Logout,     // logout
    Delete,     // delete <apelido>
    Quit,       // quit
    Unknown     // Comando não reconhecido
};

/**
 * Estrutura que representa um comando parseado
 */
struct Command
{
    CommandType type = CommandType::Unknown;
    std::vector<std::string> args;
};

/**
 * Classe Interface
 * ----------------
 * Responsável pela interface de linha de comando (CLI) do cliente.
 * Gerencia a interação com o usuário e a comunicação com o servidor.
 */
class Interface
{
public:
    Interface() = default;
    
    /**
     * Loop principal da interface.
     * Conecta ao servidor, inicia thread receptora e processa comandos do usuário.
     * 
     * @param client Referência para o objeto Client
     */
    void run(Client& client);
    
    /**
     * Exibe mensagem de ajuda com todos os comandos disponíveis
     */
    static void help();

private:
    /**
     * Faz parsing de uma linha de comando inserida pelo usuário
     * 
     * @param line Linha de comando
     * @return Estrutura Command com o comando parseado
     */
    static Command parse(const std::string& line);
    
    /**
     * Exibe o prompt para o usuário
     */
    static void prompt();
    
    /**
     * Exibe uma mensagem de erro formatada
     * 
     * @param msg Mensagem de erro
     */
    static void error(const std::string& msg);
    
    /**
     * Processa e exibe uma mensagem JSON recebida do servidor
     * 
     * @param msg Mensagem JSON do servidor
     */
    static void displayMessage(const nlohmann::json& msg);
};