#include "interface.hpp"
#include "client.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
using namespace std;

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
      → Lista todos os usuários e seus status (online/offline).

  msg <destinatário> <texto...>
      → Envia uma mensagem privada ao usuário especificado.

  logout
      → Faz logout da sessão atual.

  delete <apelido>
      → Remove a conta do apelido (deve estar deslogado).

  quit
      → Sai do programa.

Exemplo:
  register Eliza "Eliza Silva"
  login Eliza
  msg Daniel Oi, tudo bem?
  list
  logout
  quit
)" << endl;
}

void Interface::prompt()
{
    cout << "\033[1;32m> \033[0m" << flush;
}

void Interface::error(const string& msg)
{
    cerr << "\033[1;31m[Erro]\033[0m " << msg << endl;
}

Command Interface::parse(const string& line)
{
    Command cmd;
    stringstream ss(line);
    string token;

    vector<string> parts;

    /* A função std::quoted() (definida em <iomanip>) 
    serve para ler e escrever strings com aspas corretamente.
    Quando usada com >>, ela diz ao stringstream:
    “Leia o próximo valor, e se ele estiver entre aspas ("..."),
    pegue tudo dentro delas como um único token.”*/

    while (ss >> quoted(token))
        parts.push_back(token);

    if (ss.fail() && !ss.eof())
        error("Erro de formatação: aspas desbalanceadas.");


    if (parts.empty())
        return cmd;

    string name_cmd = parts[0];
    transform(name_cmd.begin(), name_cmd.end(), name_cmd.begin(), ::tolower);

    if (name_cmd == "register")
    {
       if (parts.size() < 3) // colocar tratamento de erro aqui pra caso o usuário coloque sem aspas
            error("Uso: register <apelido> \"<Nome Completo>\"");
        else
        {
            cmd.type = CommandType::Register;
            cmd.args = { parts[1], parts[2] };
        }
    }

    else if (name_cmd == "login")
    {
        if (parts.size() != 2)
            error("Uso: login <apelido>");
        else
        {
            cmd.type = CommandType::Login;
            cmd.args = { parts[1] };
        }
    }

    else if (name_cmd == "list")
        cmd.type = CommandType::List;


    else if (name_cmd == "msg")
    {
        if (parts.size() < 3)
            error("Uso: msg <apelido_dest> <mensagem>");
        else
        {
            cmd.type = CommandType::Msg;
            string msg_text;

            for (size_t i = 2; i < parts.size(); ++i)
            {
                if (i > 2) msg_text += " ";
                msg_text += parts[i];
            }
            cmd.args = { parts[1], msg_text };
        }
    }

    else if (name_cmd == "logout")
        cmd.type = CommandType::Logout;


    else if (name_cmd == "delete")
    {
        if (parts.size() != 2)
            error("Uso: delete <apelido>");
        else
        {
            cmd.type = CommandType::Delete;
            cmd.args = { parts[1] };
        }
    }

    else if (name_cmd == "quit")
        cmd.type = CommandType::Quit;

    else
        error("Comando desconhecido. Digite 'help' para ver os comandos.");

    return cmd;
}

// arrumar a chamada dessa função
void Interface::run(Client& client)
{
    cout << "\nBem-vindo ao Mensageiro Rudimentar!" << endl;
    cout << "Digite 'help' para ver os comandos disponíveis." << endl;

    // Conexão inicial
    if (!client.connectToServer("127.0.0.1", 12345))
    {
        error("Falha ao conectar ao servidor.");
        return;
    }

    client.startReceiverThread(); 

    string line;
    
    while (true)
    {
        // Exibe mensagens recebidas (se houver)
        while (auto msg = client.popReceivedMessage())
        {
            json received = json::parse(*msg);
            string type = received.value("type", "UNKNOWN");

            if (type == "LOGIN_OK")
                cout << "Login realizado com sucesso!\n";
            else if (type == "ERROR")
                cerr << "Erro: " << received["payload"]["message"] << endl;
            else if (type == "INCOMING_MSG")
                cout << "\nNova mensagem de " << received["payload"]["from"]
                    << ": " << received["payload"]["text"] << endl;
            else
                cout << "[DEBUG] Mensagem não reconhecida: " << *msg << endl;
        }

        prompt();
        if (!getline(cin, line))
            break;

        if (line.empty())
            continue;

        string line_lower = line;
        transform(line_lower.begin(), line_lower.end(), line_lower.begin(), ::tolower);

        if (line_lower == "help")
        {
            help();
            continue;
        }

        Command cmd = parse(line);
        json j;

        // Define o tipo de comando
        switch (cmd.type)
        {
        case CommandType::Register:
            j = {{"type", "REGISTER"},
                {"payload", {
                    {"nickname", cmd.args[0]},
                    {"fullname", cmd.args[1]}
                }}};
            break;

        case CommandType::Login:
            j = {{"type", "LOGIN"},
                {"payload", {
                    {"nickname", cmd.args[0]}
                }}};
            break;

        case CommandType::List:
            j = {{"type", "LIST_USERS"},
                {"payload", json::object()}};
            break;

        case CommandType::Msg:
            j = {{"type", "SEND_MSG"},
                {"payload", {
                    {"to", cmd.args[0]},
                    {"text", cmd.args[1]}
                }}};
            break;

        case CommandType::Logout:
            j = {{"type", "LOGOUT"},
                {"payload", json::object()}};
            break;

        case CommandType::Delete:
            j = {{"type", "DELETE_USER"},
                {"payload", {
                    {"nickname", cmd.args[0]}
                }}};
            break;

        case CommandType::Quit:
            if (client.sendJson(R"({"type":"LOGOUT","payload":{}})"))
                this_thread::sleep_for(chrono::milliseconds(100));
            cout << "Encerrando cliente..." << endl;
            client.disconnect();
            return;

        default:
            continue;
        }


        if (!client.sendJson(j))
            error("Falha ao enviar mensagem.");
    }

    client.disconnect();
}
