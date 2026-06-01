#include "ClaimValidator.h"
#include <algorithm>

ValidationResult ClaimValidator::validate(const Policy& policy,
                                           const Claim&  claim) const noexcept {
    // ISO-date YYYY-MM-DD lexicographic comparison is numerically correct
    if (claim.treatment_date < policy.start_date ||
        claim.treatment_date > policy.end_date) {
        return ValidationResult::PolicyExpired;
    }

    const bool covered = std::any_of(
        policy.coverages.begin(), policy.coverages.end(),
        [&](const Coverage& c) { return c.category == claim.category; });
    if (!covered) return ValidationResult::CategoryNotCovered;

    if (claim.cost <= 0.0) return ValidationResult::InvalidCost;

    return ValidationResult::Valid;
}
