#pragma once

#include "instance_reader.h"
#include "solution.h"

#include <cstdint>
#include <set>
#include <span>
#include <string>
#include <vector>

/**
 * @brief Structured outcome of a validation attempt.
 *
 * `feasible` is the single aggregate verdict. `failures` carries a
 * human-readable list of every reason feasibility was lost (out-of-range
 * indices, capacity violations, conflict pairs). The list is empty when
 * the solution is feasible.
 */
struct ValidationReport
{
    bool feasible{true};
    std::int64_t total_profit{0};
    std::int64_t total_weight{0};
    std::int64_t capacity{0};
    std::size_t invalid_index_count{0};
    std::size_t conflict_pair_count{0};
    bool capacity_violated{false};
    std::vector<std::string> failures{};
};

/**
 * @brief Validates solutions for the DCKP problem.
 *
 * The validator treats any item index outside [0, n-1] as a hard failure
 * with an explicit reason — it never silently ignores invalid indices.
 */
class Validator
{
public:
    using ItemId = DCKPInstance::ItemId;

    explicit Validator(const DCKPInstance &inst) noexcept;

    /**
     * @brief Validates @p solution and updates its feasibility flag.
     * @return true when feasible.
     */
    bool validate(Solution &solution) const;

    /**
     * @brief Builds a full ValidationReport for @p solution.
     */
    [[nodiscard]] ValidationReport analyze(const Solution &solution) const;

    /**
     * @brief Builds a full ValidationReport for a raw set of items. Useful
     * for tests and for validating solutions loaded from external sources.
     */
    [[nodiscard]] ValidationReport analyze(std::span<const ItemId> items) const;
    [[nodiscard]] ValidationReport analyze(const std::set<ItemId> &items) const;

    /**
     * @brief Human-readable, multi-line detailed report (includes every
     * failure reason).
     */
    [[nodiscard]] std::string validateDetailed(const Solution &solution) const;

    /**
     * @brief Incremental feasibility helpers for construction algorithms.
     */
    [[nodiscard]] bool checkCapacity(std::int64_t current_weight, std::int64_t item_weight) const noexcept;
    [[nodiscard]] bool checkConflicts(ItemId item, const std::set<ItemId> &selected_items) const noexcept;

private:
    const DCKPInstance &instance_;
};
