#include "ils.h"
#include "selection_state.h"

#include "../constructive/greedy_max_profit.h"
#include "../constructive/profit_order.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <ostream>
#include <utility>
#include <vector>

namespace dckp
{
    namespace
    {
        using ItemId = DCKPInstance::ItemId;
        using Weight64 = Solution::TotalWeight;

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
        [[nodiscard]] Solution perturb(const Solution &incumbent,
                                       int strength,
                                       Rng &rng,
                                       const std::vector<ItemId> &repair_order)
        {
            Solution sol = incumbent;

            const DCKPInstance &instance = sol.instance();
            if (instance.n_items() <= 0)
            {
                return sol;
            }

            SelectionState state(instance, sol);

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

            const auto &weights = instance.weights();

            const auto capacity = instance.capacity();
            for (const ItemId u : repair_order)
            {
                const auto idx = static_cast<std::size_t>(u);
                if (state.in_solution[idx] != 0)
                {
                    continue;
                }
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

        void logIteration(std::ostream &out,
                          const std::size_t iteration,
                          const Solution::TotalProfit candidate_profit,
                          const Solution::TotalProfit incumbent_before,
                          const Solution::TotalProfit incumbent_after,
                          const Solution::TotalProfit best_before,
                          const Solution::TotalProfit best_after,
                          const bool accepted,
                          const std::size_t work_iterations,
                          const StoppingCriteria::Duration elapsed)
        {
            const auto gain = best_after - best_before;
            const double gain_percent = best_before == 0
                                            ? (gain > 0 ? 100.0 : 0.0)
                                            : 100.0 * static_cast<double>(gain) /
                                                  static_cast<double>(best_before);
            const auto flags = out.flags();
            const auto precision = out.precision();
            out << "ILS iteration=" << iteration
                << " candidate_profit=" << candidate_profit
                << " incumbent_before=" << incumbent_before
                << " incumbent_after=" << incumbent_after
                << " accepted=" << (accepted ? "true" : "false")
                << " improved=" << (gain > 0 ? "true" : "false")
                << " gain=" << gain
                << " gain_percent=" << std::fixed << std::setprecision(2) << gain_percent
                << " best_before=" << best_before
                << " best_after=" << best_after
                << " work_iterations=" << work_iterations
                << " elapsed_ms=" << elapsed.count() << '\n';
            out.flags(flags);
            out.precision(precision);
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
        const auto repair_order = makeProfitOrder(instance);
        const auto initial_profit = best.totalProfit();
        std::size_t iteration = 0;
        std::size_t improvements = 0;

        if (ctx.log != nullptr)
        {
            *ctx.log << "ILS start initial_profit=" << initial_profit
                     << " selected_items=" << best.size()
                     << " elapsed_ms=" << ctx.stopping.elapsed().count() << '\n';
        }

        while (!ctx.stopping.shouldStop())
        {
            ctx.stopping.tick();
            ++iteration;

            const auto incumbent_before = incumbent.totalProfit();
            const auto best_before = best.totalProfit();
            Solution perturbed = perturb(
                incumbent, config_.perturbation_strength, ctx.rng, repair_order);
            Solution local_opt = vnd.improve(perturbed, ctx);
            const auto candidate_profit = local_opt.totalProfit();
            const bool accepted = candidate_profit >= incumbent_before;
            const bool improved = candidate_profit > best_before;

            if (improved)
            {
                best = local_opt;
                ++improvements;
                ctx.stopping.registerImprovement();
            }

            if (accepted)
            {
                incumbent = std::move(local_opt);
            }

            if (ctx.log != nullptr)
            {
                logIteration(*ctx.log,
                             iteration,
                             candidate_profit,
                             incumbent_before,
                             incumbent.totalProfit(),
                             best_before,
                             best.totalProfit(),
                             accepted,
                             ctx.stopping.iterations(),
                             ctx.stopping.elapsed());
            }
        }

        if (ctx.log != nullptr)
        {
            const auto total_gain = best.totalProfit() - initial_profit;
            const double total_gain_percent = initial_profit == 0
                                                  ? (total_gain > 0 ? 100.0 : 0.0)
                                                  : 100.0 * static_cast<double>(total_gain) /
                                                        static_cast<double>(initial_profit);
            const auto flags = ctx.log->flags();
            const auto precision = ctx.log->precision();
            *ctx.log << "ILS end iterations=" << iteration
                     << " improvements=" << improvements
                     << " initial_profit=" << initial_profit
                     << " best_profit=" << best.totalProfit()
                     << " total_gain=" << total_gain
                     << " total_gain_percent=" << std::fixed << std::setprecision(2)
                     << total_gain_percent
                     << " work_iterations=" << ctx.stopping.iterations()
                     << " elapsed_ms=" << ctx.stopping.elapsed().count() << '\n';
            ctx.log->flags(flags);
            ctx.log->precision(precision);
        }

        best.setMethodName(name());
        return best;
    }
}
