#pragma once

#include <cstdint>
#include <limits>
#include <random>

namespace dckp
{
    /**
     * @brief Deterministic random number generator wrapper for reproducible
     * experiments. Wraps std::mt19937_64 and exposes a small set of helpers.
     */
    class Rng
    {
    public:
        using Seed = std::uint64_t;
        using Engine = std::mt19937_64;

        explicit Rng(Seed seed) noexcept : engine_(seed), seed_(seed) {}

        [[nodiscard]] Seed seed() const noexcept { return seed_; }
        [[nodiscard]] Engine &engine() noexcept { return engine_; }

        [[nodiscard]] std::int64_t uniformInt(std::int64_t lo, std::int64_t hi)
        {
            std::uniform_int_distribution<std::int64_t> dist(lo, hi);
            return dist(engine_);
        }

        [[nodiscard]] double uniformReal(double lo = 0.0, double hi = 1.0)
        {
            std::uniform_real_distribution<double> dist(lo, hi);
            return dist(engine_);
        }

    private:
        Engine engine_;
        Seed seed_;
    };
}
