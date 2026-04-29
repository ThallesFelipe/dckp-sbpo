#include "vns.h"

#include "../constructive/greedy_max_profit.h"

#include <algorithm>
#include <cstdint>
#include <vector>
#include <iostream>

namespace dckp
{
    namespace
    {
        using ItemId = DCKPInstance::ItemId;
        using Weight64 = Solution::TotalWeight;

        /**
         * @brief Lightweight mirror of the Solution's selection used only
         * during a single shake call.
         *
         * @c in_solution provides O(1) membership queries, @c conflict_count
         * provides O(1) per-candidate conflict admissibility tests. Both
         * are maintained incrementally through @c applyAdd / @c applyRemove
         * at O(degree) per mutation — deliberately kept local to vns.cpp,
         * matching the pattern used by VND's State and ILS's PerturbState
         * without exposing either.
         */
        struct ShakeState
        {
            const DCKPInstance &instance;
            Solution &solution;
            std::vector<std::int32_t> conflict_count;
            std::vector<char> in_solution;

            ShakeState(const DCKPInstance &inst, Solution &sol)
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
         * @brief Maps an arbitrary shake level @p k to one of the three
         * implemented primitives (1, 2, 3). Values of @c k_max beyond 3
         * reuse the primitives in a modular cycle, so wider schedules
         * diversify through repetition rather than new movement kinds.
         */
        [[nodiscard]] int mapShakeMove(int k) noexcept
        {
            if (k < 1)
            {
                k = 1;
            }
            return ((k - 1) % 3) + 1;
        }

