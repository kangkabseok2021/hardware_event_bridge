#include <gtest/gtest.h>
#include <pqxx/pqxx>
#include <cstdlib>
#include "PolicyRepository.h"
#include "ClaimRepository.h"
#include "ClaimValidator.h"
#include "ReimbursementCalculator.h"

static std::string get_conninfo() {
    const char* ci = std::getenv("PGCONNINFO");
    return ci ? ci : "";
}

class DbTest : public ::testing::Test {
protected:
    std::unique_ptr<pqxx::connection> conn;
    int policy_id = 0;

    void SetUp() override {
        const std::string ci = get_conninfo();
        if (ci.empty()) GTEST_SKIP() << "PGCONNINFO not set — skipping DB integration tests";

        conn = std::make_unique<pqxx::connection>(ci);
        pqxx::work txn{*conn};
        txn.exec("TRUNCATE claims, coverages, policies, subscribers RESTART IDENTITY CASCADE");
        txn.exec("INSERT INTO subscribers (name, email) VALUES ('Test User', 'test@example.com')");
        const auto row = txn.exec(
            "INSERT INTO policies (subscriber_id, policy_number, start_date, end_date, annual_limit) "
            "VALUES (1, 'PKV-0001', '2024-01-01', '2024-12-31', 5000.00) RETURNING id").one_row();
        policy_id = row[0].as<int>();
        txn.exec("INSERT INTO coverages (policy_id, category, rate) VALUES ($1, 'hospital', 0.80)",
                 pqxx::params{policy_id});
        txn.exec("INSERT INTO coverages (policy_id, category, rate) VALUES ($1, 'dental', 0.60)",
                 pqxx::params{policy_id});
        txn.commit();
    }
};

TEST_F(DbTest, SubmitAndRetrieveClaim) {
    ClaimRepository repo{*conn};
    Claim c;
    c.policy_id      = policy_id;
    c.treatment_date = "2024-06-15";
    c.category       = "hospital";
    c.cost           = 1000.0;
    c.status         = "approved";
    c.reimbursement  = 800.0;

    const int id = repo.insert(c);
    EXPECT_GT(id, 0);

    const auto fetched = repo.findById(id);
    ASSERT_TRUE(fetched.has_value());
    EXPECT_EQ(fetched->policy_id, policy_id);
    EXPECT_EQ(fetched->category, "hospital");
    EXPECT_NEAR(fetched->reimbursement, 800.0, 1e-6);
}

TEST_F(DbTest, ListClaimsByPolicy) {
    ClaimRepository repo{*conn};
    for (int i = 0; i < 3; ++i) {
        Claim c;
        c.policy_id      = policy_id;
        c.treatment_date = "2024-0" + std::to_string(i + 3) + "-10";
        c.category       = "hospital";
        c.cost           = 500.0;
        c.status         = "approved";
        c.reimbursement  = 400.0;
        (void)repo.insert(c);
    }
    EXPECT_EQ(repo.findByPolicyId(policy_id).size(), 3u);
}

TEST_F(DbTest, PolicyRepositoryFetchesCoverages) {
    PolicyRepository repo{*conn};
    const auto policy = repo.findById(policy_id);
    ASSERT_TRUE(policy.has_value());
    EXPECT_EQ(policy->policy_number, "PKV-0001");
    EXPECT_EQ(policy->coverages.size(), 2u);
    EXPECT_NEAR(policy->annual_limit, 5000.0, 1e-6);
}

TEST_F(DbTest, SumApprovedAccumulatesCorrectly) {
    ClaimRepository repo{*conn};
    for (int i = 0; i < 2; ++i) {
        Claim c;
        c.policy_id      = policy_id;
        c.treatment_date = "2024-0" + std::to_string(i + 3) + "-01";
        c.category       = "hospital";
        c.cost           = 500.0;
        c.status         = "approved";
        c.reimbursement  = 400.0;
        (void)repo.insert(c);
    }
    EXPECT_NEAR(repo.sumApprovedByPolicyId(policy_id), 800.0, 1e-6);
}

TEST_F(DbTest, AnnualLimitCapEnforcedAcrossClaims) {
    ClaimRepository claim_repo{*conn};
    PolicyRepository policy_repo{*conn};
    ReimbursementCalculator calc;

    const auto policy = policy_repo.findById(policy_id);
    ASSERT_TRUE(policy.has_value());

    // Consume 4500 of 5000 limit (5625 * 0.8 = 4500)
    Claim first;
    first.policy_id      = policy_id;
    first.treatment_date = "2024-03-01";
    first.category       = "hospital";
    first.cost           = 5625.0;
    first.status         = "approved";
    first.reimbursement  = 4500.0;
    (void)claim_repo.insert(first);

    // Second claim: 1000 * 0.8 = 800, but only 500 remaining
    Claim second;
    second.policy_id      = policy_id;
    second.treatment_date = "2024-06-15";
    second.category       = "hospital";
    second.cost           = 1000.0;

    const double prior  = claim_repo.sumApprovedByPolicyId(policy_id);
    const auto   result = calc.calculate(*policy, second, prior);
    EXPECT_NEAR(result.reimbursement, 500.0, 1e-6);
}

TEST_F(DbTest, MissingPolicyReturnsNullopt) {
    PolicyRepository repo{*conn};
    EXPECT_FALSE(repo.findById(9999).has_value());
}
