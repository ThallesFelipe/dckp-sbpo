#include "vnd.h"

#include "../constructive/greedy_max_profit.h"

#include <cstdint>
#include <utility>
#include <vector>

namespace dckp
{
    namespace
    {
        using ItemId = DCKPInstance::ItemId;
        using Weight64 = Solution::TotalWeight;
        using Profit64 = Solution::TotalProfit;

        /**
         * @brief Mutable bookkeeping shared by every neighborhood.
         *
         * @c in_solution mirrors Solution::selectedItems() as a byte flag
         * for O(1) membership. @c conflict_count[u] is the number of
         * currently-selected items that conflict with @c u — an unselected
         * item is admissible only when its count is zero. Both are kept
         * in sync with Solution through @c applyAdd / @c applyRemove.
         */
        struct State
        {
            const DCKPInstance &instance;
            Solution &solution;
            std::vector<std::int32_t> conflict_count;
            std::vector<char> in_solution;

            State(const DCKPInstance &inst, Solution &sol)
                : instance(inst),
                  solution(sol),
                  conflict_count(static_cast<std::size_t>(inst.n_items()), std::int32_t{0}),
                  in_solution(static_cast<std::size_t>(inst.n_items()), char{0})
            {
                const auto &graph = instance.conflict_graph();
                for (const ItemId item : solution.selectedItems())
                {
                    in_solution[static_cast<std::size_t>(item)] = 1;
                }
                for (const ItemId item : solution.selectedItems())
                {
                    for (const ItemId nbr : graph[static_cast<std::size_t>(item)])
                    {
                        ++conflict_count[static_cast<std::size_t>(nbr)];
                    }
                }
            }

            void applyAdd(ItemId item)
            {
                const auto idx = static_cast<std::size_t>(item);
                solution.addItem(item);
                in_solution[idx] = 1;
                for (const ItemId nbr : instance.conflict_graph()[idx])
                {
                    ++conflict_count[static_cast<std::size_t>(nbr)];
                }
            }

            void applyRemove(ItemId item)
            {
                const auto idx = static_cast<std::size_t>(item);
                solution.removeItem(item);
                in_solution[idx] = 0;
                for (const ItemId nbr : instance.conflict_graph()[idx])
                {
                    --conflict_count[static_cast<std::size_t>(nbr)];
                }
            }

            [[nodiscard]] bool isSelected(ItemId item) const noexcept
            {
                return in_solution[static_cast<std::size_t>(item)] != 0;
            }
        };

        /**
         * @brief N1 — Add. Scans unselected items and accepts the first
         * one that fits in the residual capacity and has no conflict
         * with the current selection. Strictly increases profit iff the
         * item has positive profit.
         */
        bool tryAdd(State &s, StoppingCriteria &stopping)
        {
            const auto &weights = s.instance.weights();
            const auto &profits = s.instance.profits();
            const auto capacity = s.instance.capacity();
            const auto n = s.instance.n_items();

            for (ItemId u = 0; u < n; ++u)
            {
                if (stopping.shouldStop())
                {
                    return false;
                }
                const auto idx = static_cast<std::size_t>(u);
                if (s.in_solution[idx])
                {
                    continue;
                }
                if (s.conflict_count[idx] != 0)
                {
                    continue;
                }
                if (profits[idx] <= 0)
                {
                    continue;
                }
                const auto w_u = static_cast<Weight64>(weights[idx]);
                if (s.solution.totalWeight() + w_u > capacity)
                {
                    continue;
                }

                s.applyAdd(u);
                return true;
            }
            return false;
        }

