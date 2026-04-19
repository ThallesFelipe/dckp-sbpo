#include "ils.h"

#include "../constructive/greedy_max_profit.h"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace dckp
{
    namespace
    {
        using ItemId = DCKPInstance::ItemId;
        using Weight64 = Solution::TotalWeight;

        /**
         * @brief Lightweight mirror of the Solution's selection used only
         * during perturbation.
         *
         * @c in_solution gives O(1) membership queries and @c conflict_count
         * gives O(1) per-candidate conflict admissibility tests. Both are
         * maintained incrementally in @c applyAdd / @c applyRemove at
         * O(degree) cost per mutation — matching the bookkeeping used by
         * VND but intentionally kept local and minimal, as ILS only needs
         * it during a single perturbation call.
         */
        struct PerturbState
        {
            const DCKPInstance &instance;
            Solution &solution;
            std::vector<std::int32_t> conflict_count;
            std::vector<char> in_solution;

            PerturbState(const DCKPInstance &inst, Solution &sol)
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
        };

        /**
         * @brief Produces a perturbed copy of @p incumbent.
         *
         * Two phases:
         *  1. Random k-destruction. Pick @p strength distinct selected
         *     items uniformly at random (or every selected item, if the
         *     incumbent is smaller than @p strength) and remove them.
         *     A partial Fisher–Yates shuffle does this in O(k).
         *  2. Greedy fill. Iterate the unselected items in decreasing
         *     profit (ties: lighter first) and insert each one whose
         *     residual capacity and conflict counter both allow it. This
         *     keeps the perturbation cost bounded and the output feasible
         *     while re-packing the knapsack after the destructive step.
         *
         * The combined effect is a "destroy-and-repair" kick in the
         *  LNS / ruin-and-recreate family — diversifying enough to
         * leave the current VND basin of attraction without discarding
         * the structural decisions the incumbent already made.
         */
        [[nodiscard]] Solution perturb(const Solution &incumbent, int strength, Rng &rng)
        {
            Solution sol = incumbent;

            const DCKPInstance &instance = sol.instance();
            if (instance.n_items() <= 0)
            {
                return sol;
            }

            PerturbState state(instance, sol);

            std::vector<ItemId> selected(sol.selectedItems().begin(),
                                         sol.selectedItems().end());
            const auto clamped_strength = static_cast<std::size_t>(std::max(0, strength));
            const auto k = std::min(clamped_strength, selected.size());

            for (std::size_t i = 0; i < k; ++i)
            {
                const auto last = static_cast<std::int64_t>(selected.size() - 1);
                const auto j = static_cast<std::size_t>(
                    rng.uniformInt(static_cast<std::int64_t>(i), last));
                std::swap(selected[i], selected[j]);
                state.applyRemove(selected[i]);
            }

            const auto n = instance.n_items();
            const auto &profits = instance.profits();
            const auto &weights = instance.weights();

            std::vector<ItemId> order;
            order.reserve(static_cast<std::size_t>(n));
            for (ItemId u = 0; u < n; ++u)
            {
                if (state.in_solution[static_cast<std::size_t>(u)] == 0)
                {
                    order.push_back(u);
                }
            }
            std::sort(order.begin(), order.end(),
                      [&profits, &weights](ItemId a, ItemId b) noexcept
                      {
                          const auto ai = static_cast<std::size_t>(a);
                          const auto bi = static_cast<std::size_t>(b);
                          if (profits[ai] != profits[bi])
                          {
                              return profits[ai] > profits[bi];
                          }
                          return weights[ai] < weights[bi];
                      });

            const auto capacity = instance.capacity();
            for (const ItemId u : order)
            {
                const auto idx = static_cast<std::size_t>(u);
                if (state.conflict_count[idx] != 0)
                {
                    continue;
                }
                const auto w_u = static_cast<Weight64>(weights[idx]);
                if (sol.totalWeight() + w_u > capacity)
                {
                    continue;
                }
                state.applyAdd(u);
            }

            return sol;
        }
    }

    ILS::ILS() = default;

    ILS::ILS(ILSConfig config) : config_(config) {}

    std::string ILS::name() const
    {
        return "ILS";
    }

    Solution ILS::run(const DCKPInstance &instance, RunContext &ctx)
    {
        GreedyMaxProfit greedy;
        Solution s0 = greedy.run(instance, ctx);

        VND vnd(config_.vnd_config);
        Solution incumbent = vnd.improve(s0, ctx);
        Solution best = incumbent;

        while (!ctx.stopping.shouldStop())
        {
            ctx.stopping.tick();

            Solution perturbed = perturb(incumbent, config_.perturbation_strength, ctx.rng);
            Solution local_opt = vnd.improve(perturbed, ctx);

            if (local_opt.totalProfit() >= incumbent.totalProfit())
            {
                incumbent = local_opt;
            }

            if (local_opt.totalProfit() > best.totalProfit())
            {
                best = local_opt;
                ctx.stopping.registerImprovement();
            }
        }

        best.setMethodName(name());
        return best;
    }
}
