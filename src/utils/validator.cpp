#include "validator.h"

#include <algorithm>
#include <sstream>
#include <vector>

Validator::Validator(const DCKPInstance &inst) noexcept
    : instance_(inst)
{
}

bool Validator::checkCapacity(std::int64_t current_weight, std::int64_t item_weight) const noexcept
{
    return (current_weight + item_weight) <= instance_.capacity();
}

bool Validator::checkConflicts(ItemId item, const std::set<ItemId> &selected_items) const noexcept
{
    if (!instance_.is_valid_item(item))
    {
        return false;
    }
    for (const ItemId selected : selected_items)
    {
        if (instance_.has_conflict(item, selected))
        {
            return false;
        }
    }
    return true;
}

ValidationReport Validator::analyze(std::span<const ItemId> items) const
{
    ValidationReport report;
    report.capacity = instance_.capacity();

    std::vector<ItemId> valid_items;
    valid_items.reserve(items.size());

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

    std::sort(valid_items.begin(), valid_items.end());
    valid_items.erase(std::unique(valid_items.begin(), valid_items.end()), valid_items.end());

    for (std::size_t i = 0; i < valid_items.size(); ++i)
    {
        for (std::size_t j = i + 1; j < valid_items.size(); ++j)
        {
            if (instance_.has_conflict(valid_items[i], valid_items[j]))
            {
                ++report.conflict_pair_count;
                report.feasible = false;
                std::ostringstream msg;
                msg << "Conflict between items " << valid_items[i]
                    << " and " << valid_items[j] << '.';
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
