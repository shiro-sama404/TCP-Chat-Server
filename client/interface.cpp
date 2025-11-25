#include "interface.hpp"
#include "protocol.hpp"
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>

using json = nlohmann::json;
using namespace std;

// ==================== CORES ANSI ====================
namespace Colors
{
    const string RESET = "\033[0m";
    const string GREEN = "\033[1;32m";
    const string RED = "\033[1;31m";
    const string BLUE = "\033[1;34m";
    const string YELLOW = "\033[1;33m";
    const string GRAY = "\033[1;30m";

    // Na realidade move o cursor para o início e limpa a linha (pra sobrescrever o prompt)
    const string CLEAR_LINE = "\r\033[K";
}

// ==================== INTERFACE ====================

void Interface::help()
{
    cout << R"(
┌───────────────────────────────────────────────┐
│     Chat-Um-a-Um - Mensageiro Rudimentar      │
└───────────────────────────────────────────────┘

Comandos disponíveis:

  register <apelido> "<Nome Completo>"
      → Registra um novo usuário.

  login <apelido>
      → Faz login com o apelido informado.

  list
      → Lista todos os usuários e seus status.

  msg <destinatário> <texto...>
      → Envia uma mensagem privada.

  logout
      → Faz logout da sessão atual.

  delete <apelido>
      → Remove a conta (deve estar deslogado).

  quit
      → Sai do programa.
)" << endl;
}

void Interface::prompt()
{
    cout << Colors::GREEN << "> " << Colors::RESET << flush;
}

void Interface::error(const string& msg)
{
    cerr << Colors::RED << "[Erro] " << Colors::RESET << msg << endl;
}

Command Interface::parse(const string& line)
{
    Command cmd;
    stringstream ss(line);
    string token;
    vector<string> parts;

    // Lê tokens respeitando aspas
    while (ss >> quoted(token))
        parts.push_back(token);

    if (ss.fail() && !ss.eof())
    {
        error("Erro de formatação: aspas desbalanceadas.");
        return cmd;
    }

    if (parts.empty()) return cmd;

    string name_cmd = parts[0];
    transform(name_cmd.begin(), name_cmd.end(), name_cmd.begin(), ::tolower);

    if (name_cmd == "register")
        if (parts.size() != 3)
            error("Uso: register <apelido> \"<Nome Completo>\"");
        else
        {
            cmd.type = CommandType::Register;
            cmd.args = {parts[1], parts[2]};
        }
    else if (name_cmd == "login")
        if (parts.size() != 2)
            error("Uso: login <apelido>");
        else
        {
            cmd.type = CommandType::Login;
            cmd.args = {parts[1]};
        }
    else if (name_cmd == "list")
        cmd.type = CommandType::List;
    else if (name_cmd == "msg")
        if (parts.size() < 3)
            error("Uso: msg <apelido_dest> <mensagem>");
        else
        {
            cmd.type = CommandType::Msg;
            string msg_text = parts[2];
            for (size_t i = 3; i < parts.size(); ++i)
                msg_text += " " + parts[i];
            cmd.args = {parts[1], msg_text};
        }
    else if (name_cmd == "logout")
        cmd.type = CommandType::Logout;
    else if (name_cmd == "delete")
        if (parts.size() != 2)
            error("Uso: delete <apelido>");
        else
        {
            cmd.type = CommandType::Delete;
            cmd.args = {parts[1]};
        }
    else if (name_cmd == "quit")
        cmd.type = CommandType::Quit;
    else
        error("Comando desconhecido. Digite 'help' para ver os comandos.");

    return cmd;
}

