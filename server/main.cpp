#include "server.hpp"
#include <cstring>
#include <iostream>

const int DEFAULT_PORT = 12345;

int main(int argc, char* argv[])
{
    try
    {
        int port = DEFAULT_PORT;
        
        // Argumento: ./server [porta]
        if (argc > 1)
            port = std::stoi(argv[1]);
        
        std::cout << "Iniciando SERVIDOR na porta TCP: " << port << std::endl;
        Server server(port);
        server.run(); // Bloqueia aqui
        
    }
    catch (const std::exception& e)
    {
        std::cerr << "Erro fatal: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}