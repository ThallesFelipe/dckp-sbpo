#pragma once

#include "instance_reader.h"

#include <compare>
#include <cstdint>
#include <filesystem>
#include <set>
#include <string>
#include <string_view>

/**
 * @brief Candidate solution for the DCKP problem.
 *
 * A Solution is bound to a DCKPInstance at construction and maintains the
 * invariant that total_profit and total_weight always equal the sum of the
 * corresponding fields of its currently selected items. Callers cannot
 * mutate totals directly; they are recomputed on every item mutation.
 *
 * Accumulators use 64-bit signed integers to avoid overflow even on large
 * instances where the sum of int32 profits/weights would not fit in int32.
 */
class Solution
{
public:
    using ItemId = DCKPInstance::ItemId;
    using TotalProfit = std::int64_t;
    using TotalWeight = std::int64_t;
    using Seconds = double;

    /**
     * @brief Constructs an empty solution bound to @p instance.
     * The referenced instance must outlive this object.
     */
    explicit Solution(const DCKPInstance &instance) noexcept;

    Solution(const Solution &) = default;
    Solution(Solution &&) noexcept = default;
    Solution &operator=(const Solution &) = default;
    Solution &operator=(Solution &&) noexcept = default;
    ~Solution() = default;

    /**
     * @brief Inserts @p item if it is a valid index for the bound instance
     * and not already present. Totals are updated from instance data.
     * @return true if the solution changed, false otherwise (invalid item
     * id, or item already present).
     */
    bool addItem(ItemId item) noexcept;

    /**
     * @brief Removes @p item if present. Totals are updated.
     * @return true if the solution changed, false otherwise.
     */
    bool removeItem(ItemId item) noexcept;

    [[nodiscard]] bool hasItem(ItemId item) const noexcept;
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] bool empty() const noexcept;

    /**
     * @brief Clears selected items and resets derived totals. Does not
     * alter feasibility, timing, or method name metadata.
     */
    void clear() noexcept;

    /**
     * @brief Recomputes total_profit and total_weight from the current
     * items and the bound instance. Called automatically by addItem and
     * removeItem, and exposed for defensive use after bulk operations.
     */
    void recomputeTotals() noexcept;

    [[nodiscard]] const std::set<ItemId> &selectedItems() const noexcept;
    [[nodiscard]] TotalProfit totalProfit() const noexcept;
    [[nodiscard]] TotalWeight totalWeight() const noexcept;
    [[nodiscard]] bool isFeasible() const noexcept;
    [[nodiscard]] Seconds computationTime() const noexcept;
    [[nodiscard]] std::string_view methodName() const noexcept;
    [[nodiscard]] const DCKPInstance &instance() const noexcept;

    // Metadata setters: controlled access for algorithms/validator.
    void setFeasible(bool feasible) noexcept;
    void setComputationTime(Seconds seconds) noexcept;
    void setMethodName(std::string name);

    [[nodiscard]] std::string toString() const;
    void print() const;
    [[nodiscard]] bool saveToFile(const std::filesystem::path &path) const;

    // Structural comparison: deterministic, consistent with operator==.
    // Equality requires the same set of selected items (and therefore the
    // same totals because totals are derived). Ordering breaks ties by
    // total_profit, then by total_weight, then by the sorted item set.
    friend bool operator==(const Solution &a, const Solution &b) noexcept;
    friend std::strong_ordering operator<=>(const Solution &a, const Solution &b) noexcept;

private:
    const DCKPInstance *instance_;
    std::set<ItemId> selected_items_{};
    TotalProfit total_profit_{0};
    TotalWeight total_weight_{0};
    bool is_feasible_{true};
    Seconds computation_time_{0.0};
    std::string method_name_{"Unknown"};
};
