#pragma once
#include <string>

struct Coverage {
    int         id        = 0;
    int         policy_id = 0;
    std::string category;
    double      rate      = 0.0;
};