        /**
         * @brief k=1 — Add/Drop shake.
         *
         * With equal probability attempts either an Add (insert a random
         * unselected item that satisfies capacity and conflicts) or a
         * Drop (remove a random selected item). A bounded number of
         * samples (2 * n) is drawn from the uniform distribution over
         * the corresponding universe; if none yields a feasible move
         * the incumbent is returned intact.
         */
        [[nodiscard]] bool shakeAddDrop(ShakeState &s, Rng &rng)
        {
            const auto n = s.instance.n_items();
            if (n <= 0)
            {
                return false;
            }
            const auto &weights = s.instance.weights();
            const auto capacity = s.instance.capacity();
            const auto budget = static_cast<std::int64_t>(n) * 2;

            const bool try_add_first = rng.uniformInt(0, 1) == 0;

            auto try_add = [&]() -> bool
            {
                for (std::int64_t attempt = 0; attempt < budget; ++attempt)
                {
                    const auto u = static_cast<ItemId>(
                        rng.uniformInt(0, static_cast<std::int64_t>(n) - 1));
                    const auto idx = static_cast<std::size_t>(u);
                    if (s.in_solution[idx])
                    {
                        continue;
                    }
                    if (s.conflict_count[idx] != 0)
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
            };

            auto try_drop = [&]() -> bool
            {
                if (s.solution.empty())
                {
                    return false;
                }
                const std::vector<ItemId> selected(
                    s.solution.selectedItems().begin(),
                    s.solution.selectedItems().end());
                const auto last = static_cast<std::int64_t>(selected.size() - 1);
                const auto pick = static_cast<std::size_t>(rng.uniformInt(0, last));
                s.applyRemove(selected[pick]);
                return true;
            };

            if (try_add_first)
            {
                if (try_add())
                    return true;
                return try_drop();
            }
            if (try_drop())
                return true;
            return try_add();
        }

        /**
         * @brief k=2 — Swap-1-1 shake.
         *
         * Samples a random pair (i ∈ S, u ∉ S) and accepts the first pair
         * whose swap preserves capacity and conflict feasibility. Profit
         * is ignored. A budget of @c n samples bounds the cost; if no
         * feasible pair is found the incumbent is returned intact.
         */
        [[nodiscard]] bool shakeSwap11(ShakeState &s, Rng &rng)
        {
            const auto n = s.instance.n_items();
            if (n <= 0 || s.solution.empty())
            {
                return false;
            }
            const auto &weights = s.instance.weights();
            const auto capacity = s.instance.capacity();

            const std::vector<ItemId> selected(
                s.solution.selectedItems().begin(),
                s.solution.selectedItems().end());
            const auto s_last = static_cast<std::int64_t>(selected.size() - 1);
            const auto budget = static_cast<std::int64_t>(n);

            for (std::int64_t attempt = 0; attempt < budget; ++attempt)
            {
                const auto i = selected[static_cast<std::size_t>(
                    rng.uniformInt(0, s_last))];
                const auto u = static_cast<ItemId>(
                    rng.uniformInt(0, static_cast<std::int64_t>(n) - 1));
                if (u == i)
                {
                    continue;
                }
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
                const auto i_idx = static_cast<std::size_t>(i);
                const auto w_i = static_cast<Weight64>(weights[i_idx]);
                const auto w_u = static_cast<Weight64>(weights[u_idx]);
                if (s.solution.totalWeight() - w_i + w_u > capacity)
                {
                    continue;
                }
                s.applyRemove(i);
                s.applyAdd(u);
                return true;
            }
            return false;
        }

        /**
         * @brief k=3 — Swap-2-1 shake.
         *
         * Samples a random triple (i, j ∈ S with i != j, u ∉ S) and
         * accepts the first triple whose swap preserves capacity and
         * conflict feasibility. Profit is ignored. A budget of @c n
         * samples bounds the cost; if no feasible triple is found the
         * incumbent is returned intact.
         */
        [[nodiscard]] bool shakeSwap21(ShakeState &s, Rng &rng)
        {
            const auto n = s.instance.n_items();
            if (n <= 0 || s.solution.size() < 2)
            {
                return false;
            }
            const auto &weights = s.instance.weights();
            const auto capacity = s.instance.capacity();

            const std::vector<ItemId> selected(
                s.solution.selectedItems().begin(),
                s.solution.selectedItems().end());
            const auto s_last = static_cast<std::int64_t>(selected.size() - 1);
            const auto budget = static_cast<std::int64_t>(n);

            for (std::int64_t attempt = 0; attempt < budget; ++attempt)
            {
                const auto a = static_cast<std::size_t>(rng.uniformInt(0, s_last));
                auto b = static_cast<std::size_t>(rng.uniformInt(0, s_last));
                if (a == b)
                {
                    b = (b + 1) % selected.size();
                }
                const ItemId i = selected[a];
                const ItemId j = selected[b];
                const auto u = static_cast<ItemId>(
                    rng.uniformInt(0, static_cast<std::int64_t>(n) - 1));
                if (u == i || u == j)
                {
                    continue;
                }
                const auto u_idx = static_cast<std::size_t>(u);
                if (s.in_solution[u_idx])
                {
                    continue;
                }
                const std::int32_t c_from_i = s.instance.has_conflict(i, u) ? 1 : 0;
                const std::int32_t c_from_j = s.instance.has_conflict(j, u) ? 1 : 0;
                if (s.conflict_count[u_idx] - c_from_i - c_from_j != 0)
                {
                    continue;
                }
                const auto i_idx = static_cast<std::size_t>(i);
                const auto j_idx = static_cast<std::size_t>(j);
                const auto w_i = static_cast<Weight64>(weights[i_idx]);
                const auto w_j = static_cast<Weight64>(weights[j_idx]);
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
            return false;
        }

        /**
         * @brief Returns a feasible copy of @p incumbent with exactly one
         * random move applied at the shake neighborhood selected by @p k.
         * If no feasible move can be sampled within the per-neighborhood
         * budget, the returned solution equals the incumbent.
         */
        [[nodiscard]] Solution shake(const Solution &incumbent, int k, Rng &rng)
        {
            Solution sol = incumbent;

            const DCKPInstance &instance = sol.instance();
            if (instance.n_items() <= 0)
            {
                return sol;
            }

            ShakeState state(instance, sol);

            const int move = mapShakeMove(k);
            switch (move)
            {
            case 1:
                (void)shakeAddDrop(state, rng);
                break;
            case 2:
                (void)shakeSwap11(state, rng);
                break;
            case 3:
                (void)shakeSwap21(state, rng);
                break;
            default:
                break;
            }
            return sol;
        }
    }

    VNS::VNS() = default;

    VNS::VNS(VNSConfig config) : config_(config) {}

    std::string VNS::name() const
    {
        return "VNS";
    }

    Solution VNS::run(const DCKPInstance &instance, RunContext &ctx)
    {
        GreedyMaxProfit greedy;
        Solution s0 = greedy.run(instance, ctx);

        VND vnd(config_.vnd_config);
        Solution incumbent = vnd.improve(s0, ctx);
        Solution best = incumbent;

        const int k_max = std::max(1, config_.k_max);

        int k = 1;
        while (!ctx.stopping.shouldStop())
        {
            ctx.stopping.tick();

            Solution shaken = shake(incumbent, k, ctx.rng);
            Solution local_opt = vnd.improve(shaken, ctx);

            if (local_opt.totalProfit() > incumbent.totalProfit())
            {
                incumbent = local_opt;
                k = 1;
            }
            else
            {
                k = (k % k_max) + 1;
            }

            if (local_opt.totalProfit() > best.totalProfit())
            {
                const auto old_profit = best.totalProfit();
                best = local_opt;
                ctx.stopping.registerImprovement();
                std::cerr << "VNS: improved profit " << old_profit
                          << " -> " << best.totalProfit() << '\n';
                ;
            }
        }

        best.setMethodName(name());
        return best;
    }
}
