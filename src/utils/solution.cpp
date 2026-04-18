#include "solution.h"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <utility>

Solution::Solution(const DCKPInstance &instance) noexcept
    : instance_(&instance)
{
}

bool Solution::addItem(ItemId item) noexcept
{
    if (!instance_->is_valid_item(item))
    {
        return false;
    }
    const auto [it, inserted] = selected_items_.insert(item);
    if (!inserted)
    {
        return false;
    }
    const std::size_t idx = static_cast<std::size_t>(item);
    total_profit_ += static_cast<TotalProfit>(instance_->profits()[idx]);
    total_weight_ += static_cast<TotalWeight>(instance_->weights()[idx]);
    return true;
}

bool Solution::removeItem(ItemId item) noexcept
{
    const auto erased = selected_items_.erase(item);
    if (erased == 0)
    {
        return false;
    }
    if (instance_->is_valid_item(item))
    {
        const std::size_t idx = static_cast<std::size_t>(item);
        total_profit_ -= static_cast<TotalProfit>(instance_->profits()[idx]);
        total_weight_ -= static_cast<TotalWeight>(instance_->weights()[idx]);
    }
    else
    {
        // Defensive: if an invalid id was somehow present, recompute from scratch.
        recomputeTotals();
    }
    return true;
}

bool Solution::hasItem(ItemId item) const noexcept
{
    return selected_items_.contains(item);
}

std::size_t Solution::size() const noexcept
{
    return selected_items_.size();
}

bool Solution::empty() const noexcept
{
    return selected_items_.empty();
}

void Solution::clear() noexcept
{
    selected_items_.clear();
    total_profit_ = 0;
    total_weight_ = 0;
}

void Solution::recomputeTotals() noexcept
{
    TotalProfit profit_sum = 0;
    TotalWeight weight_sum = 0;
    const auto &profits = instance_->profits();
    const auto &weights = instance_->weights();

    for (const ItemId item : selected_items_)
    {
        if (instance_->is_valid_item(item))
        {
            const std::size_t idx = static_cast<std::size_t>(item);
            profit_sum += static_cast<TotalProfit>(profits[idx]);
            weight_sum += static_cast<TotalWeight>(weights[idx]);
        }
    }
    total_profit_ = profit_sum;
    total_weight_ = weight_sum;
}

const std::set<Solution::ItemId> &Solution::selectedItems() const noexcept
{
    return selected_items_;
}

Solution::TotalProfit Solution::totalProfit() const noexcept
{
    return total_profit_;
}

Solution::TotalWeight Solution::totalWeight() const noexcept
{
    return total_weight_;
}

bool Solution::isFeasible() const noexcept
{
    return is_feasible_;
}

Solution::Seconds Solution::computationTime() const noexcept
{
    return computation_time_;
}

std::string_view Solution::methodName() const noexcept
{
    return method_name_;
}

const DCKPInstance &Solution::instance() const noexcept
{
    return *instance_;
}

void Solution::setFeasible(bool feasible) noexcept
{
    is_feasible_ = feasible;
}

void Solution::setComputationTime(Seconds seconds) noexcept
{
    computation_time_ = seconds;
}

void Solution::setMethodName(std::string name)
{
    method_name_ = std::move(name);
}

std::string Solution::toString() const
{
    std::ostringstream ss;
    ss << '[' << method_name_ << "] "
       << "Lucro=" << total_profit_
       << ", Peso=" << total_weight_
       << ", Itens=" << selected_items_.size()
       << ", " << (is_feasible_ ? "Viavel" : "Inviavel")
       << ", " << std::fixed << std::setprecision(4) << computation_time_ << 's';
    return ss.str();
}

void Solution::print() const
{
    std::cout << toString() << '\n';
}

bool Solution::saveToFile(const std::filesystem::path &path) const
{
    std::ofstream file(path);
    if (!file.is_open())
    {
        return false;
    }
    file << total_profit_ << ' ' << total_weight_ << ' ' << selected_items_.size() << '\n';
    for (const ItemId item : selected_items_)
    {
        file << (item + 1) << ' ';
    }
    file << '\n';
    return static_cast<bool>(file);
}

bool operator==(const Solution &a, const Solution &b) noexcept
{
    return a.selected_items_ == b.selected_items_;
}

std::strong_ordering operator<=>(const Solution &a, const Solution &b) noexcept
{
    // Deterministic, structural total ordering. Not profit-only.
    // Because totals are derived from selected_items_, same items implies
    // same totals, so this ordering is consistent with operator==.
    if (auto cmp = a.total_profit_ <=> b.total_profit_; cmp != 0)
    {
        return cmp;
    }
    if (auto cmp = b.total_weight_ <=> a.total_weight_; cmp != 0)
    {
        // Lighter solutions order higher among equal-profit ties.
        return cmp;
    }
    if (a.selected_items_ < b.selected_items_)
    {
        return std::strong_ordering::less;
    }
    if (b.selected_items_ < a.selected_items_)
    {
        return std::strong_ordering::greater;
    }
    return std::strong_ordering::equal;
}
