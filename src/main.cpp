#include "client.hpp"
#include "interface.hpp"

int main() {
    Client client;
    Interface interface;
    interface.run(client);
    return 0;
}
