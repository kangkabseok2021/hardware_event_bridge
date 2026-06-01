#pragma once
#include "Coverage.h"
#include <string>
#include <vector>

struct Policy {
    int                   id            = 0;
    int                   subscriber_id = 0;
    std::string           policy_number;
    std::string           start_date;   // "YYYY-MM-DD"
    std::string           end_date;     // "YYYY-MM-DD"
    double                annual_limit  = 0.0;
    std::vector<Coverage> coverages;
};
