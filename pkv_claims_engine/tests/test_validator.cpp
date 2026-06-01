#include <gtest/gtest.h>
#include "ClaimValidator.h"

static Policy make_policy(const std::string& start, const std::string& end) {
    Policy p;
    p.id           = 1;
    p.start_date   = start;
    p.end_date     = end;
    p.annual_limit = 5000.0;
    Coverage c; c.id = 1; c.policy_id = 1; c.category = "hospital"; c.rate = 0.8;
    p.coverages.push_back(c);
    return p;
}

static Claim make_claim(const std::string& date, const std::string& cat, double cost) {
    Claim c;
    c.policy_id      = 1;
    c.treatment_date = date;
    c.category       = cat;
    c.cost           = cost;
    return c;
}

TEST(ClaimValidator, ValidClaim) {
    ClaimValidator v;
    EXPECT_EQ(v.validate(make_policy("2024-01-01", "2024-12-31"),
                         make_claim("2024-06-15", "hospital", 1000.0)),
              ValidationResult::Valid);
}

TEST(ClaimValidator, TreatmentBeforePolicyStart) {
    ClaimValidator v;
    EXPECT_EQ(v.validate(make_policy("2024-01-01", "2024-12-31"),
                         make_claim("2023-12-31", "hospital", 500.0)),
              ValidationResult::PolicyExpired);
}

TEST(ClaimValidator, TreatmentAfterPolicyEnd) {
    ClaimValidator v;
    EXPECT_EQ(v.validate(make_policy("2024-01-01", "2024-12-31"),
                         make_claim("2025-01-01", "hospital", 500.0)),
              ValidationResult::PolicyExpired);
}

TEST(ClaimValidator, TreatmentOnStartDate) {
    ClaimValidator v;
    EXPECT_EQ(v.validate(make_policy("2024-03-01", "2024-12-31"),
                         make_claim("2024-03-01", "hospital", 200.0)),
              ValidationResult::Valid);
}

TEST(ClaimValidator, TreatmentOnEndDate) {
    ClaimValidator v;
    EXPECT_EQ(v.validate(make_policy("2024-01-01", "2024-06-30"),
                         make_claim("2024-06-30", "hospital", 200.0)),
              ValidationResult::Valid);
}

TEST(ClaimValidator, CategoryNotCovered) {
    ClaimValidator v;
    EXPECT_EQ(v.validate(make_policy("2024-01-01", "2024-12-31"),
                         make_claim("2024-06-15", "dental", 300.0)),
              ValidationResult::CategoryNotCovered);
}

TEST(ClaimValidator, ZeroCost) {
    ClaimValidator v;
    EXPECT_EQ(v.validate(make_policy("2024-01-01", "2024-12-31"),
                         make_claim("2024-06-15", "hospital", 0.0)),
              ValidationResult::InvalidCost);
}

TEST(ClaimValidator, NegativeCost) {
    ClaimValidator v;
    EXPECT_EQ(v.validate(make_policy("2024-01-01", "2024-12-31"),
                         make_claim("2024-06-15", "hospital", -50.0)),
              ValidationResult::InvalidCost);
}
