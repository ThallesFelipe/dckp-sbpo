#include "test_support.h"
#include "utils/instance_reader.h"

namespace
{
    bool has_expected_edges(const DCKPInstance &inst)
    {
        if (inst.n_conflicts() != 3U)
        {
            return false;
        }
        return inst.has_conflict(0, 1) && inst.has_conflict(1, 2) && inst.has_conflict(3, 4);
    }
}

int main()
{
    const std::string one_based =
        "5 3 10\n"
        "7 3 4 8 2\n"
        "2 1 3 5 1\n"
        "1 2  2 3  4 5\n";

    const std::string zero_based =
        "5 3 10\n"
        "7 3 4 8 2\n"
        "2 1 3 5 1\n"
        "0 1  1 2  3 4\n";

    {
        dckp_test::ScopedTempFile tmp("norm_one", one_based);
        DCKPInstance inst;
        DCKP_CHECK(inst.read_from_file(tmp.path()));
        DCKP_CHECK(has_expected_edges(inst));
    }
    {
        dckp_test::ScopedTempFile tmp("norm_zero", zero_based);
        DCKPInstance inst;
        DCKP_CHECK(inst.read_from_file(tmp.path()));
        DCKP_CHECK(has_expected_edges(inst));
    }

    const std::string ampl_zero_based =
        "param n := 5;\n"
        "param c := 10;\n"
        "param : V : p w :=\n"
        " 0 7 2\n"
        " 1 3 1\n"
        " 2 4 3\n"
        " 3 8 5\n"
        " 4 2 1 ;\n"
        "set E :=\n"
        " 0 1\n"
        " 1 2\n"
        " 3 4 ;\n";
    {
        dckp_test::ScopedTempFile tmp("norm_ampl", ampl_zero_based);
        DCKPInstance inst;
        DCKP_CHECK(inst.read_from_file(tmp.path()));
        DCKP_CHECK(has_expected_edges(inst));
    }

    return 0;
}
