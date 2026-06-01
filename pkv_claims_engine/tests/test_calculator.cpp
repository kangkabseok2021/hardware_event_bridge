#include <gtest/gtest.h>
#include "ReimbursementCalculator.h"

static Policy make_policy(double annual_limit = 5000.0, double rate = 0.8) {
    Policy p;
    p.id           = 1;
    p.annual_limit = annual_limit;
    Coverage c; c.category = "hospital"; c.rate = rate; c.policy_id = 1;
    p.coverages.push_back(c);
    return p;
}

static Claim make_claim(double cost, double deductible = 0.0) {
    Claim c;
    c.policy_id      = 1;
    c.treatment_date = "2024-06-15";
    c.category       = "hospital";
    c.cost           = cost;
    c.deductible     = deductible;
    return c;
}

TEST(ReimbursementCalculator, BasicReimbursement) {
    ReimbursementCalculator calc;
    auto r = calc.calculate(make_policy(5000.0, 0.8), make_claim(1000.0), 0.0);
    EXPECT_NEAR(r.gross,            800.0, 1e-6);
    EXPECT_NEAR(r.after_deductible, 800.0, 1e-6);
    EXPECT_NEAR(r.reimbursement,    800.0, 1e-6);
}

TEST(ReimbursementCalculator, DeductibleReduces) {
    ReimbursementCalculator calc;
    auto r = calc.calculate(make_policy(5000.0, 0.8), make_claim(1000.0, 100.0), 0.0);
    EXPECT_NEAR(r.gross,            800.0, 1e-6);
    EXPECT_NEAR(r.after_deductible, 700.0, 1e-6);
    EXPECT_NEAR(r.reimbursement,    700.0, 1e-6);
}

TEST(ReimbursementCalculator, AnnualLimitCaps) {
    ReimbursementCalculator calc;
    // gross = 800, limit = 500 → capped at 500
    auto r = calc.calculate(make_policy(500.0, 0.8), make_claim(1000.0), 0.0);
    EXPECT_NEAR(r.reimbursement, 500.0, 1e-6);
}

TEST(ReimbursementCalculator, PriorApprovedReducesLimit) {
    ReimbursementCalculator calc;
    // limit=1000, prior=400 → remaining=600; gross=800 → capped at 600
    auto r = calc.calculate(make_policy(1000.0, 0.8), make_claim(1000.0), 400.0);
    EXPECT_NEAR(r.reimbursement, 600.0, 1e-6);
}

TEST(ReimbursementCalculator, ZeroRemainingLimit) {
    ReimbursementCalculator calc;
    auto r = calc.calculate(make_policy(500.0, 0.8), make_claim(1000.0), 500.0);
    EXPECT_NEAR(r.reimbursement, 0.0, 1e-6);
}

TEST(ReimbursementCalculator, FullCoverageRate) {
    ReimbursementCalculator calc;
    auto r = calc.calculate(make_policy(5000.0, 1.0), make_claim(1200.0), 0.0);
    EXPECT_NEAR(r.gross,         1200.0, 1e-6);
    EXPECT_NEAR(r.reimbursement, 1200.0, 1e-6);
}

TEST(ReimbursementCalculator, DeductibleExceedsGross) {
    ReimbursementCalculator calc;
    // gross=80, deductible=200 → after_ded=0
    auto r = calc.calculate(make_policy(5000.0, 0.8), make_claim(100.0, 200.0), 0.0);
    EXPECT_NEAR(r.gross,            80.0, 1e-6);
    EXPECT_NEAR(r.after_deductible,  0.0, 1e-6);
    EXPECT_NEAR(r.reimbursement,     0.0, 1e-6);
}
