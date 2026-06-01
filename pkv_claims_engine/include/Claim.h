#pragma once
#include <string>

struct Claim {
    int         id             = 0;
    int         policy_id      = 0;
    std::string treatment_date; // "YYYY-MM-DD"
    std::string category;
    double      cost           = 0.0;
    double      deductible     = 0.0;
    std::string status         = "pending";
    double      reimbursement  = 0.0;
};
