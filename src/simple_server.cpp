#include "simple_net.hpp"

int main() {
    CustomServer server(60000);
    server.Start();

    while (true) {
        server.Update(10, true);
    }
    server.DisconnectAllClients();
    server.Stop();
    return 0;
}