#pragma once

#include "../utils/instance_reader.h"
#include "../utils/solution.h"
#include "rng.h"
#include "stopping_criteria.h"

#include <string>

namespace dckp
{
    /**
     * @brief Shared execution context passed to every Algorithm::run call.
     *
     * The Runner owns the RNG and StoppingCriteria; the algorithm consumes
     * them as non-owning references. Keeping these external keeps the
     * algorithm itself reproducible and composable.
     */
    struct RunContext
    {
        Rng &rng;
        StoppingCriteria &stopping;
    };

    /**
     * @brief Abstract interface for any DCKP algorithm (constructive,
     * local search, tabu, GA, path-relinking, etc.).
     */
    class Algorithm
    {
    public:
        Algorithm() = default;
        Algorithm(const Algorithm &) = default;
        Algorithm(Algorithm &&) noexcept = default;
        Algorithm &operator=(const Algorithm &) = default;
        Algorithm &operator=(Algorithm &&) noexcept = default;
        virtual ~Algorithm() = default;

        /**
         * @brief Runs the algorithm and returns its best solution.
         * Implementations must respect @p ctx.stopping.shouldStop() and
         * only consume randomness through @p ctx.rng.
         */
        [[nodiscard]] virtual Solution run(const DCKPInstance &instance, RunContext &ctx) = 0;

        /**
         * @brief Human-readable name used in Solution::methodName and logs.
         */
        [[nodiscard]] virtual std::string name() const = 0;
    };
}
