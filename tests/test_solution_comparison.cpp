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
            "10 10 10 10 10\n"
            "1 2 3 4 5\n"
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

    Solution a(inst);
    a.addItem(0);
    a.addItem(3);

    Solution b(inst);
    b.addItem(1);
    b.addItem(2);

    DCKP_CHECK_EQ(a.totalProfit(), b.totalProfit());
    DCKP_CHECK_EQ(a.totalWeight(), b.totalWeight());
    DCKP_CHECK(!(a == b));
    DCKP_CHECK((a < b) || (b < a));

    Solution c(inst);
    c.addItem(0);
    c.addItem(3);
    DCKP_CHECK(a == c);
    DCKP_CHECK(!(a < c));
    DCKP_CHECK(!(c < a));

    std::set<Solution> pool;
    pool.insert(a);
    pool.insert(b);
    pool.insert(c);
    DCKP_CHECK_EQ(pool.size(), 2U);

    Solution empty_sol(inst);
    DCKP_CHECK(empty_sol < a);
    DCKP_CHECK(!(a < empty_sol));

    return 0;
}
