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

#include <iostream>
#include <string_view>

namespace
{
    // Helper — construct a fresh RunContext for direct VND::improve tests.
    struct ScopedContext
    {
        dckp::Rng rng{0};
        dckp::StoppingCriteria stopping{};
        dckp::RunContext ctx{rng, stopping};

        explicit ScopedContext(dckp::Rng::Seed seed = 0) : rng(seed) { stopping.start(); }
    };
}

// --- Instance 1 --------------------------------------------------------------
// A canonical 1-2 swap scenario: greedy locks in item 0 because it has the
// highest profit, blocking the strictly better pair {1, 2}.
//   0: p=100, w=10       (no conflicts, fits alone)
//   1: p=60,  w=5
//   2: p=55,  w=5
//   capacity = 10        no conflicts
//
// Greedy → {0}, profit=100.
// VND N1/N2/N3: no improving move. N4: remove 0, add (1, 2) — p=115 > 100,
// pair weight = 10, no conflict. Resulting profit=115.
static constexpr std::string_view kSwap12Instance =
    "param n := 3;\n"
    "param c := 10;\n"
    "param : V : p w :=\n"
    "  0  100  10\n"
    "  1   60   5\n"
    "  2   55   5\n"
    ";\n"
    "set E :=\n"
    ";\n";

// --- Instance 2 --------------------------------------------------------------
// A 1-1 swap scenario: we hand VND a seed that placed the low-profit item 0
// instead of the better (but conflicting) item 1.
//   0: p=5,  w=3   conflict with 1
//   1: p=10, w=3   conflict with 0
//   2: p=20, w=3
//   capacity = 6
//
// Seed = {0, 2}, profit=25. N2 removes 0, adds 1 → {1, 2}, profit=30.
static constexpr std::string_view kSwap11Instance =
    "param n := 3;\n"
    "param c := 6;\n"
    "param : V : p w :=\n"
    "  0   5  3\n"
    "  1  10  3\n"
    "  2  20  3\n"
    ";\n"
    "set E :=\n"
    "  0 1\n"
    ";\n";

// --- Instance 3 --------------------------------------------------------------
// A 2-1 swap scenario: two small items block a single high-profit item that
// conflicts with both. Starting from the pair, only N3 can escape.
//   0: p=5, w=3   conflict with 2
//   1: p=5, w=3   conflict with 2
//   2: p=20, w=5  conflicts with 0 and 1
//   3: p=1, w=1
//   capacity = 6
//
// Seed = {0, 1}, profit=10. N3 removes {0,1}, adds 2 → profit=20.
// Then N1 adds 3 → profit=21.
static constexpr std::string_view kSwap21Instance =
    "param n := 4;\n"
    "param c := 6;\n"
    "param : V : p w :=\n"
    "  0   5  3\n"
    "  1   5  3\n"
    "  2  20  5\n"
    "  3   1  1\n"
    ";\n"
    "set E :=\n"
    "  0 2\n"
    "  1 2\n"
    ";\n";

// --- Instance 4 --------------------------------------------------------------
// Empty starting solution on a small feasible instance — exercises N1 (Add)
// only, from nothing to a feasible packing.
//   0: p=4, w=2
//   1: p=5, w=2
//   2: p=3, w=2
//   capacity = 4     no conflicts
//
// N1 greedily adds the first positive-profit feasible item in ItemId order.
// Starting from empty: adds 0 (w=2, remaining=2), then 1 (w=2, remaining=0).
// Item 2 no longer fits. Final profit = 9.
static constexpr std::string_view kAddFromEmptyInstance =
    "param n := 3;\n"
    "param c := 4;\n"
    "param : V : p w :=\n"
    "  0  4  2\n"
    "  1  5  2\n"
    "  2  3  2\n"
    ";\n"
    "set E :=\n"
    ";\n";

// --- Instance 5 --------------------------------------------------------------
// Zero capacity — no item can fit, VND must return an empty feasible solution.
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

// =============================================================================
// Tests
// =============================================================================

static int test_run_never_worse_than_greedy()
{
    dckp_test::ScopedTempFile tmp("vnd_pipeline", kSwap12Instance);
    DCKPInstance instance;
    DCKP_CHECK(instance.read_from_file(tmp.path()));

    dckp::RunnerConfig config;
    config.seed = 7;
    dckp::Runner runner(instance);

    dckp::GreedyMaxProfit greedy;
    Solution greedy_sol = runner.execute(greedy, config);

    dckp::VND vnd;
    Solution vnd_sol = runner.execute(vnd, config);

    Validator validator(instance);
    DCKP_CHECK(validator.validate(greedy_sol));
    DCKP_CHECK(validator.validate(vnd_sol));

    // VND must never degrade the seed's profit.
    DCKP_CHECK(vnd_sol.totalProfit() >= greedy_sol.totalProfit());
    DCKP_CHECK(vnd_sol.methodName() == "VND");

    // On this specific instance VND strictly improves via N4.
    DCKP_CHECK_EQ(greedy_sol.totalProfit(), std::int64_t{100});
    DCKP_CHECK_EQ(vnd_sol.totalProfit(), std::int64_t{115});
    DCKP_CHECK(vnd_sol.hasItem(1));
    DCKP_CHECK(vnd_sol.hasItem(2));
    DCKP_CHECK(!vnd_sol.hasItem(0));

    std::cout << "test_run_never_worse_than_greedy PASSED\n";
    return 0;
}