void Interface::displayMessage(const json& msg)
{
    try
    {
        string type = msg.value("type", "UNKNOWN");

        if (type == "LOGIN_OK" || type == "OK")
            cout << Colors::GREEN << "[OK] " << Colors::RESET 
                 << "Comando executado com sucesso." << endl;
        else if (type == "ERROR")
        {
            string err_msg = msg["payload"].value("message", "Erro desconhecido");
            cout << Colors::RED << "[Erro] " << Colors::RESET << err_msg << endl;
        }
        else if (type == "DELIVER_MSG")
        {
            string from = msg.value("from", "Desconhecido");
            string text = msg["payload"].value("text", "");
            cout << Colors::BLUE << "[" << from << "] " << Colors::RESET 
                 << text << endl;
        }
        else if (type == "USERS")
        {
            cout << "\n--- LISTA DE USUÁRIOS ---" << endl;
            if (msg["payload"].contains("users"))
                for (const auto& user : msg["payload"]["users"])
                {
                    string status = user.value("online", false) 
                        ? Colors::GREEN + "ONLINE" + Colors::RESET
                        : Colors::RED + "OFFLINE" + Colors::RESET;
                    
                    cout << " " << Colors::YELLOW << user.value("nick", "") 
                         << Colors::RESET << " (" << user.value("name", "") 
                         << "): " << status << endl;
                }
            cout << "-------------------------" << endl;
        }
        else
            cout << Colors::GRAY << "[DEBUG] " << Colors::RESET 
                 << "Mensagem não reconhecida: " << msg.dump() << endl;
    }
    catch (const exception& e)
    {
        error(string("Erro ao processar mensagem: ") + e.what());
    }
}

void Interface::run(Client& client)
{
    cout << "\nBem-vindo ao Mensageiro Rudimentar!" << endl;
    cout << "Digite 'help' para ver os comandos disponíveis.\n" << endl;

    // Conecta ao servidor
    if (!client.connectToServer("127.0.0.1", 12345))
    {
        error("Falha ao conectar ao servidor.");
        return;
    }

    // Inicia thread de recepção de rede (Socket -> Queue)
    client.startReceiverThread();

    // Thread de impressão
    atomic<bool> running{true};
    thread printerThread([&]()
    {
        while (running && client.isConnected())
        {
            auto msg_str = client.popReceivedMessage();
            if (msg_str)
            {
                try
                {
                    cout << Colors::CLEAR_LINE << flush;
                    
                    json msg = json::parse(*msg_str);
                    displayMessage(msg);
                    
                    prompt(); 
                }
                catch (const json::parse_error&)
                {
                    cout << Colors::CLEAR_LINE << flush;
                    error("Mensagem JSON inválida recebida.");
                    prompt();
                }
            }
            this_thread::sleep_for(chrono::milliseconds(10));
        }
    });
    // ====================================================================

    string line;

    // Loop principal (Apenas para Input e Envio)
    while (true)
    {
        if (line.empty()) prompt();

        if (!getline(cin, line)) break;

        if (line.empty()) continue;

        // Comando "help" não envia ao servidor
        string line_lower = line;
        transform(line_lower.begin(), line_lower.end(), line_lower.begin(), ::tolower);
        if (line_lower == "help")
        {
            help();
            continue;
        }

        Command cmd = parse(line);
        if (cmd.type == CommandType::Unknown) continue;

        if (cmd.type == CommandType::Quit)
        {
            running = false;
            
            client.sendJson(Protocol::buildLogoutRequest().dump());
            this_thread::sleep_for(chrono::milliseconds(100));
            
            cout << "Encerrando cliente..." << endl;
            client.disconnect();
            break;
        }

        // Constrói e envia mensagem JSON
        json request;
        try
        {
            bool shouldSend = true;
            switch (cmd.type)
            {
                case CommandType::Register:
                    request = Protocol::buildRegisterRequest(cmd.args[0], cmd.args[1]);
                    break;
                case CommandType::Login:
                    request = Protocol::buildLoginRequest(cmd.args[0]);
                    break;
                case CommandType::List:
                    request = Protocol::buildListUsersRequest();
                    break;
                case CommandType::Msg:
                    request = Protocol::buildSendMessageRequest(cmd.args[0], cmd.args[1]);
                    break;
                case CommandType::Logout:
                    request = Protocol::buildLogoutRequest();
                    break;
                case CommandType::Delete:
                    request = Protocol::buildDeleteUserRequest(cmd.args[0]);
                    break;
                default:
                    shouldSend = false;
                    break;
            }

            if (shouldSend)
                if (!client.sendJson(request.dump()))
                    error("Falha ao enviar mensagem ao servidor.");
        }
        catch (const exception& e)
        {
            error(string("Erro ao processar comando: ") + e.what());
        }
    }

    running = false;
    if (printerThread.joinable())
        printerThread.join();

    client.disconnect();
}