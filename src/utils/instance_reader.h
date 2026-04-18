#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

/**
 * @brief Stores and queries one Disjunctively Constrained Knapsack Problem instance.
 */
class DCKPInstance
{
public:
    using ItemId = std::int32_t;
    using Value = std::int32_t;
    using Capacity = std::int64_t;
    using Count = std::size_t;
    using Conflict = std::pair<ItemId, ItemId>;

    [[nodiscard]] bool read_from_file(const std::filesystem::path &file_path);

    [[nodiscard]] bool has_conflict(ItemId item1, ItemId item2) const noexcept;

    void print(std::ostream &out) const;
    void print() const;

    [[nodiscard]] double conflict_density() const noexcept;
    [[nodiscard]] Count conflict_degree(ItemId item) const noexcept;

    [[nodiscard]] std::string_view last_error() const noexcept;

    [[nodiscard]] ItemId n_items() const noexcept;
    [[nodiscard]] Capacity capacity() const noexcept;
    [[nodiscard]] Count n_conflicts() const noexcept;
    [[nodiscard]] const std::vector<Value> &profits() const noexcept;
    [[nodiscard]] const std::vector<Value> &weights() const noexcept;
    [[nodiscard]] const std::vector<Conflict> &conflicts() const noexcept;
    [[nodiscard]] const std::vector<std::vector<ItemId>> &conflict_graph() const noexcept;
    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] bool is_valid_item(ItemId item) const noexcept;
    void clear() noexcept;

private:
    ItemId n_items_{0};
    Capacity capacity_{0};
    std::vector<Value> profits_{};
    std::vector<Value> weights_{};
    std::vector<Conflict> conflicts_{};
    std::vector<std::vector<ItemId>> conflict_graph_{};
    std::string last_error_{};

    [[nodiscard]] bool parse_numeric_compact(std::string_view content);
    [[nodiscard]] bool parse_ampl_like(std::string_view content);
    [[nodiscard]] bool normalize_conflicts(const std::vector<Conflict> &raw_conflicts, int default_base);
    void build_conflict_graph();
    void clear_data() noexcept;
    void set_error(std::string message);
};
