#pragma once

#include "../utils/instance_reader.h"

#include <vector>

namespace dckp
{
    [[nodiscard]] std::vector<DCKPInstance::ItemId> makeProfitOrder(
        const DCKPInstance &instance);
}
