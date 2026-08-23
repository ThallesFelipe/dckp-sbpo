#include <cctype>
#include <charconv>
#include <chrono>
#include <ctime>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>

#include "utils/instance_reader.h"
#include "utils/solution.h"
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
    [[nodiscard]] std::unique_ptr<dckp::Algorithm> makeAlgorithm(
        std::string_view name, const dckp::ILSConfig &ils_config)
    {
        if (iequals(name, "Greedy_MaxProfit") || iequals(name, "Greedy"))
        {
            return std::make_unique<dckp::GreedyMaxProfit>();
        }
        if (iequals(name, "VND"))
        {
            return std::make_unique<dckp::VND>(ils_config.vnd_config);
        }
        if (iequals(name, "ILS"))
        {
            return std::make_unique<dckp::ILS>(ils_config);
        }
        if (iequals(name, "VNS"))
        {
            return std::make_unique<dckp::VNS>(dckp::VNSConfig{
                .k_max = 3,
                .vnd_config = ils_config.vnd_config,
            });
        }
        return nullptr;
    }

    struct CliOptions
    {
        std::filesystem::path instance_path{};
        std::string algo{"VNS"};
        std::int64_t time_limit_ms{1000000};
        std::uint64_t seed{42};
        dckp::ILSConfig ils_config{};
        bool csv{false};
        bool verbose{false};
        bool help{false};
    };

    [[nodiscard]] bool needsValue(std::string_view flag) noexcept
    {
        return flag == "--algo" || flag == "--time-limit" || flag == "--seed" ||
               flag == "--ils-perturbation-strength" || flag == "--vnd-add" ||
               flag == "--vnd-swap-1-1" || flag == "--vnd-swap-2-1" ||
               flag == "--vnd-swap-1-2";
    }

    template <typename Integer>
    [[nodiscard]] bool parseInteger(const std::string_view text, Integer &value) noexcept
    {
        if (text.empty())
        {
            return false;
        }
        const char *const begin = text.data();
        const char *const end = begin + text.size();
        const auto [position, error] = std::from_chars(begin, end, value);
        return error == std::errc{} && position == end;
    }

    [[nodiscard]] bool parseBinaryFlag(const std::string_view text, bool &value) noexcept
    {
        int parsed{};
        if (!parseInteger(text, parsed) || (parsed != 0 && parsed != 1))
        {
            return false;
        }
        value = parsed == 1;
        return true;
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
            if (arg == "--help" || arg == "-h")
            {
                opts.help = true;
                return true;
            }
            if (arg == "--verbose" || arg == "-v")
            {
                opts.verbose = true;
            }
            else if (arg == "--csv")
            {
                opts.csv = true;
            }
            else if (needsValue(arg))
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
                    if (!parseInteger(value, opts.time_limit_ms) || opts.time_limit_ms <= 0)
                    {
                        std::cerr << "Invalid --time-limit (expected a positive integer): "
                                  << value << '\n';
                        return false;
                    }
                }
                else if (arg == "--seed")
                {
                    if (!parseInteger(value, opts.seed))
                    {
                        std::cerr << "Invalid --seed (expected uint64): " << value << '\n';
                        return false;
                    }
                }
                else if (arg == "--ils-perturbation-strength")
                {
                    if (!parseInteger(value, opts.ils_config.perturbation_strength) ||
                        opts.ils_config.perturbation_strength < 1 ||
                        opts.ils_config.perturbation_strength > 8)
                    {
                        std::cerr << "Invalid --ils-perturbation-strength "
                                     "(expected an integer from 1 to 8): "
                                  << value << '\n';
                        return false;
                    }
                }
                else
                {
                    bool *target{};
                    if (arg == "--vnd-add")
                    {
                        target = &opts.ils_config.vnd_config.enable_add;
                    }
                    else if (arg == "--vnd-swap-1-1")
                    {
                        target = &opts.ils_config.vnd_config.enable_swap_1_1;
                    }
                    else if (arg == "--vnd-swap-2-1")
                    {
                        target = &opts.ils_config.vnd_config.enable_swap_2_1;
                    }
                    else
                    {
                        target = &opts.ils_config.vnd_config.enable_swap_1_2;
                    }
                    if (!parseBinaryFlag(value, *target))
                    {
                        std::cerr << "Invalid " << arg << " (expected 0 or 1): " << value
                                  << '\n';
                        return false;
                    }
                }
            }
            else if (!arg.empty() && arg[0] == '-')
            {
                std::cerr << "Unknown option: " << arg << '\n';
                return false;
            }
            else
            {
                if (!opts.instance_path.empty())
                {
                    std::cerr << "Unexpected extra argument: " << arg << '\n';
                    return false;
                }
                opts.instance_path = std::filesystem::path{arg};
            }
        }
        const auto &vnd = opts.ils_config.vnd_config;
        if (!vnd.enable_add && !vnd.enable_swap_1_1 && !vnd.enable_swap_2_1 &&
            !vnd.enable_swap_1_2)
        {
            std::cerr << "At least one VND neighborhood must be enabled.\n";
            return false;
        }
        return !opts.instance_path.empty();
    }

    /**
     * @brief Prints a one-line usage hint to stderr.
     */
    void printUsage(std::ostream &out)
    {
        out
            << "Usage: dckp_sbpo <instance_path> [--algo NAME] [--time-limit MS] "
               "[--seed N] [--csv] [--verbose]\n"
               "  --algo: Greedy_MaxProfit | VND | ILS | VNS (default: VNS)\n"
               "  --time-limit: milliseconds (default: 1000000)\n"
               "  --seed: uint64 (default: 42)\n"
               "  --ils-perturbation-strength: integer in [1, 8] (default: 4)\n"
               "  --vnd-add: 0 | 1 (default: 1)\n"
               "  --vnd-swap-1-1: 0 | 1 (default: 1)\n"
               "  --vnd-swap-2-1: 0 | 1 (default: 1)\n"
               "  --vnd-swap-1-2: 0 | 1 (default: 1)\n"
               "  --csv: emit a single CSV row without a header\n"
               "  --verbose: emit diagnostics and iteration logs to stderr\n";
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
        printUsage(std::cerr);
        return 2;
    }

    if (opts.help)
    {
        printUsage(std::cout);
        return 0;
    }

    auto algorithm = makeAlgorithm(opts.algo, opts.ils_config);
    if (!algorithm)
    {
        std::cerr << "Unknown --algo: " << opts.algo << '\n';
        printUsage(std::cerr);
        return 2;
    }

    DCKPInstance instance;
    if (!instance.read_from_file(opts.instance_path))
    {
        std::cerr << "Failed to read instance: " << instance.last_error() << '\n';
        return 1;
    }

    if (opts.verbose)
    {
        instance.print(std::cerr);
    }

    dckp::RunnerConfig config;
    config.seed = opts.seed;
    config.time_limit =
        std::chrono::milliseconds{opts.time_limit_ms};
    config.log = opts.verbose ? &std::cerr : nullptr;

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
        std::cerr << solution.toString() << '\n';
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
        return valid ? 0 : 1;
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

    return valid ? 0 : 1;
}
