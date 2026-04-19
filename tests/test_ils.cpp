#include "test_support.h"

#include "utils/instance_reader.h"
#include "utils/solution.h"
#include "utils/validator.h"
#include "algorithms/algorithm.h"
#include "algorithms/rng.h"
#include "algorithms/runner.h"
#include "algorithms/stopping_criteria.h"
#include "constructive/greedy_max_profit.h"
#include "local_search/ils.h"
#include "local_search/vnd.h"

#include <iostream>
#include <string_view>

// --- Instance A --------------------------------------------------------------
// Same 1-2 swap scenario used in the VND suite. Here ILS must at least match
// VND's optimum (115) while remaining feasible and reporting name() == "ILS".
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

// --- Instance B --------------------------------------------------------------
// Zero capacity: ILS must terminate cleanly and return an empty solution.
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

// --- Instance C --------------------------------------------------------------
// Plateau scenario: two feasible packings with equal profit (120). Both are
// VND-local-optima, so the acceptance criterion (local_opt.profit >=
// incumbent.profit, equality included) is the only thing that lets ILS walk
// between them without losing progress.
//
//   capacity = 10; no conflicts; every item is mutually compatible.
//   0: p=60 w=5
//   1: p=60 w=5
//   2: p=50 w=4
//   3: p=50 w=4
//
// Two disjoint 120-profit packings exist: {0,1} and {2,3} (weight 8). Any
// pair {i ∈ {0,1}, j ∈ {2,3}} has profit 110 (weight 9). The singleton {0}
// or {1} has weight 5 and leaves room only for one of {2, 3} (→ p=110) —
// never p=120 — since {0, 1, 2 or 3} is weight 14 > 10.
// On any seed ILS must terminate with profit >= 110 and never lower.
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

// =============================================================================
// Tests
// =============================================================================

static int test_ils_feasible_and_named()
{
    dckp_test::ScopedTempFile tmp("ils_small", kSmallInstance);
    DCKPInstance instance;
    DCKP_CHECK(instance.read_from_file(tmp.path()));

    dckp::RunnerConfig config;
    config.seed = 7;
    config.iteration_limit = 50;
    dckp::Runner runner(instance);

    dckp::ILS ils;
    Solution sol = runner.execute(ils, config);

    Validator validator(instance);
    DCKP_CHECK(validator.validate(sol));
    DCKP_CHECK(sol.methodName() == "ILS");

    // ILS should at least match what VND can reach here (115).
    DCKP_CHECK(sol.totalProfit() >= std::int64_t{115});

    std::cout << "test_ils_feasible_and_named PASSED\n";
    return 0;
}

static int test_ils_never_worse_than_vnd()
{
    dckp_test::ScopedTempFile tmp("ils_not_worse", kSmallInstance);
    DCKPInstance instance;
    DCKP_CHECK(instance.read_from_file(tmp.path()));

    dckp::RunnerConfig config;
    config.seed = 42;
    config.iteration_limit = 200;
    dckp::Runner runner(instance);

    dckp::VND vnd;
    Solution vnd_sol = runner.execute(vnd, config);

    dckp::ILS ils;
    Solution ils_sol = runner.execute(ils, config);

    Validator validator(instance);
    DCKP_CHECK(validator.validate(vnd_sol));
    DCKP_CHECK(validator.validate(ils_sol));

    DCKP_CHECK(ils_sol.totalProfit() >= vnd_sol.totalProfit());

    std::cout << "test_ils_never_worse_than_vnd PASSED\n";
    return 0;
}

static int test_ils_zero_capacity()
{
    dckp_test::ScopedTempFile tmp("ils_zerocap", kZeroCapacityInstance);
    DCKPInstance instance;
    DCKP_CHECK(instance.read_from_file(tmp.path()));

    dckp::RunnerConfig config;
    config.seed = 3;
    config.iteration_limit = 20;
    dckp::Runner runner(instance);

    dckp::ILS ils;
    Solution sol = runner.execute(ils, config);

    Validator validator(instance);
    DCKP_CHECK(validator.validate(sol));
    DCKP_CHECK(sol.empty());
    DCKP_CHECK_EQ(sol.totalProfit(), std::int64_t{0});

    std::cout << "test_ils_zero_capacity PASSED\n";
    return 0;
}

static int test_ils_plateau_acceptance_preserves_best()
{
    dckp_test::ScopedTempFile tmp("ils_plateau", kPlateauInstance);
    DCKPInstance instance;
    DCKP_CHECK(instance.read_from_file(tmp.path()));

    // Many iterations + strong perturbation: even after repeated kicks
    // that may walk incumbent across the equal-profit plateaus {0,1}
    // and {2,3}, `best` must never regress below the global optimum
    // (120) once it has been reached.
    dckp::RunnerConfig config;
    config.seed = 9;
    config.iteration_limit = 300;
    dckp::Runner runner(instance);

    dckp::ILSConfig cfg;
    cfg.perturbation_strength = 2;
    dckp::ILS ils(cfg);

    Solution sol = runner.execute(ils, config);

    Validator validator(instance);
    DCKP_CHECK(validator.validate(sol));
    DCKP_CHECK_EQ(sol.totalProfit(), std::int64_t{120});
    DCKP_CHECK_EQ(sol.size(), std::size_t{2});

    std::cout << "test_ils_plateau_acceptance_preserves_best PASSED\n";
    return 0;
}

static int test_ils_respects_iteration_limit()
{
    dckp_test::ScopedTempFile tmp("ils_iter", kSmallInstance);
    DCKPInstance instance;
    DCKP_CHECK(instance.read_from_file(tmp.path()));

    dckp::RunnerConfig config;
    config.seed = 11;
    config.iteration_limit = 1;
    dckp::Runner runner(instance);

    dckp::ILS ils;
    Solution sol = runner.execute(ils, config);

    Validator validator(instance);
    DCKP_CHECK(validator.validate(sol));

    std::cout << "test_ils_respects_iteration_limit PASSED\n";
    return 0;
}

int main()
{
    int failures = 0;
    failures += test_ils_feasible_and_named();
    failures += test_ils_never_worse_than_vnd();
    failures += test_ils_zero_capacity();
    failures += test_ils_plateau_acceptance_preserves_best();
    failures += test_ils_respects_iteration_limit();

    if (failures != 0)
    {
        std::cerr << failures << " test(s) FAILED\n";
    }
    return failures;
}
