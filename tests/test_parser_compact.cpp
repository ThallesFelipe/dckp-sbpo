#include "test_support.h"
#include "utils/instance_reader.h"

int main()
{
    // Compact: n=5, declared_conflicts=3, capacity=10
    //          profits     weights         edges (1-based)
    const std::string content =
        "5 3 10\n"
        "7 3 4 8 2\n"
        "2 1 3 5 1\n"
        "1 2  2 3  4 5\n";

    dckp_test::ScopedTempFile tmp("compact", content);

    DCKPInstance inst;
    DCKP_CHECK(inst.read_from_file(tmp.path()));
    DCKP_CHECK_EQ(inst.n_items(), 5);
    DCKP_CHECK_EQ(inst.capacity(), 10);
    DCKP_CHECK_EQ(inst.n_conflicts(), 3U);

    // Verify profits/weights.
    const std::vector<int> expected_profits{7, 3, 4, 8, 2};
    const std::vector<int> expected_weights{2, 1, 3, 5, 1};
    DCKP_CHECK_EQ(inst.profits().size(), 5U);
    for (std::size_t i = 0; i < 5; ++i)
    {
        DCKP_CHECK_EQ(inst.profits()[i], expected_profits[i]);
        DCKP_CHECK_EQ(inst.weights()[i], expected_weights[i]);
    }

    // Edges should be normalized to 0-based: {(0,1),(1,2),(3,4)}.
    DCKP_CHECK(inst.has_conflict(0, 1));
    DCKP_CHECK(inst.has_conflict(1, 2));
    DCKP_CHECK(inst.has_conflict(3, 4));
    DCKP_CHECK(!inst.has_conflict(0, 2));
    DCKP_CHECK(!inst.has_conflict(2, 3));

    return 0;
}
