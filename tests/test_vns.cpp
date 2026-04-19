#include "test_support.h"

#include "utils/instance_reader.h"
#include "utils/solution.h"
#include "utils/validator.h"
#include "algorithms/algorithm.h"
#include "algorithms/rng.h"
#include "algorithms/runner.h"
#include "algorithms/stopping_criteria.h"
#include "constructive/greedy_max_profit.h"
#include "local_search/vnd.h"
#include "local_search/vns.h"

#include <iostream>
#include <string_view>
static constexpr std::string_view kSmallInstance =
    "param n := 3;\n"
    "param c := 10;\n"
    "param : V : p w :=\n"
    "  0  100  10\n"
    "  1   60   5\n"
    "  2   55   5\n"
    ";\n"
    "set E :=\n"
    ";\n";

static constexpr std::string_view kZeroCapacityInstance =
    "param n := 3;\n"
    "param c := 0;\n"
    "param : V : p w :=\n"
    "  0  10  5\n"
    "  1   8  3\n"
    "  2   7  4\n"
    ";\n"
    "set E :=\n"
    ";\n";

static constexpr std::string_view kPlateauInstance =
    "param n := 4;\n"
    "param c := 10;\n"
    "param : V : p w :=\n"
    "  0  60  5\n"
    "  1  60  5\n"
    "  2  50  4\n"
    "  3  50  4\n"
    ";\n"
    "set E :=\n"
    ";\n";

static int test_vns_feasible_and_named()
{
    dckp_test::ScopedTempFile tmp("vns_small", kSmallInstance);
    DCKPInstance instance;
    DCKP_CHECK(instance.read_from_file(tmp.path()));

    dckp::RunnerConfig config;
    config.seed = 7;
    config.iteration_limit = 50;
    dckp::Runner runner(instance);

    dckp::VNS vns;
    Solution sol = runner.execute(vns, config);

    Validator validator(instance);
    DCKP_CHECK(validator.validate(sol));
    DCKP_CHECK(sol.methodName() == "VNS");

    DCKP_CHECK(sol.totalProfit() >= std::int64_t{115});

    std::cout << "test_vns_feasible_and_named PASSED\n";
    return 0;
}

static int test_vns_never_worse_than_vnd()
{
    dckp_test::ScopedTempFile tmp("vns_not_worse", kSmallInstance);
    DCKPInstance instance;
    DCKP_CHECK(instance.read_from_file(tmp.path()));

    dckp::RunnerConfig config;
    config.seed = 42;
    config.iteration_limit = 200;
    dckp::Runner runner(instance);

    dckp::VND vnd;
    Solution vnd_sol = runner.execute(vnd, config);

    dckp::VNS vns;
    Solution vns_sol = runner.execute(vns, config);

    Validator validator(instance);
    DCKP_CHECK(validator.validate(vnd_sol));
    DCKP_CHECK(validator.validate(vns_sol));

    DCKP_CHECK(vns_sol.totalProfit() >= vnd_sol.totalProfit());

    std::cout << "test_vns_never_worse_than_vnd PASSED\n";
    return 0;
}

static int test_vns_k_max_zero_is_clamped()
{
    dckp_test::ScopedTempFile tmp("vns_kmax_zero", kSmallInstance);
    DCKPInstance instance;
    DCKP_CHECK(instance.read_from_file(tmp.path()));

    dckp::VNSConfig cfg;
    cfg.k_max = 0;
    dckp::VNS vns(cfg);

    dckp::RunnerConfig config;
    config.seed = 5;
    config.iteration_limit = 50;
    dckp::Runner runner(instance);

    Solution sol = runner.execute(vns, config);

    Validator validator(instance);
    DCKP_CHECK(validator.validate(sol));
    DCKP_CHECK(sol.totalProfit() >= std::int64_t{115});

    std::cout << "test_vns_k_max_zero_is_clamped PASSED\n";
    return 0;
}

static int test_vns_zero_capacity()
{
    dckp_test::ScopedTempFile tmp("vns_zerocap", kZeroCapacityInstance);
    DCKPInstance instance;
    DCKP_CHECK(instance.read_from_file(tmp.path()));

    dckp::RunnerConfig config;
    config.seed = 3;
    config.iteration_limit = 20;
    dckp::Runner runner(instance);

    dckp::VNS vns;
    Solution sol = runner.execute(vns, config);

    Validator validator(instance);
    DCKP_CHECK(validator.validate(sol));
    DCKP_CHECK(sol.empty());
    DCKP_CHECK_EQ(sol.totalProfit(), std::int64_t{0});

    std::cout << "test_vns_zero_capacity PASSED\n";
    return 0;
}

static int test_vns_respects_small_iteration_limit()
{
    dckp_test::ScopedTempFile tmp("vns_iter", kSmallInstance);
    DCKPInstance instance;
    DCKP_CHECK(instance.read_from_file(tmp.path()));

    dckp::RunnerConfig config;
    config.seed = 11;
    config.iteration_limit = 1;
    dckp::Runner runner(instance);

    dckp::VNS vns;
    Solution sol = runner.execute(vns, config);

    Validator validator(instance);
    DCKP_CHECK(validator.validate(sol));

    std::cout << "test_vns_respects_small_iteration_limit PASSED\n";
    return 0;
}

static int test_vns_shake_preserves_feasibility_plateau()
{
    dckp_test::ScopedTempFile tmp("vns_plateau", kPlateauInstance);
    DCKPInstance instance;
    DCKP_CHECK(instance.read_from_file(tmp.path()));

    dckp::RunnerConfig config;
    config.seed = 9;
    config.iteration_limit = 300;
    dckp::Runner runner(instance);

    dckp::VNSConfig cfg;
    cfg.k_max = 3;
    dckp::VNS vns(cfg);

    Solution sol = runner.execute(vns, config);

    Validator validator(instance);
    DCKP_CHECK(validator.validate(sol));
    DCKP_CHECK_EQ(sol.totalProfit(), std::int64_t{120});
    DCKP_CHECK_EQ(sol.size(), std::size_t{2});

    std::cout << "test_vns_shake_preserves_feasibility_plateau PASSED\n";
    return 0;
}

int main()
{
    int failures = 0;
    failures += test_vns_feasible_and_named();
    failures += test_vns_never_worse_than_vnd();
    failures += test_vns_k_max_zero_is_clamped();
    failures += test_vns_zero_capacity();
    failures += test_vns_respects_small_iteration_limit();
    failures += test_vns_shake_preserves_feasibility_plateau();

    if (failures != 0)
    {
        std::cerr << failures << " test(s) FAILED\n";
    }
    return failures;
}
