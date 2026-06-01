#include "PolicyRepository.h"

PolicyRepository::PolicyRepository(pqxx::connection& conn) : conn_(conn) {}

std::optional<Policy> PolicyRepository::findById(int policy_id) {
    pqxx::work txn{conn_};

    const auto rows = txn.exec_params(
        "SELECT id, subscriber_id, policy_number, "
        "       start_date::text, end_date::text, annual_limit::float8 "
        "FROM policies WHERE id = $1",
        policy_id);

    if (rows.empty()) { txn.commit(); return std::nullopt; }

    Policy p;
    p.id            = rows[0][0].as<int>();
    p.subscriber_id = rows[0][1].as<int>();
    p.policy_number = rows[0][2].as<std::string>();
    p.start_date    = rows[0][3].as<std::string>();
    p.end_date      = rows[0][4].as<std::string>();
    p.annual_limit  = rows[0][5].as<double>();

    const auto cov_rows = txn.exec_params(
        "SELECT id, policy_id, category, rate::float8 "
        "FROM coverages WHERE policy_id = $1",
        policy_id);

    p.coverages.reserve(cov_rows.size());
    for (const auto& row : cov_rows) {
        Coverage c;
        c.id        = row[0].as<int>();
        c.policy_id = row[1].as<int>();
        c.category  = row[2].as<std::string>();
        c.rate      = row[3].as<double>();
        p.coverages.push_back(std::move(c));
    }

    txn.commit();
    return p;
}
