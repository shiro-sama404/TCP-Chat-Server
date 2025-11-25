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

// Tipos de mensagens do protocolo
enum class MessageType
{
    // Comandos do cliente
    REGISTER,
    LOGIN,
    LOGOUT,
    SEND_MSG,
    LIST_USERS,
    DELETE_USER,
    
    // Respostas do servidor
    OK,
    LOGIN_OK,
    ERROR_MSG,
    DELIVER_MSG,
    USERS,
    
    UNKNOWN
};

// Tipos de erro
enum class ErrorType
{
    NICK_TAKEN,
    BAD_FORMAT,
    NO_SUCH_USER,
    ALREADY_ONLINE,
    UNAUTHORIZED,
    BAD_STATE,
    UNKNOWN_COMMAND,
    INTERNAL_SERVER_ERROR
};

// Limites do protocolo
constexpr size_t MAX_NICKNAME_LENGTH = 32;
constexpr size_t MAX_FULLNAME_LENGTH = 128;
constexpr size_t MAX_MESSAGE_LENGTH = 4096;
constexpr size_t MAX_JSON_SIZE = 8192;

// Estruturas de dados
struct UserInfo
{
    std::string nickname;
    std::string fullName;
    bool isOnline;
};

// Funções de validação
bool isValidNickname(const std::string& nick);
bool isValidFullName(const std::string& name);
bool isValidMessage(const std::string& msg);

// Conversão MessageType <-> string
MessageType stringToMessageType(const std::string& type);
std::string messageTypeToString(MessageType type);

// Conversão ErrorType <-> string
ErrorType stringToErrorType(const std::string& error);
std::string errorTypeToString(ErrorType error);

// Builders de mensagens (cliente -> servidor)
nlohmann::json buildRegisterRequest(const std::string& nickname, const std::string& fullName);
nlohmann::json buildLoginRequest(const std::string& nickname);
nlohmann::json buildLogoutRequest();
nlohmann::json buildSendMessageRequest(const std::string& to, const std::string& text);
nlohmann::json buildListUsersRequest();
nlohmann::json buildDeleteUserRequest(const std::string& nickname);

// Builders de respostas (servidor -> cliente)
nlohmann::json buildOkResponse();
nlohmann::json buildLoginOkResponse(const std::string& nickname);
nlohmann::json buildErrorResponse(ErrorType error);
nlohmann::json buildDeliverMessage(const std::string& from, const std::string& text, time_t timestamp);
nlohmann::json buildUsersListResponse(const std::vector<UserInfo>& users);

// Parsing seguro
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