        /**
         * @brief N2 — 1-1 swap. For each selected @c i and each unselected
         * @c u with strictly higher profit, checks that removing @c i
         * makes @c u conflict-free (@c conflict_count[u] - [i↔u] == 0)
         * and that capacity is respected.
         */
        bool trySwap11(State &s, StoppingCriteria &stopping)
        {
            const auto &weights = s.instance.weights();
            const auto &profits = s.instance.profits();
            const auto capacity = s.instance.capacity();
            const auto n = s.instance.n_items();

            const std::vector<ItemId> selected(
                s.solution.selectedItems().begin(), s.solution.selectedItems().end());

            for (const ItemId i : selected)
            {
                if (stopping.shouldStop())
                {
                    return false;
                }
                const auto i_idx = static_cast<std::size_t>(i);
                const auto w_i = static_cast<Weight64>(weights[i_idx]);
                const auto p_i = profits[i_idx];

                for (ItemId u = 0; u < n; ++u)
                {
                    const auto u_idx = static_cast<std::size_t>(u);
                    if (s.in_solution[u_idx])
                    {
                        continue;
                    }
                    if (profits[u_idx] <= p_i)
                    {
                        continue;
                    }
                    const std::int32_t c_from_i = s.instance.has_conflict(i, u) ? 1 : 0;
                    if (s.conflict_count[u_idx] - c_from_i != 0)
                    {
                        continue;
                    }
                    const auto w_u = static_cast<Weight64>(weights[u_idx]);
                    if (s.solution.totalWeight() - w_i + w_u > capacity)
                    {
                        continue;
                    }

                    s.applyRemove(i);
                    s.applyAdd(u);
                    return true;
                }
            }
            return false;
        }

        /**
         * @brief N3 — 2-1 swap. For each selected pair (i, j) and each
         * unselected @c u with profit strictly greater than p_i + p_j,
         * checks that removing both makes @c u conflict-free and that
         * capacity is respected.
         */
        bool trySwap21(State &s, StoppingCriteria &stopping)
        {
            const auto &weights = s.instance.weights();
            const auto &profits = s.instance.profits();
            const auto capacity = s.instance.capacity();
            const auto n = s.instance.n_items();

            const std::vector<ItemId> selected(
                s.solution.selectedItems().begin(), s.solution.selectedItems().end());

            for (std::size_t a = 0; a < selected.size(); ++a)
            {
                if (stopping.shouldStop())
                {
                    return false;
                }
                const ItemId i = selected[a];
                const auto i_idx = static_cast<std::size_t>(i);
                const auto w_i = static_cast<Weight64>(weights[i_idx]);
                const auto p_i = profits[i_idx];

                for (std::size_t b = a + 1; b < selected.size(); ++b)
                {
                    if (stopping.shouldStop())
                    {
                        return false;
                    }
                    const ItemId j = selected[b];
                    const auto j_idx = static_cast<std::size_t>(j);
                    const auto w_j = static_cast<Weight64>(weights[j_idx]);
                    const auto p_j = profits[j_idx];
                    const auto p_ij =
                        static_cast<Profit64>(p_i) + static_cast<Profit64>(p_j);

                    for (ItemId u = 0; u < n; ++u)
                    {
                        const auto u_idx = static_cast<std::size_t>(u);
                        if (s.in_solution[u_idx])
                        {
                            continue;
                        }
                        if (static_cast<Profit64>(profits[u_idx]) <= p_ij)
                        {
                            continue;
                        }
                        const std::int32_t c_from_i = s.instance.has_conflict(i, u) ? 1 : 0;
                        const std::int32_t c_from_j = s.instance.has_conflict(j, u) ? 1 : 0;
                        if (s.conflict_count[u_idx] - c_from_i - c_from_j != 0)
                        {
                            continue;
                        }
                        const auto w_u = static_cast<Weight64>(weights[u_idx]);
                        if (s.solution.totalWeight() - w_i - w_j + w_u > capacity)
                        {
                            continue;
                        }

                        s.applyRemove(i);
                        s.applyRemove(j);
                        s.applyAdd(u);
                        return true;
                    }
                }
            }
            return false;
        }

