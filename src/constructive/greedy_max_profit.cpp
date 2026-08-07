#include "greedy_max_profit.h"
#include "profit_order.h"

#include <cstddef>
#include <vector>

namespace dckp
{
    std::string GreedyMaxProfit::name() const
    {
        return "Greedy_MaxProfit";
    }

    Solution GreedyMaxProfit::run(const DCKPInstance &instance, RunContext &ctx)
    {
        Solution sol(instance);
        sol.setMethodName(name());

        const auto n = instance.n_items();
        if (n <= 0)
        {
            return sol;
        }

        const auto &weights = instance.weights();
        const auto &conflict_graph = instance.conflict_graph();

        const std::vector<DCKPInstance::ItemId> order = makeProfitOrder(instance);

        std::vector<bool> forbidden(static_cast<std::size_t>(n), false);

        auto remaining_capacity = instance.capacity();

        for (const auto item : order)
        {
            if (ctx.stopping.shouldStop())
            {
                break;
            }
            ctx.stopping.tick();

            const auto idx = static_cast<std::size_t>(item);
            const auto w = static_cast<DCKPInstance::Capacity>(weights[idx]);

            if (w > remaining_capacity)
            {
                continue;
            }

            if (forbidden[idx])
            {
                continue;
            }

            sol.addItem(item);
            remaining_capacity -= w;
            ctx.stopping.registerImprovement();

            for (const auto neighbor : conflict_graph[idx])
            {
                forbidden[static_cast<std::size_t>(neighbor)] = true;
            }
        }

        return sol;
    }
}
