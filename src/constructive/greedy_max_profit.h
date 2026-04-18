#pragma once

#include "../algorithms/algorithm.h"

namespace dckp
{
    /**
     * @brief Constructive greedy heuristic that selects items in decreasing
     * order of absolute profit.
     *
     * Complexity: O(N log N) for the sort, plus O(N · D_max) for the
     * insertion phase where D_max is the maximum conflict degree. The
     * conflict check for each candidate uses a forbidden-set bitset that
     * is maintained incrementally, giving O(1) per candidate lookup and
     * O(degree) per accepted item to update the bitset. This is strictly
     * better than the naive O(N · K) scan over selected items.
     *
     * Tie-breaking: when two items have the same profit, the lighter item
     * is preferred (greedy packing heuristic).
     */
    class GreedyMaxProfit : public Algorithm
    {
    public:
        GreedyMaxProfit() = default;
        ~GreedyMaxProfit() override = default;

        [[nodiscard]] Solution run(const DCKPInstance &instance, RunContext &ctx) override;
        [[nodiscard]] std::string name() const override;
    };
}
