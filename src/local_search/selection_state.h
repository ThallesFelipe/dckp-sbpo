#pragma once

#include "../utils/instance_reader.h"
#include "../utils/solution.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace dckp
{
    struct SelectionState
    {
        using ItemId = DCKPInstance::ItemId;

        const DCKPInstance &instance;
        Solution &solution;
        std::vector<std::int32_t> conflict_count;
        std::vector<std::uint8_t> in_solution;

        SelectionState(const DCKPInstance &inst, Solution &sol)
            : instance(inst),
              solution(sol),
              conflict_count(static_cast<std::size_t>(inst.n_items()), std::int32_t{0}),
              in_solution(static_cast<std::size_t>(inst.n_items()), std::uint8_t{0})
        {
            const auto &graph = instance.conflict_graph();
            for (const ItemId item : solution.selectedItems())
            {
                in_solution[static_cast<std::size_t>(item)] = 1;
            }
            for (const ItemId item : solution.selectedItems())
            {
                for (const ItemId neighbor : graph[static_cast<std::size_t>(item)])
                {
                    ++conflict_count[static_cast<std::size_t>(neighbor)];
                }
            }
        }

        void applyAdd(const ItemId item)
        {
            const auto index = static_cast<std::size_t>(item);
            solution.addItem(item);
            in_solution[index] = 1;
            for (const ItemId neighbor : instance.conflict_graph()[index])
            {
                ++conflict_count[static_cast<std::size_t>(neighbor)];
            }
        }

        void applyRemove(const ItemId item)
        {
            const auto index = static_cast<std::size_t>(item);
            solution.removeItem(item);
            in_solution[index] = 0;
            for (const ItemId neighbor : instance.conflict_graph()[index])
            {
                --conflict_count[static_cast<std::size_t>(neighbor)];
            }
        }
    };
}
