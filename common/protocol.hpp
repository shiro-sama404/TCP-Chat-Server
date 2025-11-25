#pragma once

#include <nlohmann/json.hpp>
#include <string>

/**
 * Módulo Protocol
 * ---------------
 * Define tipos de mensagens, validação e serialização/deserialização
 * do protocolo JSON do mensageiro.
 */

namespace Protocol
{

/**
 * Tipos de mensagens do protocolo
 */
enum class MessageType
{
    // Comandos do cliente
    REGISTER,       // Registro de novo usuário
    LOGIN,          // Login de usuário existente
    LOGOUT,         // Desconexão
    SEND_MSG,       // Envio de mensagem direta
    LIST_USERS,     // Solicitação da lista de usuários
    DELETE_USER,    // Remoção de conta
    
    // Respostas do servidor
    OK,             // Confirmação genérica de sucesso
    LOGIN_OK,       // Confirmação de login
    ERROR_MSG,      // Mensagem de erro
    DELIVER_MSG,    // Entrega de mensagem recebida
    USERS,          // Lista de usuários ativos/cadastrados
    
    UNKNOWN         // Tipo desconhecido ou inválido
};

/**
 * Tipos de erro possíveis
 */
enum class ErrorType
{
    NICK_TAKEN,             // Apelido já está em uso
    BAD_FORMAT,             // Formato da mensagem incorreto
    NO_SUCH_USER,           // Usuário não encontrado
    ALREADY_ONLINE,         // Usuário já está logado
    UNAUTHORIZED,           // Ação não autorizada (ex: enviar msg sem login)
    BAD_STATE,              // Estado inválido do servidor/cliente
    UNKNOWN_COMMAND,        // Comando não reconhecido
    INTERNAL_SERVER_ERROR   // Erro interno genérico
};

// Limites do protocolo
constexpr size_t MAX_NICKNAME_LENGTH = 32;
constexpr size_t MAX_FULLNAME_LENGTH = 128;
constexpr size_t MAX_MESSAGE_LENGTH = 4096;
constexpr size_t MAX_JSON_SIZE = 8192;

/**
 * Estrutura de informações de um usuário
 */
struct UserInfo
{
    std::string nickname;
    std::string fullName;
    bool isOnline;
};

// ==================== VALIDAÇÃO ====================
bool isValidNickname(const std::string& nick);
bool isValidFullName(const std::string& name);
bool isValidMessage(const std::string& msg);

// ==================== CONVERSÃO DE TIPOS ====================
MessageType stringToMessageType(const std::string& type);
std::string messageTypeToString(MessageType type);
ErrorType stringToErrorType(const std::string& error);
std::string errorTypeToString(ErrorType error);

// ==================== BUILDERS - REQUISIÇÕES (Cliente -> Servidor) ====================
nlohmann::json buildRegisterRequest(const std::string& nickname, const std::string& fullName);
nlohmann::json buildLoginRequest(const std::string& nickname);
nlohmann::json buildLogoutRequest();
nlohmann::json buildSendMessageRequest(const std::string& to, const std::string& text);
nlohmann::json buildListUsersRequest();
nlohmann::json buildDeleteUserRequest(const std::string& nickname);

// ==================== BUILDERS - RESPOSTAS (Servidor -> Cliente) ====================
nlohmann::json buildOkResponse();
nlohmann::json buildLoginOkResponse(const std::string& nickname);
nlohmann::json buildErrorResponse(ErrorType error);
nlohmann::json buildDeliverMessage(const std::string& from, const std::string& text, time_t timestamp);
nlohmann::json buildUsersListResponse(const std::vector<UserInfo>& users);

// ==================== PARSING SEGURO ====================
class ParseException : public std::runtime_error
{
public:
    explicit ParseException(const std::string& msg) : std::runtime_error(msg) {}
};

MessageType parseMessageType(const nlohmann::json& j);
std::string parseNickname(const nlohmann::json& j);
std::string parseFullName(const nlohmann::json& j);
std::string parseMessageText(const nlohmann::json& j);
std::string parseRecipient(const nlohmann::json& j);

} // namespace Protocol