static int test_swap_1_1_improves_seed()
{
    dckp_test::ScopedTempFile tmp("vnd_swap11", kSwap11Instance);
    DCKPInstance instance;
    DCKP_CHECK(instance.read_from_file(tmp.path()));

    Solution seed(instance);
    DCKP_CHECK(seed.addItem(0));
    DCKP_CHECK(seed.addItem(2));
    DCKP_CHECK_EQ(seed.totalProfit(), std::int64_t{25});

    ScopedContext sc;
    dckp::VND vnd;
    Solution improved = vnd.improve(seed, sc.ctx);

    Validator validator(instance);
    DCKP_CHECK(validator.validate(improved));

    DCKP_CHECK_EQ(improved.totalProfit(), std::int64_t{30});
    DCKP_CHECK(improved.hasItem(1));
    DCKP_CHECK(improved.hasItem(2));
    DCKP_CHECK(!improved.hasItem(0));

    std::cout << "test_swap_1_1_improves_seed PASSED\n";
    return 0;
}

static int test_swap_2_1_improves_seed()
{
    dckp_test::ScopedTempFile tmp("vnd_swap21", kSwap21Instance);
    DCKPInstance instance;
    DCKP_CHECK(instance.read_from_file(tmp.path()));

    Solution seed(instance);
    DCKP_CHECK(seed.addItem(0));
    DCKP_CHECK(seed.addItem(1));
    DCKP_CHECK_EQ(seed.totalProfit(), std::int64_t{10});

    ScopedContext sc;
    dckp::VND vnd;
    Solution improved = vnd.improve(seed, sc.ctx);

    Validator validator(instance);
    DCKP_CHECK(validator.validate(improved));

    // N3 replaces {0,1} (p=10) with {2} (p=20), then N1 adds {3} (p=1).
    DCKP_CHECK_EQ(improved.totalProfit(), std::int64_t{21});
    DCKP_CHECK(improved.hasItem(2));
    DCKP_CHECK(improved.hasItem(3));
    DCKP_CHECK(!improved.hasItem(0));
    DCKP_CHECK(!improved.hasItem(1));

    std::cout << "test_swap_2_1_improves_seed PASSED\n";
    return 0;
}

static int test_swap_1_2_improves_seed()
{
    dckp_test::ScopedTempFile tmp("vnd_swap12", kSwap12Instance);
    DCKPInstance instance;
    DCKP_CHECK(instance.read_from_file(tmp.path()));

    Solution seed(instance);
    DCKP_CHECK(seed.addItem(0));
    DCKP_CHECK_EQ(seed.totalProfit(), std::int64_t{100});

    ScopedContext sc;
    dckp::VND vnd;
    Solution improved = vnd.improve(seed, sc.ctx);

    Validator validator(instance);
    DCKP_CHECK(validator.validate(improved));

    DCKP_CHECK_EQ(improved.totalProfit(), std::int64_t{115});
    DCKP_CHECK(improved.hasItem(1));
    DCKP_CHECK(improved.hasItem(2));
    DCKP_CHECK(!improved.hasItem(0));

    std::cout << "test_swap_1_2_improves_seed PASSED\n";
    return 0;
}

static int test_add_from_empty()
{
    dckp_test::ScopedTempFile tmp("vnd_add_empty", kAddFromEmptyInstance);
    DCKPInstance instance;
    DCKP_CHECK(instance.read_from_file(tmp.path()));

    Solution seed(instance);  // empty
    DCKP_CHECK(seed.empty());

    ScopedContext sc;
    dckp::VND vnd;
    Solution improved = vnd.improve(seed, sc.ctx);

    Validator validator(instance);
    DCKP_CHECK(validator.validate(improved));

    // N1 picks 0 then 1 in ItemId order; 2 no longer fits.
    DCKP_CHECK_EQ(improved.totalProfit(), std::int64_t{9});
    DCKP_CHECK_EQ(improved.size(), std::size_t{2});
    DCKP_CHECK(improved.hasItem(0));
    DCKP_CHECK(improved.hasItem(1));

    std::cout << "test_add_from_empty PASSED\n";
    return 0;
}

