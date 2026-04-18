#include "validator.h"

#include <cstdint>
#include <sstream>
#include <vector>

Validator::Validator(const DCKPInstance &inst) noexcept
    : instance_(inst)
{
}

void Validator::recalculateMetrics(Solution &solution) const noexcept
{
    const auto &profits = instance_.profits();
    const auto &weights = instance_.weights();
    const int n = instance_.n_items();

    int profit_sum = 0;
    int weight_sum = 0;

    for (const int item : solution.selected_items)
    {
        if (item >= 0 && item < n)
        {
            profit_sum += profits[static_cast<std::size_t>(item)];
            weight_sum += weights[static_cast<std::size_t>(item)];
        }
    }

    solution.total_profit = profit_sum;
    solution.total_weight = weight_sum;
}

bool Validator::checkCapacity(int current_weight, int item_weight) const noexcept
{
    const std::int64_t combined =
        static_cast<std::int64_t>(current_weight) + static_cast<std::int64_t>(item_weight);
    return combined <= static_cast<std::int64_t>(instance_.capacity());
}

bool Validator::checkConflicts(int item, const std::set<int> &selected_items) const noexcept
{
    if (item < 0 || item >= instance_.n_items())
    {
        return false;
    }

    for (const int selected : selected_items)
    {
        if (instance_.has_conflict(item, selected))
        {
            return false;
        }
    }

    return true;
}

bool Validator::validate(Solution &solution) const
{
    bool valid = true;

    recalculateMetrics(solution);

    if (solution.total_weight > instance_.capacity())
    {
        valid = false;
    }

    std::vector<int> items(solution.selected_items.begin(), solution.selected_items.end());
    const auto n = items.size();

    for (std::size_t i = 0; i < n; ++i)
    {
        for (std::size_t j = i + 1; j < n; ++j)
        {
            if (instance_.has_conflict(items[i], items[j]))
            {
                valid = false;
            }
        }
    }

    solution.is_feasible = valid;
    return valid;
}

std::string Validator::validateDetailed(const Solution &solution) const
{
    std::ostringstream ss;
    const int n = instance_.n_items();

    int profit_sum = 0;
    int weight_sum = 0;

    for (const int item : solution.selected_items)
    {
        if (item >= 0 && item < n)
        {
            profit_sum += instance_.profits()[static_cast<std::size_t>(item)];
            weight_sum += instance_.weights()[static_cast<std::size_t>(item)];
        }
    }

    ss << "Itens: " << solution.selected_items.size()
       << ", Peso: " << weight_sum << '/' << instance_.capacity()
       << ", Lucro: " << profit_sum;

    const bool capacity_ok = weight_sum <= instance_.capacity();
    ss << " | Capacidade: " << (capacity_ok ? "OK" : "VIOLADA");

    int conflict_count = 0;
    std::vector<int> items(solution.selected_items.begin(), solution.selected_items.end());
    const auto item_count = items.size();

    for (std::size_t i = 0; i < item_count; ++i)
    {
        for (std::size_t j = i + 1; j < item_count; ++j)
        {
            if (instance_.has_conflict(items[i], items[j]))
            {
                ++conflict_count;
            }
        }
    }

    ss << " | Conflitos: " << conflict_count;
    ss << " | " << ((capacity_ok && conflict_count == 0) ? "VIAVEL" : "INVIAVEL");

    return ss.str();
}
