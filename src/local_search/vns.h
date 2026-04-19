#pragma once

#include "../algorithms/algorithm.h"
#include "vnd.h"

#include <string>

namespace dckp
{
    /**
     * @brief Configuration knobs for the Variable Neighborhood Search driver.
     *
     * @c k_max controls how many shaking neighborhoods the search cycles
     * through before restarting at @c k = 1. The algorithm implements three
     * shaking neighborhoods (Add/Drop, Swap-1-1, Swap-2-1); when @c k_max
     * exceeds three the effective shaking move is mapped back into that
     * range via @c ((k - 1) % 3) + 1, keeping the outer schedule wider
     * without requiring extra primitives.
     *
     * @c vnd_config is forwarded verbatim to the inner VND used as the
     * local-search component.
     */
    struct VNSConfig
    {
        int k_max{3};
        VNDConfig vnd_config{};
    };

    /**
     * @brief Variable Neighborhood Search (VNS) for the DCKP.
     *
     * Classic basic-VNS skeleton on top of the project's VND:
     *  - Initial solution : GreedyMaxProfit followed by VND.
     *  - Shaking          : at shake level @c k a single random feasible
     *                       move is applied to the incumbent. k=1 is
     *                       Add-or-Drop, k=2 is Swap-1-1, k=3 is Swap-2-1.
     *                       Values of @c k beyond 3 are folded back into
     *                       the three primitives via modular arithmetic.
     *  - Local search     : VND::improve of the shaken solution.
     *  - Change of nbhd   : if the local optimum strictly improves the
     *                       incumbent, accept and reset @c k to 1. Else
     *                       advance @c k cyclically within [1, k_max].
     *
     * The incumbent and the global-best tracker are both strictly profit-
     * monotone. Shaking is intentionally profit-blind: it only guarantees
     * feasibility, so the VND local-search step performs every
     * intensification decision. Every strict improvement of @c best calls
     * @c ctx.stopping.registerImprovement() so the composite criterion's
     * no-improvement counter is reset correctly.
     *
     * The shake step does not reuse VND's private bookkeeping. A small
     * local state mirrors the selection with @c in_solution and
     * @c conflict_count vectors, kept consistent through applyAdd /
     * applyRemove. When a shaking neighborhood cannot find a feasible
     * move within its sampling budget, the incumbent is returned intact
     * — the outer loop then advances @c k and tries again.
     */
    class VNS : public Algorithm
    {
    public:
        VNS();
        explicit VNS(VNSConfig config);

        ~VNS() override = default;
        VNS(const VNS &) = delete;
        VNS &operator=(const VNS &) = delete;
        VNS(VNS &&) noexcept = default;
        VNS &operator=(VNS &&) noexcept = default;

        [[nodiscard]] Solution run(const DCKPInstance &instance, RunContext &ctx) override;
        [[nodiscard]] std::string name() const override;

        [[nodiscard]] const VNSConfig &config() const noexcept { return config_; }

    private:
        VNSConfig config_{};
    };
}
