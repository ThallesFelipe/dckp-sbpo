#include <filesystem>
#include <iostream>

#include "utils/instance_reader.h"
#include "utils/validator.h"
#include "algorithms/runner.h"
#include "constructive/greedy_max_profit.h"

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

    dckp::GreedyMaxProfit greedy;
    Solution sol = runner.execute(greedy, config);

    Validator validator(instance);
    std::cout << '\n'
              << validator.validateDetailed(sol) << '\n';

    sol.print();

    std::cout << "\nCSV_OUTPUT,"
              << instance_path.filename().string() << ','
              << sol.methodName() << ','
              << sol.totalProfit() << ','
              << sol.computationTime() << '\n';

    return 0;
}
