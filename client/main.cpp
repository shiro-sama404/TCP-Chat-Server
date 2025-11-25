#include "client.hpp"
#include "interface.hpp"
#include <cstring>
#include <iostream>

const int DEFAULT_PORT = 12345;

int main(int argc, char* argv[])
{
    try
    {
        std::string host = "127.0.0.1";
        int port = DEFAULT_PORT;
        
        // Argumentos: ./client [host] [porta]
        if (argc > 1) host = argv[1];
        if (argc > 2) port = std::stoi(argv[2]);
        
        std::cout << "Iniciando CLIENTE" << std::endl;
        std::cout << "Conectando em " << host << ":" << port << std::endl;
        
        Client client;
        Interface interface;
        interface.run(client);
    }
    catch (const std::exception& e)
    {
        std::cerr << "Erro fatal: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}