        /**
         * @brief N4 — 1-2 swap. For each selected @c i, pre-filters the
         * set of unselected items that would be both capacity- and
         * conflict-feasible after @c i is removed, then scans candidate
         * pairs (u, v) that are pairwise compatible and whose combined
         * profit strictly exceeds p_i.
         */
        bool trySwap12(State &s, StoppingCriteria &stopping)
        {
            const auto &weights = s.instance.weights();
            const auto &profits = s.instance.profits();
            const auto capacity = s.instance.capacity();
            const auto n = s.instance.n_items();

            const std::vector<ItemId> selected(
                s.solution.selectedItems().begin(), s.solution.selectedItems().end());

            for (const ItemId i : selected)
            {
                if (stopping.shouldStop())
                {
                    return false;
                }
                const auto i_idx = static_cast<std::size_t>(i);
                const auto w_i = static_cast<Weight64>(weights[i_idx]);
                const auto p_i = static_cast<Profit64>(profits[i_idx]);
                const Weight64 base_weight = s.solution.totalWeight() - w_i;

                std::vector<ItemId> candidates;
                candidates.reserve(static_cast<std::size_t>(n));
                for (ItemId u = 0; u < n; ++u)
                {
                    const auto u_idx = static_cast<std::size_t>(u);
                    if (s.in_solution[u_idx])
                    {
                        continue;
                    }
                    const std::int32_t c_from_i = s.instance.has_conflict(i, u) ? 1 : 0;
                    if (s.conflict_count[u_idx] - c_from_i != 0)
                    {
                        continue;
                    }
                    const auto w_u = static_cast<Weight64>(weights[u_idx]);
                    if (base_weight + w_u > capacity)
                    {
                        continue;
                    }
                    candidates.push_back(u);
                }

                for (std::size_t a = 0; a < candidates.size(); ++a)
                {
                    if (stopping.shouldStop())
                    {
                        return false;
                    }
                    const ItemId u = candidates[a];
                    const auto u_idx = static_cast<std::size_t>(u);
                    const auto w_u = static_cast<Weight64>(weights[u_idx]);
                    const auto p_u = static_cast<Profit64>(profits[u_idx]);

                    for (std::size_t b = a + 1; b < candidates.size(); ++b)
                    {
                        const ItemId v = candidates[b];
                        const auto v_idx = static_cast<std::size_t>(v);
                        const auto p_v = static_cast<Profit64>(profits[v_idx]);

                        if (p_u + p_v <= p_i)
                        {
                            continue;
                        }
                        if (s.instance.has_conflict(u, v))
                        {
                            continue;
                        }
                        const auto w_v = static_cast<Weight64>(weights[v_idx]);
                        if (base_weight + w_u + w_v > capacity)
                        {
                            continue;
                        }

                        s.applyRemove(i);
                        s.applyAdd(u);
                        s.applyAdd(v);
                        return true;
                    }
                }
            }
            return false;
        }

        using Neighborhood = bool (*)(State &, StoppingCriteria &);
    }

    VND::VND() = default;

    VND::VND(VNDConfig config) : config_(config) {}

    VND::VND(VNDConfig config, std::unique_ptr<Algorithm> seed_algorithm)
        : config_(config), seed_algorithm_(std::move(seed_algorithm))
    {
    }

    std::string VND::name() const
    {
        return "VND";
    }

    Solution VND::run(const DCKPInstance &instance, RunContext &ctx)
    {
        Solution seed = [&]() -> Solution
        {
            if (seed_algorithm_)
            {
                return seed_algorithm_->run(instance, ctx);
            }
            GreedyMaxProfit greedy;
            return greedy.run(instance, ctx);
        }();

        return improve(seed, ctx);
    }

    Solution VND::improve(const Solution &seed, RunContext &ctx)
    {
        Solution best = seed;
        best.setMethodName(name());

        const DCKPInstance &instance = best.instance();
        if (instance.n_items() <= 0)
        {
            return best;
        }

        std::vector<Neighborhood> schedule;
        if (config_.enable_add)
        {
            schedule.push_back(&tryAdd);
        }
        if (config_.enable_swap_1_1)
        {
            schedule.push_back(&trySwap11);
        }
        if (config_.enable_swap_2_1)
        {
            schedule.push_back(&trySwap21);
        }
        if (config_.enable_swap_1_2)
        {
            schedule.push_back(&trySwap12);
        }
        if (schedule.empty())
        {
            return best;
        }

        State state(instance, best);

        std::size_t k = 0;
        while (k < schedule.size())
        {
            if (ctx.stopping.shouldStop())
            {
                break;
            }
            ctx.stopping.tick();

            const Profit64 profit_before = best.totalProfit();
            const bool moved = schedule[k](state, ctx.stopping);
            if (moved && best.totalProfit() > profit_before)
            {
                ctx.stopping.registerImprovement();
                k = 0;
            }
            else
            {
                ++k;
            }
        }

        return best;
    }
}
