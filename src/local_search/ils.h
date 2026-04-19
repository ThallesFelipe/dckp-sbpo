#pragma once

#include "../algorithms/algorithm.h"
#include "vnd.h"

#include <string>

namespace dckp
{
    /**
     * @brief Configuration knobs for the Iterated Local Search driver.
     *
     * @c perturbation_strength controls the number of items removed at
     * every kick (a random subset of the incumbent). Larger values promote
     * diversification at the cost of intensification.
     *
     * @c vnd_config is forwarded verbatim to the inner VND used as the
     * local-search component.
     */
    struct ILSConfig
    {
        int perturbation_strength{4};
        VNDConfig vnd_config{};
    };

    /**
     * @brief Iterated Local Search (ILS) for the DCKP.
     *
     * Classic four-component ILS skeleton:
     *  - Initial solution     : GreedyMaxProfit.
     *  - Local search         : VND (Add, Swap-1-1, Swap-2-1, Swap-1-2).
     *  - Perturbation         : random k-destruction of the incumbent
     *                           followed by a deterministic greedy fill
     *                           (decreasing profit) that restores
     *                           feasibility and re-packs the knapsack.
     *  - Acceptance criterion : @c local_opt.profit >= @c incumbent.profit
     *                           (plateau-permissive, favoring exploration).
     *
     * The algorithm runs until @c ctx.stopping reports termination. The
     * elite tracker (`best`) is decoupled from the acceptance criterion:
     * `best` is strictly profit-monotone, while `incumbent` is allowed to
     * walk across equal-profit plateaus. Every strict best-update calls
     * @c ctx.stopping.registerImprovement() so the composite criterion's
     * no-improvement counter is reset correctly.
     *
     * ILS does not re-seed via GreedyMaxProfit during the loop: the
     * constructive heuristic is invoked exactly once at the start. All
     * subsequent diversification happens through the perturbation step.
     */
    class ILS : public Algorithm
    {
    public:
        ILS();
        explicit ILS(ILSConfig config);

        ~ILS() override = default;
        ILS(const ILS &) = delete;
        ILS &operator=(const ILS &) = delete;
        ILS(ILS &&) noexcept = default;
        ILS &operator=(ILS &&) noexcept = default;

        [[nodiscard]] Solution run(const DCKPInstance &instance, RunContext &ctx) override;
        [[nodiscard]] std::string name() const override;

        [[nodiscard]] const ILSConfig &config() const noexcept { return config_; }

    private:
        ILSConfig config_{};
    };
}
