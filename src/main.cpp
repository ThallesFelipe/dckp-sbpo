#include <chrono>
#include <filesystem>
#include <iostream>

#include "utils/instance_reader.h"
#include "utils/validator.h"
#include "algorithms/runner.h"
#include "constructive/greedy_max_profit.h"
#include "local_search/ils.h"
#include "local_search/vnd.h"

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        std::cerr << "Usage: dckp_sbpo <instance_path>\n";
        return 2;
    }

    const std::filesystem::path instance_path{argv[1]};

    DCKPInstance instance;
    if (!instance.read_from_file(instance_path))
    {
        std::cerr << "Failed to read instance: " << instance.last_error() << '\n';
        return 1;
    }

    instance.print();

    dckp::RunnerConfig config;
    config.seed = 42;
    dckp::Runner runner(instance);
    Validator validator(instance);

    dckp::GreedyMaxProfit greedy;
    Solution greedy_sol = runner.execute(greedy, config);
    std::cout << '\n'
              << validator.validateDetailed(greedy_sol) << '\n';
    greedy_sol.print();

    dckp::VND vnd;
    Solution vnd_sol = runner.execute(vnd, config);
    std::cout << '\n'
              << validator.validateDetailed(vnd_sol) << '\n';
    vnd_sol.print();

    dckp::RunnerConfig ils_config = config;
    ils_config.time_limit = std::chrono::milliseconds{2000};
    dckp::ILS ils;
    Solution ils_sol = runner.execute(ils, ils_config);
    std::cout << '\n'
              << validator.validateDetailed(ils_sol) << '\n';
    ils_sol.print();

    std::cout << "\nCSV_OUTPUT,"
              << instance_path.filename().string() << ','
              << greedy_sol.methodName() << ','
              << greedy_sol.totalProfit() << ','
              << greedy_sol.computationTime() << '\n';

    std::cout << "CSV_OUTPUT,"
              << instance_path.filename().string() << ','
              << vnd_sol.methodName() << ','
              << vnd_sol.totalProfit() << ','
              << vnd_sol.computationTime() << '\n';

    std::cout << "CSV_OUTPUT,"
              << instance_path.filename().string() << ','
              << ils_sol.methodName() << ','
              << ils_sol.totalProfit() << ','
              << ils_sol.computationTime() << '\n';

    return 0;
}
