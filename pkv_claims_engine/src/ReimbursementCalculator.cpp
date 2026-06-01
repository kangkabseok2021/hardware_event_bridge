#include "ReimbursementCalculator.h"
#include <algorithm>

CalcResult ReimbursementCalculator::calculate(const Policy& policy,
                                               const Claim&  claim,
                                               double        prior_approved_total) const {
    double rate = 0.0;
    for (const auto& cov : policy.coverages) {
        if (cov.category == claim.category) { rate = cov.rate; break; }
    }

    const double gross            = claim.cost * rate;
    const double after_deductible = std::max(0.0, gross - claim.deductible);
    const double remaining_limit  = std::max(0.0, policy.annual_limit - prior_approved_total);
    const double reimbursement    = std::min(after_deductible, remaining_limit);

    return {gross, after_deductible, reimbursement};
}
