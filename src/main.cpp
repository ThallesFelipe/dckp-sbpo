#include <algorithm>
#include <cctype>
#include <chrono>
#include <ctime>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>

#include "utils/instance_reader.h"
#include "utils/validator.h"
#include "algorithms/algorithm.h"
#include "algorithms/runner.h"
#include "constructive/greedy_max_profit.h"
#include "local_search/ils.h"
#include "local_search/vnd.h"
#include "local_search/vns.h"

namespace
{
    /**
     * @brief Case-insensitive comparison of two ASCII strings — used to
     * accept algorithm names regardless of capitalization on the CLI.
     */
    [[nodiscard]] bool iequals(std::string_view a, std::string_view b) noexcept
    {
        if (a.size() != b.size())
        {
            return false;
        }
        for (std::size_t i = 0; i < a.size(); ++i)
        {
            const auto ca = static_cast<unsigned char>(a[i]);
            const auto cb = static_cast<unsigned char>(b[i]);
            if (std::tolower(ca) != std::tolower(cb))
            {
                return false;
            }
        }
        return true;
    }

    /**
     * @brief Minimal algorithm factory — stays intentionally local to
     * main.cpp so that Runner::execute() does not have to become a
     * registry. Returns @c nullptr on an unknown name.
     */
    [[nodiscard]] std::unique_ptr<dckp::Algorithm> makeAlgorithm(std::string_view name)
    {
        if (iequals(name, "Greedy_MaxProfit") || iequals(name, "Greedy"))
        {
            return std::make_unique<dckp::GreedyMaxProfit>();
        }
        if (iequals(name, "VND"))
        {
            return std::make_unique<dckp::VND>();
        }
        if (iequals(name, "ILS"))
        {
            return std::make_unique<dckp::ILS>();
        }
        if (iequals(name, "VNS"))
        {
            return std::make_unique<dckp::VNS>();
        }
        return nullptr;
    }

    struct CliOptions
    {
        std::filesystem::path instance_path{};
        std::string algo{"VNS"};
        std::int64_t time_limit_ms{600000};
        std::uint64_t seed{42};
        bool csv{false};
        bool verbose{false};
    };

    [[nodiscard]] bool needsValue(std::string_view flag) noexcept
    {
        return flag == "--algo" || flag == "--time-limit" || flag == "--seed";
    }

    /**
     * @brief Parses argv into @p opts. On error prints a usage hint to
     * stderr and returns false. The binary's single positional argument
     * is the instance path.
     */
    [[nodiscard]] bool parseCli(int argc, char *argv[], CliOptions &opts)
    {
        for (int i = 1; i < argc; ++i)
        {
            std::string_view arg{argv[i]};
            if (arg == "--verbose" || arg == "-v")
            {
                opts.verbose = true;
                continue;
            }
            if (arg == "--csv")
            {
                opts.csv = true;
                continue;
            }
            if (needsValue(arg))
            {
                if (i + 1 >= argc)
                {
                    std::cerr << "Missing value for " << arg << '\n';
                    return false;
                }
                const std::string_view value{argv[++i]};
                if (arg == "--algo")
                {
                    opts.algo = std::string{value};
                }
                else if (arg == "--time-limit")
                {
                    try
                    {
                        opts.time_limit_ms = std::stoll(std::string{value});
                    }
                    catch (...)
                    {
                        std::cerr << "Invalid --time-limit: " << value << '\n';
                        return false;
                    }
                }
                else if (arg == "--seed")
                {
                    try
                    {
                        opts.seed = std::stoull(std::string{value});
                    }
                    catch (...)
                    {
                        std::cerr << "Invalid --seed: " << value << '\n';
                        return false;
                    }
                }
                continue;
            }
            if (!arg.empty() && arg[0] == '-')
            {
                std::cerr << "Unknown option: " << arg << '\n';
                return false;
            }
            if (!opts.instance_path.empty())
            {
                std::cerr << "Unexpected extra argument: " << arg << '\n';
                return false;
            }
            opts.instance_path = std::filesystem::path{arg};
        }
        return !opts.instance_path.empty();
    }

