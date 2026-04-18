#include "test_support.h"

#include "utils/instance_reader.h"
#include "utils/solution.h"
#include "utils/validator.h"
#include "algorithms/runner.h"
#include "constructive/greedy_max_profit.h"

#include <iostream>
#include <string_view>

// Minimal AMPL-like instance with known optimal greedy behavior.
//   Items: 0(p=10,w=5), 1(p=8,w=3), 2(p=7,w=4), 3(p=6,w=2), 4(p=5,w=6)
//   Capacity: 10
//   Conflicts: (0,1)
//
// Greedy by profit: tries 0(p=10,w=5) → accepted, remaining=5.
// Then 1(p=8,w=3) → forbidden (conflict with 0).
// Then 2(p=7,w=4) → accepted, remaining=1.
// Then 3(p=6,w=2) → w=2 > remaining=1, skip.
// Then 4(p=5,w=6) → w=6 > remaining=1, skip.
// Expected: items {0,2}, profit=17, weight=9.

static constexpr std::string_view kSmallInstance =
    "param n := 5;\n"
    "param c := 10;\n"
    "param : V : p w :=\n"
    "  0  10  5\n"
    "  1   8  3\n"
    "  2   7  4\n"
    "  3   6  2\n"
    "  4   5  6\n"
    ";\n"
    "set E :=\n"
    "  0 1\n"
    ";\n";

// Instance where tie-breaking matters.
//   Items: 0(p=10,w=5), 1(p=10,w=3), 2(p=10,w=4)
//   Capacity: 8
//   No conflicts.
//
// Greedy by profit: all have p=10. Tie-break by weight (ascending).
// Item 1(w=3) → accepted, remaining=5.
// Item 2(w=4) → accepted, remaining=1.
// Item 0(w=5) → w=5 > remaining=1, skip.
// Expected: items {1,2}, profit=20, weight=7.

static constexpr std::string_view kTieBreakInstance =
    "param n := 3;\n"
    "param c := 8;\n"
    "param : V : p w :=\n"
    "  0  10  5\n"
    "  1  10  3\n"
    "  2  10  4\n"
    ";\n"
    "set E :=\n"
    ";\n";

// Edge case: zero-capacity — no item can fit.
static constexpr std::string_view kZeroCapacity =
    "param n := 3;\n"
    "param c := 0;\n"
    "param : V : p w :=\n"
    "  0  10  5\n"
    "  1   8  3\n"
    "  2   7  4\n"
    ";\n"
    "set E :=\n"
    ";\n";

// Dense conflict instance: every item conflicts with every other.
//   Items: 0(p=10,w=1), 1(p=8,w=1), 2(p=6,w=1)
//   Capacity: 100
//   Conflicts: (0,1), (0,2), (1,2)
//
// Greedy picks item 0 only (all others forbidden).
// Expected: items {0}, profit=10, weight=1.

static constexpr std::string_view kCliqueForbidden =
    "param n := 3;\n"
    "param c := 100;\n"
    "param : V : p w :=\n"
    "  0  10  1\n"
    "  1   8  1\n"
    "  2   6  1\n"
    ";\n"
    "set E :=\n"
    "  0 1\n"
    "  0 2\n"
    "  1 2\n"
    ";\n";

static int test_basic_greedy()
{
    dckp_test::ScopedTempFile tmp("greedy_basic", kSmallInstance);
    DCKPInstance instance;
    DCKP_CHECK(instance.read_from_file(tmp.path()));

    dckp::RunnerConfig config;
    config.seed = 42;
    dckp::Runner runner(instance);
    dckp::GreedyMaxProfit greedy;
    Solution sol = runner.execute(greedy, config);

    // Validate feasibility
    Validator validator(instance);
    DCKP_CHECK(validator.validate(sol));

    // Check expected solution
    DCKP_CHECK_EQ(sol.size(), std::size_t{2});
    DCKP_CHECK(sol.hasItem(0));
    DCKP_CHECK(sol.hasItem(2));
    DCKP_CHECK_EQ(sol.totalProfit(), std::int64_t{17});
    DCKP_CHECK_EQ(sol.totalWeight(), std::int64_t{9});
    DCKP_CHECK(sol.methodName() == "Greedy_MaxProfit");

    std::cout << "test_basic_greedy PASSED\n";
    return 0;
}

static int test_tie_break()
{
    dckp_test::ScopedTempFile tmp("greedy_tie", kTieBreakInstance);
    DCKPInstance instance;
    DCKP_CHECK(instance.read_from_file(tmp.path()));

    dckp::RunnerConfig config;
    config.seed = 0;
    dckp::Runner runner(instance);
    dckp::GreedyMaxProfit greedy;
    Solution sol = runner.execute(greedy, config);

    Validator validator(instance);
    DCKP_CHECK(validator.validate(sol));

    // Tie-break by lighter weight: items 1(w=3) and 2(w=4) before 0(w=5)
    DCKP_CHECK_EQ(sol.size(), std::size_t{2});
    DCKP_CHECK(sol.hasItem(1));
    DCKP_CHECK(sol.hasItem(2));
    DCKP_CHECK_EQ(sol.totalProfit(), std::int64_t{20});
    DCKP_CHECK_EQ(sol.totalWeight(), std::int64_t{7});

    std::cout << "test_tie_break PASSED\n";
    return 0;
}

static int test_clique_forbidden()
{
    dckp_test::ScopedTempFile tmp("greedy_clique", kCliqueForbidden);
    DCKPInstance instance;
    DCKP_CHECK(instance.read_from_file(tmp.path()));

    dckp::RunnerConfig config;
    config.seed = 0;
    dckp::Runner runner(instance);
    dckp::GreedyMaxProfit greedy;
    Solution sol = runner.execute(greedy, config);

    Validator validator(instance);
    DCKP_CHECK(validator.validate(sol));

    // Only the best item survives a complete conflict graph.
    DCKP_CHECK_EQ(sol.size(), std::size_t{1});
    DCKP_CHECK(sol.hasItem(0));
    DCKP_CHECK_EQ(sol.totalProfit(), std::int64_t{10});

    std::cout << "test_clique_forbidden PASSED\n";
    return 0;
}

static int test_zero_capacity()
{
    dckp_test::ScopedTempFile tmp("greedy_zerocap", kZeroCapacity);
    DCKPInstance instance;
    DCKP_CHECK(instance.read_from_file(tmp.path()));

    dckp::RunnerConfig config;
    config.seed = 0;
    dckp::Runner runner(instance);
    dckp::GreedyMaxProfit greedy;
    Solution sol = runner.execute(greedy, config);

    Validator validator(instance);
    DCKP_CHECK(validator.validate(sol));
    DCKP_CHECK(sol.empty());
    DCKP_CHECK_EQ(sol.totalProfit(), std::int64_t{0});

    std::cout << "test_zero_capacity PASSED\n";
    return 0;
}

int main()
{
    int failures = 0;
    failures += test_basic_greedy();
    failures += test_tie_break();
    failures += test_clique_forbidden();
    failures += test_zero_capacity();

    if (failures != 0)
    {
        std::cerr << failures << " test(s) FAILED\n";
    }
    return failures;
}
