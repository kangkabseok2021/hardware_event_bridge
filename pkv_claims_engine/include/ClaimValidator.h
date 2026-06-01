#pragma once
#include "Policy.h"
#include "Claim.h"

enum class ValidationResult {
    Valid,
    PolicyExpired,
    CategoryNotCovered,
    InvalidCost
};

class ClaimValidator {
public:
    [[nodiscard]] ValidationResult validate(const Policy& policy,
                                             const Claim&  claim) const noexcept;
};
