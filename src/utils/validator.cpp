#include "validator.h"

#include <cstdint>
#include <sstream>
#include <vector>

Validator::Validator(const DCKPInstance &inst) noexcept
    : instance_(inst)
{
}

ValidationReport Validator::analyze(std::span<const ItemId> items) const
{
    ValidationReport report;
    report.capacity = instance_.capacity();

    std::vector<ItemId> valid_items;
    valid_items.reserve(items.size());
    std::vector<std::uint8_t> selected(
        static_cast<std::size_t>(instance_.n_items()), std::uint8_t{0});

    for (const ItemId item : items)
    {
        if (!instance_.is_valid_item(item))
        {
            ++report.invalid_index_count;
            report.feasible = false;
            std::ostringstream msg;
            msg << "Item " << item << " is out of range [0, "
                << instance_.n_items() << ").";
            report.failures.push_back(msg.str());
            continue;
        }
        const std::size_t idx = static_cast<std::size_t>(item);
        if (selected[idx] != 0)
        {
            ++report.duplicate_item_count;
            report.feasible = false;
            std::ostringstream msg;
            msg << "Item " << item << " is selected more than once.";
            report.failures.push_back(msg.str());
            continue;
        }
        selected[idx] = 1;
        report.total_profit += static_cast<std::int64_t>(instance_.profits()[idx]);
        report.total_weight += static_cast<std::int64_t>(instance_.weights()[idx]);
        valid_items.push_back(item);
    }

    if (report.total_weight > instance_.capacity())
    {
        report.feasible = false;
        report.capacity_violated = true;
        std::ostringstream msg;
        msg << "Capacity exceeded: total weight " << report.total_weight
            << " > capacity " << instance_.capacity() << '.';
        report.failures.push_back(msg.str());
    }

    const auto &conflict_graph = instance_.conflict_graph();
    for (const ItemId item : valid_items)
    {
        for (const ItemId neighbor : conflict_graph[static_cast<std::size_t>(item)])
        {
            if (neighbor > item && selected[static_cast<std::size_t>(neighbor)] != 0)
            {
                ++report.conflict_pair_count;
                report.feasible = false;
                std::ostringstream msg;
                msg << "Conflict between items " << item
                    << " and " << neighbor << '.';
                report.failures.push_back(msg.str());
            }
        }
    }

    return report;
}

ValidationReport Validator::analyze(const std::set<ItemId> &items) const
{
    std::vector<ItemId> buf(items.begin(), items.end());
    return analyze(std::span<const ItemId>{buf.data(), buf.size()});
}

ValidationReport Validator::analyze(const Solution &solution) const
{
    return analyze(solution.selectedItems());
}

bool Validator::validate(Solution &solution) const
{
    const ValidationReport report = analyze(solution);
    solution.setFeasible(report.feasible);
    return report.feasible;
}

std::string Validator::validateDetailed(const Solution &solution) const
{
    const ValidationReport report = analyze(solution);

    std::ostringstream ss;
    ss << "Items: " << solution.selectedItems().size()
       << ", Weight: " << report.total_weight << '/' << report.capacity
       << ", Profit: " << report.total_profit
       << " | Invalid indices: " << report.invalid_index_count
       << " | Duplicates: " << report.duplicate_item_count
       << " | Capacity: " << (report.capacity_violated ? "VIOLATED" : "OK")
       << " | Conflicts: " << report.conflict_pair_count
       << " | " << (report.feasible ? "FEASIBLE" : "INFEASIBLE");

    if (!report.failures.empty())
    {
        ss << "\nReasons:";
        for (const std::string &reason : report.failures)
        {
            ss << "\n  - " << reason;
        }
    }
    return ss.str();
}
