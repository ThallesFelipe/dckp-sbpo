#include "runner.h"

#include <chrono>
#include <string>

namespace dckp
{
    Runner::Runner(const DCKPInstance &instance) noexcept
        : instance_(instance)
    {
    }

    Solution Runner::execute(Algorithm &algorithm, const RunnerConfig &config)
    {
        Rng rng(config.seed);
        StoppingCriteria stopping;
        if (config.time_limit > StoppingCriteria::Duration::zero())
        {
            stopping.setTimeLimit(config.time_limit);
        }
        if (config.iteration_limit > 0)
        {
            stopping.setIterationLimit(config.iteration_limit);
        }
        if (config.no_improvement_limit > 0)
        {
            stopping.setMaxIterationsWithoutImprovement(config.no_improvement_limit);
        }

        RunContext ctx{rng, stopping};
        stopping.start();

        const auto wall_start = std::chrono::steady_clock::now();
        Solution result = algorithm.run(instance_, ctx);
        const auto wall_end = std::chrono::steady_clock::now();

        const double elapsed_seconds =
            std::chrono::duration<double>(wall_end - wall_start).count();
        result.setComputationTime(elapsed_seconds);
        if (std::string_view{result.methodName()} == "Unknown")
        {
            result.setMethodName(algorithm.name());
        }
        return result;
    }
}
