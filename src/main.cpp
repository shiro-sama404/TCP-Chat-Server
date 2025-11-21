#include "client.hpp"
#include "interface.hpp"
#include "server.hpp" // Requer a definição da classe Server
#include <iostream>
#include <cstring>
#include <stdexcept>

using namespace std;

// Porta padrão para a conexão TCP
const int DEFAULT_PORT = 12345; 

int main(int argc, char* argv[]) {
    try {
        int port = DEFAULT_PORT;

        // 1. VERIFICA SE O ARGUMENTO --server FOI PASSADO
        if (argc > 1 && strcmp(argv[1], "--server") == 0) {
            
            // --- MODO SERVIDOR: Configuração de Porta ---
            
            // 2. Tenta ler a porta configurável (opcional)
            if (argc > 2) {
                try {
                    port = stoi(argv[2]);
                } catch (const exception& e) {
                    cerr << "Aviso: Porta inválida \"" << argv[2] << "\". Usando porta padrão: " << DEFAULT_PORT << endl;
                    port = DEFAULT_PORT; // Garante o uso da porta padrão em caso de erro de parsing
                }
            }

            // 3. INICIA O SERVIDOR (Utiliza socket, bind, listen)
            cout << "Iniciando SERVIDOR na porta TCP: " << port << endl;
            Server server(port);
            server.run(); // O loop do servidor bloqueia aqui
            
        } else {
            // --- MODO CLIENTE (Padrão) ---
            
            cout << "Iniciando CLIENTE. Conectando em 127.0.0.1:" << DEFAULT_PORT << endl;
            cout << "(Use ./<executavel> --server [porta] para iniciar o servidor)" << endl;
            
            Client client;
            Interface interface;
            
            // A interface do cliente deve ser modificada para usar a porta DEFAULT_PORT
            // se o argumento não for fornecido.
            interface.run(client); 
        }

    } catch (const exception& e) {
        cerr << "Erro fatal: " << e.what() << endl;
        return 1;
    }
    return 0;
}