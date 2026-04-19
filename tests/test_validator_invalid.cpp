#include "test_support.h"
#include "utils/instance_reader.h"
#include "utils/solution.h"
#include "utils/validator.h"

#include <algorithm>
#include <string>
#include <vector>

namespace
{
    DCKPInstance makeInstance()
    {
        const std::string content =
            "5 2 10\n"
            "5 4 3 2 1\n"
            "2 3 1 4 1\n"
            "1 2  3 4\n";
        dckp_test::ScopedTempFile tmp("validator_inv", content);
        DCKPInstance inst;
        if (!inst.read_from_file(tmp.path()))
        {
            throw std::runtime_error(std::string{"parse failed: "} + std::string{inst.last_error()});
        }
        return inst;
    }

    bool containsSubstr(const std::vector<std::string> &messages, std::string_view needle)
    {
        return std::any_of(messages.begin(), messages.end(),
                           [&](const std::string &m)
                           { return m.find(needle) != std::string::npos; });
    }
}

int main()
{
    DCKPInstance inst = makeInstance();
    Validator validator(inst);

    {
        Solution sol(inst);
        DCKP_CHECK(!sol.addItem(-1));
        DCKP_CHECK(!sol.addItem(5));
        DCKP_CHECK(!sol.addItem(999));
        DCKP_CHECK_EQ(sol.size(), 0U);
        DCKP_CHECK_EQ(sol.totalProfit(), 0);
        DCKP_CHECK_EQ(sol.totalWeight(), 0);
    }

    {
        const std::vector<DCKPInstance::ItemId> items{-1, 0, 7};
        const ValidationReport report = validator.analyze(std::span<const DCKPInstance::ItemId>{items.data(), items.size()});
        DCKP_CHECK(!report.feasible);
        DCKP_CHECK_EQ(report.invalid_index_count, 2U);
        DCKP_CHECK(containsSubstr(report.failures, "Item -1"));
        DCKP_CHECK(containsSubstr(report.failures, "Item 7"));
    }

    {
        Solution sol(inst);
        DCKP_CHECK(sol.addItem(0));
        DCKP_CHECK(sol.addItem(3));
        DCKP_CHECK(sol.addItem(2));
        std::vector<DCKPInstance::ItemId> items{0, 1, 2, 3, 4};
        const ValidationReport report = validator.analyze(std::span<const DCKPInstance::ItemId>{items.data(), items.size()});
        DCKP_CHECK(!report.feasible);
        DCKP_CHECK(report.capacity_violated);
        DCKP_CHECK(containsSubstr(report.failures, "Capacity"));
    }

    {
        std::vector<DCKPInstance::ItemId> items{0, 1};
        const ValidationReport report = validator.analyze(std::span<const DCKPInstance::ItemId>{items.data(), items.size()});
        DCKP_CHECK(!report.feasible);
        DCKP_CHECK_EQ(report.conflict_pair_count, 1U);
        DCKP_CHECK(containsSubstr(report.failures, "Conflict"));
    }

    {
        Solution sol(inst);
        DCKP_CHECK(sol.addItem(0));
        DCKP_CHECK(sol.addItem(2));
        DCKP_CHECK(sol.addItem(4));
        const ValidationReport report = validator.analyze(sol);
        DCKP_CHECK(report.feasible);
        DCKP_CHECK(report.failures.empty());
        DCKP_CHECK_EQ(report.invalid_index_count, 0U);
        DCKP_CHECK(!report.capacity_violated);
        DCKP_CHECK_EQ(report.conflict_pair_count, 0U);
        DCKP_CHECK(validator.validate(sol));
        DCKP_CHECK(sol.isFeasible());
    }

    return 0;
}
