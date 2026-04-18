#pragma once

#include "instance_reader.h"
#include "solution.h"

#include <set>
#include <string>

/**
 * @brief Validates solutions for the DCKP problem.
 *
 * Checks:
 * 1. Capacity constraint: total weight <= capacity
 * 2. Conflict constraints: no pair of conflicting items selected
 */
class Validator
{
public:
    /**
     * @brief Constructs a validator bound to a problem instance.
     * @param inst Reference to the DCKP instance (must outlive this object)
     */
    explicit Validator(const DCKPInstance &inst) noexcept;

    /**
     * @brief Validates a complete solution, updating its feasibility and metrics.
     * @param solution Solution to validate (modified: recalculates metrics and sets is_feasible)
     * @return true if the solution is feasible, false otherwise
     */
    bool validate(Solution &solution) const;

    /**
     * @brief Checks whether adding an item would violate the capacity constraint.
     * @param current_weight Current total weight of the solution
     * @param item_weight Weight of the item to be added
     * @return true if it does NOT violate capacity, false otherwise
     */
    [[nodiscard]] bool checkCapacity(int current_weight, int item_weight) const noexcept;

    /**
     * @brief Checks whether an item conflicts with any already-selected item.
     * @param item Index of the item to check (0-based)
     * @param selected_items Set of currently selected items
     * @return true if there are NO conflicts, false otherwise
     */
    [[nodiscard]] bool checkConflicts(int item, const std::set<int> &selected_items) const noexcept;

    /**
     * @brief Validates and returns a detailed human-readable report.
     * @param solution Solution to validate (read-only)
     * @return String with validation details
     */
    [[nodiscard]] std::string validateDetailed(const Solution &solution) const;

    /**
     * @brief Recalculates total profit and total weight from selected items.
     * @param solution Solution whose metrics will be recalculated (modified)
     */
    void recalculateMetrics(Solution &solution) const noexcept;

private:
    const DCKPInstance &instance_;
};
