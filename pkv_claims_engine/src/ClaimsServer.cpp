#include "ClaimsServer.h"
#include "ClaimValidator.h"
#include "ReimbursementCalculator.h"
#include "PolicyRepository.h"
#include "ClaimRepository.h"
#include <json.hpp>
#include <pqxx/pqxx>
#include <stdexcept>

using json = nlohmann::json;

static json claim_to_json(const Claim& c) {
    return {{"id", c.id}, {"policy_id", c.policy_id},
            {"treatment_date", c.treatment_date}, {"category", c.category},
            {"cost", c.cost}, {"deductible", c.deductible},
            {"status", c.status}, {"reimbursement", c.reimbursement}};
}

ClaimsServer::ClaimsServer(std::string conninfo)
    : conninfo_(std::move(conninfo)) {
    setupRoutes();
}

void ClaimsServer::setupRoutes() {
    // POST /api/v1/claims
    svr_.Post("/api/v1/claims", [this](const httplib::Request& req,
                                        httplib::Response&      res) {
        try {
            pqxx::connection conn{conninfo_};
            PolicyRepository policy_repo{conn};
            ClaimRepository  claim_repo{conn};

            const auto body = json::parse(req.body);
            Claim claim;
            claim.policy_id      = body.at("policy_id").get<int>();
            claim.treatment_date = body.at("treatment_date").get<std::string>();
            claim.category       = body.at("category").get<std::string>();
            claim.cost           = body.at("cost").get<double>();
            claim.deductible     = body.value("deductible", 0.0);

            const auto policy_opt = policy_repo.findById(claim.policy_id);
            if (!policy_opt) {
                res.status = 404;
                res.set_content(R"({"error":"policy not found"})", "application/json");
                return;
            }

            ClaimValidator validator;
            const auto vr = validator.validate(*policy_opt, claim);

            if (vr != ValidationResult::Valid) {
                claim.status        = "rejected";
                claim.reimbursement = 0.0;
                const int id = claim_repo.insert(claim);
                res.status   = 422;
                res.set_content(json{{"id", id}, {"status", "rejected"}}.dump(),
                                "application/json");
                return;
            }

            const double prior = claim_repo.sumApprovedByPolicyId(claim.policy_id);
            ReimbursementCalculator calc;
            const auto cr       = calc.calculate(*policy_opt, claim, prior);
            claim.status        = "approved";
            claim.reimbursement = cr.reimbursement;

            const int id = claim_repo.insert(claim);
            res.status   = 201;
            res.set_content(
                json{{"id", id}, {"status", "approved"},
                     {"reimbursement", claim.reimbursement}}.dump(),
                "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });

    // GET /api/v1/claims/:id
    svr_.Get(R"(/api/v1/claims/(\d+))",
             [this](const httplib::Request& req, httplib::Response& res) {
        try {
            pqxx::connection conn{conninfo_};
            ClaimRepository repo{conn};
            const int id = std::stoi(req.matches[1]);
            const auto c = repo.findById(id);
            if (!c) {
                res.status = 404;
                res.set_content(R"({"error":"not found"})", "application/json");
                return;
            }
            res.set_content(claim_to_json(*c).dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });

    // GET /api/v1/policies/:id/claims
    svr_.Get(R"(/api/v1/policies/(\d+)/claims)",
             [this](const httplib::Request& req, httplib::Response& res) {
        try {
            pqxx::connection conn{conninfo_};
            ClaimRepository repo{conn};
            const int policy_id = std::stoi(req.matches[1]);
            const auto claims   = repo.findByPolicyId(policy_id);
            json arr            = json::array();
            for (const auto& c : claims) arr.push_back(claim_to_json(c));
            res.set_content(arr.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });
}

void ClaimsServer::start(const std::string& host, int port) {
    svr_.listen(host, port);
}
