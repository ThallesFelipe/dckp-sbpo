#pragma once

#include <chrono>
#include <cstddef>

namespace dckp
{
    /**
     * @brief Composite stopping criterion for iterative algorithms.
     *
     * Any combination of wall-clock time, total iterations, and iterations
     * without improvement can be active simultaneously. `shouldStop()`
     * returns true as soon as any active limit is reached. A criterion is
     * inactive until explicitly configured via its setter.
     */
    class StoppingCriteria
    {
    public:
        using Clock = std::chrono::steady_clock;
        using Duration = std::chrono::milliseconds;

        StoppingCriteria() = default;

        void setTimeLimit(Duration limit) noexcept
        {
            time_limit_ = limit;
            has_time_limit_ = true;
        }

        void setIterationLimit(std::size_t limit) noexcept
        {
            iter_limit_ = limit;
            has_iter_limit_ = true;
        }

        void setMaxIterationsWithoutImprovement(std::size_t limit) noexcept
        {
            no_improve_limit_ = limit;
            has_no_improve_limit_ = true;
        }

        void start() noexcept
        {
            start_time_ = Clock::now();
            iterations_ = 0;
            iterations_at_last_improvement_ = 0;
            started_ = true;
        }

        void tick() noexcept
        {
            ++iterations_;
        }

        void registerImprovement() noexcept
        {
            iterations_at_last_improvement_ = iterations_;
        }

        [[nodiscard]] bool shouldStop() const noexcept
        {
            if (!started_)
            {
                return false;
            }
            if (has_time_limit_ && elapsed() >= time_limit_)
            {
                return true;
            }
            if (has_iter_limit_ && iterations_ >= iter_limit_)
            {
                return true;
            }
            if (has_no_improve_limit_ &&
                (iterations_ - iterations_at_last_improvement_) >= no_improve_limit_)
            {
                return true;
            }
            return false;
        }

        [[nodiscard]] Duration elapsed() const noexcept
        {
            return std::chrono::duration_cast<Duration>(Clock::now() - start_time_);
        }

        [[nodiscard]] std::size_t iterations() const noexcept { return iterations_; }
        [[nodiscard]] bool started() const noexcept { return started_; }

    private:
        Clock::time_point start_time_{};
        Duration time_limit_{0};
        std::size_t iter_limit_{0};
        std::size_t no_improve_limit_{0};
        std::size_t iterations_{0};
        std::size_t iterations_at_last_improvement_{0};
        bool has_time_limit_{false};
        bool has_iter_limit_{false};
        bool has_no_improve_limit_{false};
        bool started_{false};
    };
}
