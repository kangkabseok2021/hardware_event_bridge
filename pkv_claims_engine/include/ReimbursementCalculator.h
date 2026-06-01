#pragma once
#include "Policy.h"
#include "Claim.h"

struct CalcResult {
    double gross;
    double after_deductible;
    double reimbursement;
};

class ReimbursementCalculator {
public:
    [[nodiscard]] CalcResult calculate(const Policy& policy,
                                        const Claim&  claim,
                                        double        prior_approved_total) const;
};
