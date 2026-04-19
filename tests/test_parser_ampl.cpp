#include "test_support.h"
#include "utils/instance_reader.h"

int main()
{
    const std::string content =
        "   param   n   :=   4 ;\n"
        "\tparam   c :=   20;\n"
        "\n"
        "param  :  V  :  p  w  :=\n"
        "   0   5   2\n"
        "   1   6   4\n"
        "   2   7   6\n"
        "   3   8   8 ;\n"
        "\n"
        "set  E :=\n"
        "   0 1\n"
        "   2 3 ;\n";

    dckp_test::ScopedTempFile tmp("ampl", content);

    DCKPInstance inst;
    if (!inst.read_from_file(tmp.path()))
    {
        std::cerr << "Parser failed: " << inst.last_error() << '\n';
        return 1;
    }

    DCKP_CHECK_EQ(inst.n_items(), 4);
    DCKP_CHECK_EQ(inst.capacity(), 20);
    DCKP_CHECK_EQ(inst.n_conflicts(), 2U);

    DCKP_CHECK_EQ(inst.profits()[0], 5);
    DCKP_CHECK_EQ(inst.profits()[3], 8);
    DCKP_CHECK_EQ(inst.weights()[1], 4);
    DCKP_CHECK_EQ(inst.weights()[2], 6);

    DCKP_CHECK(inst.has_conflict(0, 1));
    DCKP_CHECK(inst.has_conflict(2, 3));
    DCKP_CHECK(!inst.has_conflict(1, 2));

    return 0;
}
