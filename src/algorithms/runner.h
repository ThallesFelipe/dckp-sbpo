#pragma once

#include "../utils/instance_reader.h"
#include "../utils/solution.h"
#include "algorithm.h"
#include "rng.h"
#include "stopping_criteria.h"

#include <cstddef>
#include <cstdint>

namespace dckp
{
    /**
     * @brief Configuration consumed by Runner::execute.
     *
     * Any field left at its default (zero) is treated as inactive, except
     * @c seed which is always applied. The Runner configures the
     * StoppingCriteria exactly according to the active fields.
     */
    struct RunnerConfig
    {
        Rng::Seed seed{0};
        StoppingCriteria::Duration time_limit{0};
        std::size_t iteration_limit{0};
        std::size_t no_improvement_limit{0};
    };

    /**
     * @brief Wires an Algorithm to a DCKPInstance with reproducible seed,
     * stopping criteria and timing. Returns the Solution produced by the
     * algorithm, with its methodName and computationTime populated.
     */
    class Runner
    {
    public:
        explicit Runner(const DCKPInstance &instance) noexcept;

        [[nodiscard]] Solution execute(Algorithm &algorithm, const RunnerConfig &config);

    private:
        const DCKPInstance &instance_;
    };
}
