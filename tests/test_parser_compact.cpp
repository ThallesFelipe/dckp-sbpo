#include "test_support.h"
#include "utils/instance_reader.h"

int main()
{
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

    const std::vector<int> expected_profits{7, 3, 4, 8, 2};
    const std::vector<int> expected_weights{2, 1, 3, 5, 1};
    DCKP_CHECK_EQ(inst.profits().size(), 5U);
    for (std::size_t i = 0; i < 5; ++i)
    {
        DCKP_CHECK_EQ(inst.profits()[i], expected_profits[i]);
        DCKP_CHECK_EQ(inst.weights()[i], expected_weights[i]);
    }

    DCKP_CHECK(inst.has_conflict(0, 1));
    DCKP_CHECK(inst.has_conflict(1, 2));
    DCKP_CHECK(inst.has_conflict(3, 4));
    DCKP_CHECK(!inst.has_conflict(0, 2));
    DCKP_CHECK(!inst.has_conflict(2, 3));

    const std::string signed_content = "+2\v+0\f+5\n+4 +3\r\n+2\t+1\n";
    dckp_test::ScopedTempFile signed_tmp("compact_signed", signed_content);
    DCKP_CHECK(inst.read_from_file(signed_tmp.path()));
    DCKP_CHECK_EQ(inst.n_items(), 2);
    DCKP_CHECK_EQ(inst.capacity(), 5);
    DCKP_CHECK_EQ(inst.n_conflicts(), 0U);
    DCKP_CHECK_EQ(inst.profits()[0], 4);
    DCKP_CHECK_EQ(inst.weights()[1], 1);

    dckp_test::ScopedTempFile invalid_tmp(
        "compact_invalid", "2 0 5\n4 invalid\n2 1\n");
    DCKP_CHECK(!inst.read_from_file(invalid_tmp.path()));

    dckp_test::ScopedTempFile overflow_tmp(
        "compact_overflow", "9223372036854775808 0 5\n");
    DCKP_CHECK(!inst.read_from_file(overflow_tmp.path()));

    return 0;
}
