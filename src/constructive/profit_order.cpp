#include "profit_order.h"

#include <algorithm>
#include <cstddef>
#include <numeric>

namespace dckp
{
    std::vector<DCKPInstance::ItemId> makeProfitOrder(const DCKPInstance &instance)
    {
        const auto &profits = instance.profits();
        const auto &weights = instance.weights();

        std::vector<DCKPInstance::ItemId> order(static_cast<std::size_t>(instance.n_items()));
        std::iota(order.begin(), order.end(), DCKPInstance::ItemId{0});
        std::erase_if(order, [&profits](const DCKPInstance::ItemId item)
                      { return profits[static_cast<std::size_t>(item)] <= 0; });

        std::sort(order.begin(), order.end(),
                  [&profits, &weights](const DCKPInstance::ItemId a,
                                       const DCKPInstance::ItemId b) noexcept
                  {
                      const auto a_index = static_cast<std::size_t>(a);
                      const auto b_index = static_cast<std::size_t>(b);
                      if (profits[a_index] != profits[b_index])
                      {
                          return profits[a_index] > profits[b_index];
                      }
                      return weights[a_index] < weights[b_index];
                  });
        return order;
    }
}
