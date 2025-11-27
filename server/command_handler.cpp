#include "command_handler.hpp"
#include "protocol.hpp"
#include <ctime>
#include <iostream>

using json = nlohmann::json;
using namespace Protocol;
using namespace std;

string CommandHandler::processCommand(const string& raw_message, int client_sockfd)
{
    try
    {
        json request = json::parse(raw_message);
        MessageType type = parseMessageType(request);

        switch (type)
        {
            case MessageType::REGISTER    : return handleRegister(request);
            case MessageType::LOGIN       : return handleLogin(request, client_sockfd);
            case MessageType::LOGOUT      : return handleLogout(client_sockfd);
            case MessageType::SEND_MSG    : return handleSendMessage(request, client_sockfd);
            case MessageType::LIST_USERS  : return handleListUsers();
            case MessageType::DELETE_USER : return handleDeleteUser(request, client_sockfd);
            
            default:
                return buildErrorResponse(ErrorType::UNKNOWN_COMMAND).dump();
        }
    }
    catch (const json::parse_error& e)
    {
        cerr << "[CommandHandler] Erro de parsing JSON: " << e.what() << endl;
        return buildErrorResponse(ErrorType::BAD_FORMAT).dump();
    }
    catch (const ParseException& e)
    {
        cerr << "[CommandHandler] Erro de protocolo: " << e.what() << endl;
        return buildErrorResponse(ErrorType::BAD_FORMAT).dump();
    }
    catch (const exception& e)
    {
        cerr << "[CommandHandler] Erro interno: " << e.what() << endl;
        return buildErrorResponse(ErrorType::INTERNAL_SERVER_ERROR).dump();
    }
}

// ==================== HANDLERS INDIVIDUAIS ====================

string CommandHandler::handleRegister(const json& request)
{
    lock_guard<mutex> lock(server.getStateMutex());
    
    try
    {
        string nickname = parseNickname(request);
        string fullName = parseFullName(request);
        
        // Verifica se apelido já existe
        if (server.getUsers().count(nickname))
            return buildErrorResponse(ErrorType::NICK_TAKEN).dump();
        
        // Registra usuário
        server.getUsers()[nickname] = {fullName, false};
        
        cout << "[Server] Usuário registrado: " << nickname << endl;
        return buildOkResponse().dump();
    }
    catch (const ParseException& e)
    {
        return buildErrorResponse(ErrorType::BAD_FORMAT).dump();
    }
}

string CommandHandler::handleLogin(const json& request, int client_sockfd)
{
    lock_guard<mutex> lock(server.getStateMutex());
    
    try
    {
        string nickname = parseNickname(request);
        
        // Verifica se usuário existe
        if (server.getUsers().find(nickname) == server.getUsers().end())
            return buildErrorResponse(ErrorType::NO_SUCH_USER).dump();
        
        // Verifica se já está online
        if (server.getSessions().count(nickname))
            return buildErrorResponse(ErrorType::ALREADY_ONLINE).dump();
        
        // Verifica se este socket já tem uma sessão
        if (server.getFdToNickname().count(client_sockfd))
            return buildErrorResponse(ErrorType::BAD_STATE).dump();
        
        // Cria sessão
        server.getSessions()[nickname] = client_sockfd;
        server.getFdToNickname()[client_sockfd] = nickname;
        server.getUsers()[nickname].isLogged = true;
        
        cout << "[Server] Login: " << nickname << " (FD: " << client_sockfd << ")" << endl;
        
        // Entrega mensagens pendentes (fora do lock para evitar deadlock)
        server.deliverPendingMessages(client_sockfd, nickname);
        
        return buildLoginOkResponse(nickname).dump();
    }
    catch (const ParseException& e)
    {
        return buildErrorResponse(ErrorType::BAD_FORMAT).dump();
    }
}

