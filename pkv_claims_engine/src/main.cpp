#include "ClaimsServer.h"
#include <cstdlib>
#include <iostream>

int main() {
    const char* conninfo = std::getenv("PGCONNINFO");
    if (!conninfo) {
        std::cerr << "PGCONNINFO environment variable not set\n";
        return 1;
    }
    const char* port_str = std::getenv("PORT");
    const int   port     = port_str ? std::stoi(port_str) : 8080;

    std::cout << "PKV Claims Engine listening on port " << port << '\n';
    ClaimsServer server{conninfo};
    server.start("0.0.0.0", port);
}
