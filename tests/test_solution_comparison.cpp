#include "test_support.h"
#include "utils/instance_reader.h"
#include "utils/solution.h"

#include <set>

namespace
{
    DCKPInstance makeInstance()
    {
        const std::string content =
            "5 2 100\n"
            "10 10 10 10 10\n" // profits: all equal
            "1 2 3 4 5\n"      // weights: distinct
            "1 2  3 4\n";
        dckp_test::ScopedTempFile tmp("cmp", content);
        DCKPInstance inst;
        if (!inst.read_from_file(tmp.path()))
        {
            throw std::runtime_error("parse failed");
        }
        return inst;
    }
}

int main()
{
    DCKPInstance inst = makeInstance();

    // Two solutions with equal total_profit but different item sets must
    // not compare equal (equality is structural, not profit-only).
    Solution a(inst);
    a.addItem(0); // profit 10, weight 1
    a.addItem(3); // profit 10, weight 4  -> (20, 5)

    Solution b(inst);
    b.addItem(1); // profit 10, weight 2
    b.addItem(2); // profit 10, weight 3  -> (20, 5)

    DCKP_CHECK_EQ(a.totalProfit(), b.totalProfit());
    DCKP_CHECK_EQ(a.totalWeight(), b.totalWeight());
    DCKP_CHECK(!(a == b));
    DCKP_CHECK((a < b) || (b < a)); // total ordering must be strict either way.

    // Identical item sets compare equal.
    Solution c(inst);
    c.addItem(0);
    c.addItem(3);
    DCKP_CHECK(a == c);
    DCKP_CHECK(!(a < c));
    DCKP_CHECK(!(c < a));

    // Usable as keys in std::set (exercises strict weak ordering).
    std::set<Solution> pool;
    pool.insert(a);
    pool.insert(b);
    pool.insert(c); // duplicate of a
    DCKP_CHECK_EQ(pool.size(), 2U);

    // Profit-dominance ordering: a solution with strictly higher profit
    // must order greater.
    Solution empty_sol(inst);
    DCKP_CHECK(empty_sol < a);
    DCKP_CHECK(!(a < empty_sol));

    return 0;
}
