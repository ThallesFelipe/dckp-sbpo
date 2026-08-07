#include "solution.h"

#include <iomanip>
#include <sstream>
#include <utility>

Solution::Solution(const DCKPInstance &instance) noexcept
    : instance_(&instance)
{
}

bool Solution::addItem(ItemId item)
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
    const std::size_t idx = static_cast<std::size_t>(item);
    total_profit_ -= static_cast<TotalProfit>(instance_->profits()[idx]);
    total_weight_ -= static_cast<TotalWeight>(instance_->weights()[idx]);
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
       << "Profit=" << total_profit_
       << ", Weight=" << total_weight_
       << ", Items=" << selected_items_.size()
       << ", " << (is_feasible_ ? "Feasible" : "Infeasible")
       << ", " << std::fixed << std::setprecision(4) << computation_time_ << 's';
    return ss.str();
}

bool operator==(const Solution &a, const Solution &b) noexcept
{
    return a.selected_items_ == b.selected_items_;
}

std::strong_ordering operator<=>(const Solution &a, const Solution &b) noexcept
{
    if (auto cmp = a.total_profit_ <=> b.total_profit_; cmp != std::strong_ordering::equal)
    {
        return cmp;
    }
    if (auto cmp = b.total_weight_ <=> a.total_weight_; cmp != std::strong_ordering::equal)
    {
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
