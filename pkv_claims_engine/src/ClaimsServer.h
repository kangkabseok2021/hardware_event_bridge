#pragma once
#include <string>
#include <httplib.h>

class ClaimsServer {
public:
    explicit ClaimsServer(std::string conninfo);
    void start(const std::string& host = "0.0.0.0", int port = 8080);

private:
    std::string     conninfo_;
    httplib::Server svr_;

    void setupRoutes();
};