static int test_zero_capacity()
{
    dckp_test::ScopedTempFile tmp("vnd_zerocap", kZeroCapacityInstance);
    DCKPInstance instance;
    DCKP_CHECK(instance.read_from_file(tmp.path()));

    dckp::RunnerConfig config;
    config.seed = 3;
    dckp::Runner runner(instance);

    dckp::VND vnd;
    Solution sol = runner.execute(vnd, config);

    Validator validator(instance);
    DCKP_CHECK(validator.validate(sol));
    DCKP_CHECK(sol.empty());
    DCKP_CHECK_EQ(sol.totalProfit(), std::int64_t{0});

    std::cout << "test_zero_capacity PASSED\n";
    return 0;
}

static int test_disabled_neighborhoods_are_no_op()
{
    dckp_test::ScopedTempFile tmp("vnd_disabled", kSwap11Instance);
    DCKPInstance instance;
    DCKP_CHECK(instance.read_from_file(tmp.path()));

    Solution seed(instance);
    DCKP_CHECK(seed.addItem(0));
    DCKP_CHECK(seed.addItem(2));

    ScopedContext sc;
    dckp::VNDConfig cfg;
    cfg.enable_add = false;
    cfg.enable_swap_1_1 = false;
    cfg.enable_swap_2_1 = false;
    cfg.enable_swap_1_2 = false;
    dckp::VND vnd(cfg);
    Solution result = vnd.improve(seed, sc.ctx);

    // With every neighborhood disabled, VND must return the seed unchanged.
    DCKP_CHECK_EQ(result.totalProfit(), seed.totalProfit());
    DCKP_CHECK_EQ(result.size(), seed.size());
    DCKP_CHECK(result.hasItem(0));
    DCKP_CHECK(result.hasItem(2));

    std::cout << "test_disabled_neighborhoods_are_no_op PASSED\n";
    return 0;
}

static int test_respects_iteration_limit()
{
    dckp_test::ScopedTempFile tmp("vnd_iterlimit", kSwap11Instance);
    DCKPInstance instance;
    DCKP_CHECK(instance.read_from_file(tmp.path()));

    // iteration_limit=1 means the very first outer iteration trips stopping
    // before the next neighborhood can be explored.
    dckp::RunnerConfig config;
    config.seed = 11;
    config.iteration_limit = 1;
    dckp::Runner runner(instance);

    dckp::VND vnd;
    Solution sol = runner.execute(vnd, config);

    Validator validator(instance);
    DCKP_CHECK(validator.validate(sol));
    // We don't assert on the exact profit — only that VND terminated cleanly
    // while producing a feasible solution.

    std::cout << "test_respects_iteration_limit PASSED\n";
    return 0;
}

static int test_injected_seed_algorithm_is_used()
{
    // Minimal Algorithm that returns a known, non-trivial hand-picked seed.
    // Lets us verify that VND uses the injected seed instead of the default
    // GreedyMaxProfit, and still improves it through its neighborhoods.
    class FixedSeed : public dckp::Algorithm
    {
    public:
        [[nodiscard]] Solution run(const DCKPInstance &instance,
                                   dckp::RunContext & /*ctx*/) override
        {
            Solution s(instance);
            s.addItem(0);
            s.addItem(2);
            s.setMethodName("FixedSeed");
            return s;
        }

        [[nodiscard]] std::string name() const override { return "FixedSeed"; }
    };

    dckp_test::ScopedTempFile tmp("vnd_injected", kSwap11Instance);
    DCKPInstance instance;
    DCKP_CHECK(instance.read_from_file(tmp.path()));

    dckp::VNDConfig cfg;
    auto seed_alg = std::make_unique<FixedSeed>();
    dckp::VND vnd(cfg, std::move(seed_alg));

    dckp::RunnerConfig config;
    config.seed = 0;
    dckp::Runner runner(instance);
    Solution sol = runner.execute(vnd, config);

    Validator validator(instance);
    DCKP_CHECK(validator.validate(sol));

    // FixedSeed produces {0,2} (profit 25). VND N2 upgrades to {1,2} (profit 30).
    DCKP_CHECK_EQ(sol.totalProfit(), std::int64_t{30});
    DCKP_CHECK(sol.methodName() == "VND");

    std::cout << "test_injected_seed_algorithm_is_used PASSED\n";
    return 0;
}

int main()
{
    int failures = 0;
    failures += test_run_never_worse_than_greedy();
    failures += test_swap_1_1_improves_seed();
    failures += test_swap_2_1_improves_seed();
    failures += test_swap_1_2_improves_seed();
    failures += test_add_from_empty();
    failures += test_zero_capacity();
    failures += test_disabled_neighborhoods_are_no_op();
    failures += test_respects_iteration_limit();
    failures += test_injected_seed_algorithm_is_used();

    if (failures != 0)
    {
        std::cerr << failures << " test(s) FAILED\n";
    }
    return failures;
}
