#include "protocol.hpp"
#include <algorithm>
#include <cctype>

using json = nlohmann::json;

namespace Protocol
{

// ==================== VALIDAÇÃO ====================

bool isValidNickname(const std::string& nick)
{
    if (nick.empty() || nick.length() > MAX_NICKNAME_LENGTH)
        return false;
    
    // Apenas alfanuméricos e underscore
    return std::all_of(nick.begin(), nick.end(), [](char c)
    {
        return std::isalnum(c) || c == '_';
    });
}

bool isValidFullName(const std::string& name)
{
    if (name.empty() || name.length() > MAX_FULLNAME_LENGTH)
        return false;
    
    // Não pode ter apenas espaços
    return std::any_of(name.begin(), name.end(), [](char c)
    {
        return !std::isspace(c);
    });
}

bool isValidMessage(const std::string& msg)
{
    return !msg.empty() && msg.length() <= MAX_MESSAGE_LENGTH;
}

// ==================== CONVERSÃO DE TIPOS ====================

MessageType stringToMessageType(const std::string& type)
{
    if (type == "REGISTER") return MessageType::REGISTER;
    if (type == "LOGIN") return MessageType::LOGIN;
    if (type == "LOGOUT") return MessageType::LOGOUT;
    if (type == "SEND_MSG") return MessageType::SEND_MSG;
    if (type == "LIST_USERS") return MessageType::LIST_USERS;
    if (type == "DELETE_USER") return MessageType::DELETE_USER;
    if (type == "OK") return MessageType::OK;
    if (type == "LOGIN_OK") return MessageType::LOGIN_OK;
    if (type == "ERROR") return MessageType::ERROR_MSG;
    if (type == "DELIVER_MSG") return MessageType::DELIVER_MSG;
    if (type == "USERS") return MessageType::USERS;
    return MessageType::UNKNOWN;
}

std::string messageTypeToString(MessageType type)
{
    switch (type)
    {
        case MessageType::REGISTER: return "REGISTER";
        case MessageType::LOGIN: return "LOGIN";
        case MessageType::LOGOUT: return "LOGOUT";
        case MessageType::SEND_MSG: return "SEND_MSG";
        case MessageType::LIST_USERS: return "LIST_USERS";
        case MessageType::DELETE_USER: return "DELETE_USER";
        case MessageType::OK: return "OK";
        case MessageType::LOGIN_OK: return "LOGIN_OK";
        case MessageType::ERROR_MSG: return "ERROR";
        case MessageType::DELIVER_MSG: return "DELIVER_MSG";
        case MessageType::USERS: return "USERS";
        default: return "UNKNOWN";
    }
}

ErrorType stringToErrorType(const std::string& error)
{
    if (error == "NICK_TAKEN") return ErrorType::NICK_TAKEN;
    if (error == "BAD_FORMAT") return ErrorType::BAD_FORMAT;
    if (error == "NO_SUCH_USER") return ErrorType::NO_SUCH_USER;
    if (error == "ALREADY_ONLINE") return ErrorType::ALREADY_ONLINE;
    if (error == "UNAUTHORIZED") return ErrorType::UNAUTHORIZED;
    if (error == "BAD_STATE") return ErrorType::BAD_STATE;
    if (error == "UNKNOWN_COMMAND") return ErrorType::UNKNOWN_COMMAND;
    if (error == "INTERNAL_SERVER_ERROR") return ErrorType::INTERNAL_SERVER_ERROR;
    return ErrorType::INTERNAL_SERVER_ERROR;
}

std::string errorTypeToString(ErrorType error)
{
    switch (error)
    {
        case ErrorType::NICK_TAKEN: return "NICK_TAKEN";
        case ErrorType::BAD_FORMAT: return "BAD_FORMAT";
        case ErrorType::NO_SUCH_USER: return "NO_SUCH_USER";
        case ErrorType::ALREADY_ONLINE: return "ALREADY_ONLINE";
        case ErrorType::UNAUTHORIZED: return "UNAUTHORIZED";
        case ErrorType::BAD_STATE: return "BAD_STATE";
        case ErrorType::UNKNOWN_COMMAND: return "UNKNOWN_COMMAND";
        case ErrorType::INTERNAL_SERVER_ERROR: return "INTERNAL_SERVER_ERROR";
    }
    return "INTERNAL_SERVER_ERROR";
}

// ==================== BUILDERS - REQUISIÇÕES ====================

json buildRegisterRequest(const std::string& nickname, const std::string& fullName)
{
    return {
        {"type", "REGISTER"},
        {"payload", {
            {"nickname", nickname},
            {"fullname", fullName}
        }}
    };
}

json buildLoginRequest(const std::string& nickname)
{
    return {
        {"type", "LOGIN"},
        {"payload", {
            {"nickname", nickname}
        }}
    };
}

json buildLogoutRequest()
{
    return {
        {"type", "LOGOUT"},
        {"payload", json::object()}
    };
}

json buildSendMessageRequest(const std::string& to, const std::string& text)
{
    return {
        {"type", "SEND_MSG"},
        {"payload", {
            {"to", to},
            {"text", text}
        }}
    };
}

json buildListUsersRequest() {
    return {
        {"type", "LIST_USERS"},
        {"payload", json::object()}
    };
}

json buildDeleteUserRequest(const std::string& nickname)
{
    return {
        {"type", "DELETE_USER"},
        {"payload", {
            {"nickname", nickname}
        }}
    };
}

// ==================== BUILDERS - RESPOSTAS ====================

json buildOkResponse()
{
    return {{"type", "OK"}};
}

json buildLoginOkResponse(const std::string& nickname)
{
    return {
        {"type", "LOGIN_OK"},
        {"payload", {{"nickname", nickname}}}
    };
}

json buildErrorResponse(ErrorType error)
{
    return {
        {"type", "ERROR"},
        {"payload", {{"message", errorTypeToString(error)}}}
    };
}

json buildDeliverMessage(const std::string& from, const std::string& text, time_t timestamp)
{
    return {
        {"type", "DELIVER_MSG"},
        {"from", from},
        {"payload", {
            {"text", text},
            {"ts", timestamp}
        }}
    };
}

json buildUsersListResponse(const std::vector<UserInfo>& users)
{
    json user_list = json::array();
    for (const auto& user : users)
        user_list.push_back({
            {"nick", user.nickname},
            {"online", user.isOnline},
            {"name", user.fullName}
        });
        
    return {
        {"type", "USERS"},
        {"payload", {{"users", user_list}}}
    };
}

// ==================== PARSING SEGURO ====================

MessageType parseMessageType(const json& j)
{
    if (!j.contains("type") || !j["type"].is_string())
        throw ParseException("Campo 'type' ausente ou inválido");
    return stringToMessageType(j["type"].get<std::string>());
}

std::string parseNickname(const json& j)
{
    if (!j.contains("payload") || !j["payload"].contains("nickname"))
        throw ParseException("Campo 'nickname' ausente");
    
    std::string nick = j["payload"]["nickname"].get<std::string>();
    if (!isValidNickname(nick))
        throw ParseException("Apelido inválido");
    return nick;
}

std::string parseFullName(const json& j)
{
    if (!j.contains("payload") || !j["payload"].contains("fullname"))
        throw ParseException("Campo 'fullname' ausente");
    
    std::string name = j["payload"]["fullname"].get<std::string>();
    if (!isValidFullName(name))
        throw ParseException("Nome completo inválido");
    return name;
}

std::string parseMessageText(const json& j)
{
    if (!j.contains("payload") || !j["payload"].contains("text"))
        throw ParseException("Campo 'text' ausente");
    
    std::string text = j["payload"]["text"].get<std::string>();
    if (!isValidMessage(text))
        throw ParseException("Mensagem inválida ou muito longa");
    return text;
}

std::string parseRecipient(const json& j)
{
    if (!j.contains("payload") || !j["payload"].contains("to"))
        throw ParseException("Campo 'to' ausente");
    
    std::string to = j["payload"]["to"].get<std::string>();
    if (!isValidNickname(to))
        throw ParseException("Destinatário inválido");
    return to;
}

} // namespace Protocol