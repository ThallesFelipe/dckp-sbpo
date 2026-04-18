#pragma once

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
    using Conflict = std::pair<ItemId, ItemId>;

    /**
     * @brief Loads an instance file and replaces current data.
     */
    [[nodiscard]] bool read_from_file(const std::filesystem::path &file_path);

    /**
     * @brief Returns true when two items are in conflict.
     */
    [[nodiscard]] bool has_conflict(ItemId item1, ItemId item2) const noexcept;

    /**
     * @brief Prints a short summary to a stream.
     */
    void print(std::ostream &out) const;

    /**
     * @brief Prints a short summary to standard output.
     */
    void print() const;

    /**
     * @brief Returns graph density in percentage.
     */
    [[nodiscard]] double conflict_density() const noexcept;

    /**
     * @brief Returns the degree of one item in the conflict graph.
     */
    [[nodiscard]] ItemId conflict_degree(ItemId item) const noexcept;

    /**
     * @brief Returns the latest load/parsing error message.
     */
    [[nodiscard]] std::string_view last_error() const noexcept;

    [[nodiscard]] ItemId n_items() const noexcept;
    [[nodiscard]] ItemId capacity() const noexcept;
    [[nodiscard]] ItemId n_conflicts() const noexcept;
    [[nodiscard]] const std::vector<Value> &profits() const noexcept;
    [[nodiscard]] const std::vector<Value> &weights() const noexcept;
    [[nodiscard]] const std::vector<Conflict> &conflicts() const noexcept;
    [[nodiscard]] const std::vector<std::vector<ItemId>> &conflict_graph() const noexcept;
    [[nodiscard]] bool empty() const noexcept;
    void clear() noexcept;

private:
    ItemId n_items_{0};
    ItemId capacity_{0};
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
    [[nodiscard]] bool is_valid_item(ItemId item) const noexcept;
};
