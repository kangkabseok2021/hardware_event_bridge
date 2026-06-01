#include "ClaimRepository.h"

ClaimRepository::ClaimRepository(pqxx::connection& conn) : conn_(conn) {}

Claim ClaimRepository::rowToClaim(const pqxx::row& row) {
    Claim c;
    c.id             = row[0].as<int>();
    c.policy_id      = row[1].as<int>();
    c.treatment_date = row[2].as<std::string>();
    c.category       = row[3].as<std::string>();
    c.cost           = row[4].as<double>();
    c.deductible     = row[5].as<double>();
    c.status         = row[6].as<std::string>();
    c.reimbursement  = row[7].is_null() ? 0.0 : row[7].as<double>();
    return c;
}

int ClaimRepository::insert(const Claim& claim) {
    pqxx::work txn{conn_};
    const auto row = txn.exec_params1(
        "INSERT INTO claims "
        "(policy_id, treatment_date, category, cost, deductible, status, reimbursement) "
        "VALUES ($1, $2::date, $3, $4, $5, $6, $7) RETURNING id",
        claim.policy_id, claim.treatment_date, claim.category,
        claim.cost, claim.deductible, claim.status, claim.reimbursement);
    txn.commit();
    return row[0].as<int>();
}

std::optional<Claim> ClaimRepository::findById(int id) {
    pqxx::work txn{conn_};
    const auto rows = txn.exec_params(
        "SELECT id, policy_id, treatment_date::text, category, "
        "       cost::float8, deductible::float8, status, reimbursement::float8 "
        "FROM claims WHERE id = $1",
        id);
    txn.commit();
    if (rows.empty()) return std::nullopt;
    return rowToClaim(rows[0]);
}

std::vector<Claim> ClaimRepository::findByPolicyId(int policy_id) {
    pqxx::work txn{conn_};
    const auto rows = txn.exec_params(
        "SELECT id, policy_id, treatment_date::text, category, "
        "       cost::float8, deductible::float8, status, reimbursement::float8 "
        "FROM claims WHERE policy_id = $1 ORDER BY id",
        policy_id);
    txn.commit();
    std::vector<Claim> result;
    result.reserve(rows.size());
    for (const auto& row : rows) result.push_back(rowToClaim(row));
    return result;
}

double ClaimRepository::sumApprovedByPolicyId(int policy_id) {
    pqxx::work txn{conn_};
    const auto row = txn.exec_params1(
        "SELECT COALESCE(SUM(reimbursement), 0.0)::float8 "
        "FROM claims WHERE policy_id = $1 AND status = 'approved'",
        policy_id);
    txn.commit();
    return row[0].as<double>();
}
