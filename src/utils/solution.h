#pragma once

#include <set>
#include <string>

/**
 * @brief Represents a candidate solution for the DCKP problem.
 *
 * Stores the selected items, total profit, total weight,
 * feasibility status, computation time, and method name.
 *
 * Uses std::set for ordered iteration and O(log n) lookups.
 */
class Solution
{
public:
    std::set<int> selected_items;
    int total_profit;
    int total_weight;
    bool is_feasible;
    double computation_time;
    std::string method_name;

    /**
     * @brief Default constructor.
     * @post Empty solution with profit=0, weight=0, is_feasible=true.
     */
    Solution() noexcept;

    /**
     * @brief Adds an item to the solution.
     * @param item Item index (0-based)
     * @param profit Item profit
     * @param weight Item weight
     * @note If the item already exists, the operation is ignored.
     */
    void addItem(int item, int profit, int weight);

    /**
     * @brief Removes an item from the solution.
     * @param item Item index (0-based)
     * @param profit Item profit
     * @param weight Item weight
     * @note If the item does not exist, the operation is ignored.
     */
    void removeItem(int item, int profit, int weight);

    /**
     * @brief Checks whether an item is in the solution.
     * @param item Item index (0-based)
     * @return true if the item is selected, false otherwise
     */
    [[nodiscard]] bool hasItem(int item) const noexcept;

    /**
     * @brief Returns the number of selected items.
     */
    [[nodiscard]] int size() const noexcept;

    /**
     * @brief Checks whether the solution is empty.
     * @return true if no items are selected
     */
    [[nodiscard]] bool empty() const noexcept;

    /**
     * @brief Clears the solution to its default state.
     * @post selected_items empty, total_profit=0, total_weight=0.
     */
    void clear() noexcept;

    /**
     * @brief Converts the solution to a human-readable string.
     */
    [[nodiscard]] std::string toString() const;

    /**
     * @brief Prints solution information to stdout.
     */
    void print() const;

    /**
     * @brief Saves the solution to a file.
     * @param filename Output file path
     * @return true on success, false on failure
     */
    [[nodiscard]] bool saveToFile(const std::string &filename) const;

    /**
     * @brief Compares by profit (greater).
     */
    [[nodiscard]] bool operator>(const Solution &other) const noexcept;

    /**
     * @brief Compares by profit (less).
     */
    [[nodiscard]] bool operator<(const Solution &other) const noexcept;

    /**
     * @brief Equality based on profit.
     */
    [[nodiscard]] bool operator==(const Solution &other) const noexcept;
};
