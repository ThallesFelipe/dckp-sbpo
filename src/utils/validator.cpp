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

    // Pass 1: validate ranges, accumulate totals for valid indices only, and
    // record explicit failure reasons for anything out of range.
    for (const ItemId item : items)
    {
        if (!instance_.is_valid_item(item))
        {
            ++report.invalid_index_count;
            report.feasible = false;
            std::ostringstream msg;
            msg << "Item " << item << " fora do intervalo [0, "
                << instance_.n_items() << ").";
            report.failures.push_back(msg.str());
            continue;
        }
        const std::size_t idx = static_cast<std::size_t>(item);
        report.total_profit += static_cast<std::int64_t>(instance_.profits()[idx]);
        report.total_weight += static_cast<std::int64_t>(instance_.weights()[idx]);
        valid_items.push_back(item);
    }

    // Pass 2: capacity.
    if (report.total_weight > instance_.capacity())
    {
        report.feasible = false;
        report.capacity_violated = true;
        std::ostringstream msg;
        msg << "Capacidade excedida: peso total " << report.total_weight
            << " > capacidade " << instance_.capacity() << '.';
        report.failures.push_back(msg.str());
    }

    // Pass 3: conflicts. Using the deduplicated sorted valid_items list.
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
                msg << "Conflito entre itens " << valid_items[i]
                    << " e " << valid_items[j] << '.';
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
    ss << "Itens: " << solution.selectedItems().size()
       << ", Peso: " << report.total_weight << '/' << report.capacity
       << ", Lucro: " << report.total_profit
       << " | Indices invalidos: " << report.invalid_index_count
       << " | Capacidade: " << (report.capacity_violated ? "VIOLADA" : "OK")
       << " | Conflitos: " << report.conflict_pair_count
       << " | " << (report.feasible ? "VIAVEL" : "INVIAVEL");

    if (!report.failures.empty())
    {
        ss << "\nMotivos:";
        for (const std::string &reason : report.failures)
        {
            ss << "\n  - " << reason;
        }
    }
    return ss.str();
}
