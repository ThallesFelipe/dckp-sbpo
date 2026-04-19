#pragma once

#include "../algorithms/algorithm.h"

#include <memory>
#include <string>

namespace dckp
{
    /**
     * @brief Configuration of the VND neighborhood schedule.
     *
     * Each flag independently enables one of the four neighborhoods. VND
     * visits the enabled neighborhoods in the declared order (N1 before
     * N2 before N3 before N4), restarting at N1 after any improvement.
     */
    struct VNDConfig
    {
        bool enable_add{true};
        bool enable_swap_1_1{true};
        bool enable_swap_2_1{true};
        bool enable_swap_1_2{true};
    };

    /**
     * @brief Variable Neighborhood Descent for the DCKP.
     *
     * Classic deterministic VND: a feasible starting solution is produced
     * either from an injected seed Algorithm or from GreedyMaxProfit by
     * default. The search then walks through a fixed sequence of four
     * profit-strictly-improving neighborhoods (Add, Swap-1-1, Swap-2-1,
     * Swap-1-2). Moves are applied under first-improvement; whenever a
     * neighborhood yields an improving move the search restarts at N1.
     * When the last enabled neighborhood is exhausted without improvement
     * the current solution is a local optimum w.r.t. all enabled
     * neighborhoods and is returned.
     *
     * All candidate feasibility checks rely on an incremental
     * @c conflict_count vector (per-item count of currently-selected
     * conflicting neighbors), updated in O(degree) on every add/remove.
     * A swap feasibility test subtracts the contribution of items about
     * to leave the solution, so no rebuild is required.
     *
     * Complexity per pass (n items, k = |S|, d = avg degree):
     *  - N1:  O(n)
     *  - N2:  O(k · n)          per pass
     *  - N3:  O(k² · n)         per pass
     *  - N4:  O(k · n²)         per pass (with a candidate filter that
     *                           typically prunes aggressively)
     *
     * The algorithm never produces an infeasible solution: every move is
     * rejected unless both capacity and conflict constraints hold after
     * applying it.
     */
    class VND : public Algorithm
    {
    public:
        VND();
        explicit VND(VNDConfig config);
        VND(VNDConfig config, std::unique_ptr<Algorithm> seed_algorithm);

        ~VND() override = default;
        VND(const VND &) = delete;
        VND &operator=(const VND &) = delete;
        VND(VND &&) noexcept = default;
        VND &operator=(VND &&) noexcept = default;

        [[nodiscard]] Solution run(const DCKPInstance &instance, RunContext &ctx) override;
        [[nodiscard]] std::string name() const override;

        /**
         * @brief Applies the VND local search to @p seed and returns the
         * resulting local optimum. The seed must be bound to a feasible
         * selection for its own instance; the search preserves feasibility.
         */
        [[nodiscard]] Solution improve(const Solution &seed, RunContext &ctx);

        [[nodiscard]] const VNDConfig &config() const noexcept { return config_; }

    private:
        VNDConfig config_{};
        std::unique_ptr<Algorithm> seed_algorithm_{};
    };
}