    /**
     * @brief Prints a one-line usage hint to stderr.
     */
    void printUsage()
    {
        std::cerr
            << "Usage: dckp_sbpo <instance_path> [--algo NAME] [--time-limit MS] "
               "[--seed N] [--csv] [--verbose]\n"
               "  --algo: Greedy_MaxProfit | VND | ILS | VNS (default: VNS)\n"
               "  --time-limit: milliseconds (default: 600000)\n"
               "  --seed: uint64 (default: 42)\n"
               "  --csv: emit a single CSV row without a header\n";
    }

    /**
     * @brief Strips the trailing extension from @p path's filename, so
     * "20I5.txt" and "1I1" both map to a clean identifier for the CSV.
     */
    [[nodiscard]] std::string instanceBaseName(const std::filesystem::path &path)
    {
        return path.stem().string();
    }

    [[nodiscard]] std::string formatTimestamp(
        const std::chrono::system_clock::time_point time_point)
    {
        const auto milliseconds =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                time_point.time_since_epoch()) %
            1000;

        const std::time_t time = std::chrono::system_clock::to_time_t(time_point);
        std::tm local_time{};
        localtime_r(&time, &local_time);

        std::ostringstream out;
        out << std::put_time(&local_time, "%Y-%m-%d %H:%M:%S")
            << '.' << std::setw(3) << std::setfill('0') << milliseconds.count();
        return out.str();
    }
}

int main(int argc, char *argv[])
{
    CliOptions opts;
    if (!parseCli(argc, argv, opts))
    {
        printUsage();
        return 2;
    }

    DCKPInstance instance;
    if (!instance.read_from_file(opts.instance_path))
    {
        std::cerr << "Failed to read instance: " << instance.last_error() << '\n';
        return 1;
    }

    auto algorithm = makeAlgorithm(opts.algo);
    if (!algorithm)
    {
        std::cerr << "Unknown --algo: " << opts.algo << '\n';
        printUsage();
        return 2;
    }

    if (opts.verbose)
    {
        instance.print();
    }

    dckp::RunnerConfig config;
    config.seed = opts.seed;
    config.time_limit =
        std::chrono::milliseconds{std::max<std::int64_t>(0, opts.time_limit_ms)};

    dckp::Runner runner(instance);
    const auto steady_start = std::chrono::steady_clock::now();
    const auto system_start = std::chrono::system_clock::now();
    Solution solution = runner.execute(*algorithm, config);
    const auto system_end = std::chrono::system_clock::now();
    const auto steady_end = std::chrono::steady_clock::now();

    Validator validator(instance);
    const bool valid = validator.validate(solution);

    if (opts.verbose)
    {
        std::cerr << validator.validateDetailed(solution) << '\n';
        solution.print();
    }

    const auto time_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            steady_end - steady_start)
            .count();
    const std::string start_time = formatTimestamp(system_start);
    const std::string end_time = formatTimestamp(system_end);

    if (opts.csv)
    {
        std::cout << instanceBaseName(opts.instance_path)
                  << ',' << opts.seed
                  << ',' << solution.totalProfit()
                  << ',' << solution.totalWeight()
                  << ',' << instance.capacity()
                  << ',' << start_time
                  << ',' << end_time
                  << ',' << time_ms
                  << ',' << (valid ? "true" : "false")
                  << ',' << solution.methodName()
                  << '\n';
        return 0;
    }

    std::cout << "instance=" << instanceBaseName(opts.instance_path)
              << " algorithm=" << solution.methodName()
              << " profit=" << solution.totalProfit()
              << " weight=" << solution.totalWeight()
              << " capacity=" << instance.capacity()
              << " start_time=\"" << start_time << '"'
              << " end_time=\"" << end_time << '"'
              << " time_ms=" << time_ms
              << " valid=" << (valid ? "true" : "false")
              << '\n';

    return 0;
}
