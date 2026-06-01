#pragma once
#include "Claim.h"
#include <optional>
#include <vector>
#include <pqxx/pqxx>

class ClaimRepository {
public:
    explicit ClaimRepository(pqxx::connection& conn);

    [[nodiscard]] int                  insert(const Claim& claim);
    [[nodiscard]] std::optional<Claim> findById(int id);
    [[nodiscard]] std::vector<Claim>   findByPolicyId(int policy_id);
    [[nodiscard]] double               sumApprovedByPolicyId(int policy_id);

private:
    pqxx::connection& conn_;
    static Claim rowToClaim(const pqxx::row& row);
};
