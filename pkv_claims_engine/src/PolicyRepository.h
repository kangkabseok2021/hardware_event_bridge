#pragma once
#include "Policy.h"
#include <optional>
#include <pqxx/pqxx>

class PolicyRepository {
public:
    explicit PolicyRepository(pqxx::connection& conn);
    [[nodiscard]] std::optional<Policy> findById(int policy_id);

private:
    pqxx::connection& conn_;
};