string CommandHandler::handleLogout(int client_sockfd)
{
    lock_guard<mutex> lock(server.getStateMutex());
    
    // Verifica se tem sessão
    auto it = server.getFdToNickname().find(client_sockfd);
    if (it == server.getFdToNickname().end())
        return buildErrorResponse(ErrorType::BAD_STATE).dump();
    
    string nickname = it->second;
    
    // Remove sessão
    server.getUsers()[nickname].isLogged = false;
    server.getSessions().erase(nickname);
    server.getFdToNickname().erase(client_sockfd);
    
    cout << "[Server] Logout: " << nickname << endl;
    return buildOkResponse().dump();
}

string CommandHandler::handleSendMessage(const json& request, int client_sockfd)
{
    lock_guard<mutex> lock(server.getStateMutex());
    
    // Verifica autenticação
    auto it = server.getFdToNickname().find(client_sockfd);
    if (it == server.getFdToNickname().end())
        return buildErrorResponse(ErrorType::UNAUTHORIZED).dump();
    
    string from = it->second;
    
    try
    {
        string to = parseRecipient(request);
        string text = parseMessageText(request);
        
        // Verifica se destinatário existe
        if (server.getUsers().find(to) == server.getUsers().end())
            return buildErrorResponse(ErrorType::NO_SUCH_USER).dump();
        
        // Cria mensagem de entrega
        time_t now = time(nullptr);
        json deliver_msg = buildDeliverMessage(from, text, now);
        
        // Entrega imediata ou store-and-forward
        if (server.getSessions().count(to))
        {
            // Online: entrega imediata
            server.sendToClient(server.getSessions().at(to), deliver_msg.dump());
            cout << "[Server] Mensagem entregue: " << from << " -> " << to << endl;
        }
        else
        {
            // Offline: armazena na fila
            server.getMessageQueues()[to].push(deliver_msg.dump());
            cout << "[Server] Mensagem armazenada: " << from << " -> " << to 
                 << " (offline)" << endl;
        }
        
        return buildOkResponse().dump();
    }
    catch (const ParseException& e)
    {
        return buildErrorResponse(ErrorType::BAD_FORMAT).dump();
    }
}

string CommandHandler::handleListUsers()
{
    lock_guard<mutex> lock(server.getStateMutex());
    
    vector<UserInfo> user_list;
    for (const auto& [nickname, data] : server.getUsers())
        user_list.push_back({nickname, data.fullName, data.isLogged});
    
    return buildUsersListResponse(user_list).dump();
}

string CommandHandler::handleDeleteUser(const json& request, int client_sockfd)
{
    lock_guard<mutex> lock(server.getStateMutex());
    
    try
    {
        string nickname = parseNickname(request);
        
        // Verifica se usuário existe
        if (server.getUsers().find(nickname) == server.getUsers().end())
            return buildErrorResponse(ErrorType::NO_SUCH_USER).dump();
        
        // Verifica se é o próprio usuário
        auto it = server.getFdToNickname().find(client_sockfd);
        if (it == server.getFdToNickname().end() || it->second != nickname)
            return buildErrorResponse(ErrorType::UNAUTHORIZED).dump();
        
        // Verifica se está online
        if (server.getUsers().at(nickname).isLogged)
        {
            server.getUsers()[nickname].isLogged = false;
            server.getSessions().erase(nickname);
            server.getFdToNickname().erase(client_sockfd);
            cout << "[Server] Sessão encerrada para deleção: " << nickname << endl;
        }
        else
            return buildErrorResponse(ErrorType::BAD_STATE).dump();
        
        // Remove usuário e dados associados
        server.getUsers().erase(nickname);
        server.getMessageQueues().erase(nickname);
        server.getSessions().erase(nickname);
        
        cout << "[Server] Usuário deletado: " << nickname << endl;
        return buildOkResponse().dump();
    }
    catch (const ParseException& e)
    {
        return buildErrorResponse(ErrorType::BAD_FORMAT).dump();
    }
}