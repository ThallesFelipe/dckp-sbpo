#include "instance_reader.h"

#include <algorithm>
#include <charconv>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <optional>
#include <sstream>
#include <string>
#include <system_error>

namespace
{
    enum class InputFormat
    {
        NumericCompact,
        AmplLike,
    };

    using ItemId = DCKPInstance::ItemId;
    using Value = DCKPInstance::Value;
    using Conflict = DCKPInstance::Conflict;

    std::string_view trim(std::string_view text) noexcept
    {
        const std::size_t begin = text.find_first_not_of(" \t\r\n");
        if (begin == std::string_view::npos)
        {
            return {};
        }

        const std::size_t end = text.find_last_not_of(" \t\r\n");
        return text.substr(begin, end - begin + 1U);
    }

    bool starts_with(std::string_view text, std::string_view prefix) noexcept
    {
        return text.size() >= prefix.size() && text.substr(0, prefix.size()) == prefix;
    }

    bool try_parse_i32(std::string_view token, ItemId &value)
    {
        token = trim(token);
        if (token.empty())
        {
            return false;
        }

        std::int64_t parsed_value = 0;
        const char *first = token.data();
        const char *last = token.data() + token.size();
        const auto [ptr, ec] = std::from_chars(first, last, parsed_value);

        if (ec != std::errc{} || ptr != last)
        {
            return false;
        }

        if (parsed_value < static_cast<std::int64_t>(std::numeric_limits<ItemId>::min()) ||
            parsed_value > static_cast<std::int64_t>(std::numeric_limits<ItemId>::max()))
        {
            return false;
        }

        value = static_cast<ItemId>(parsed_value);
        return true;
    }

    bool try_read_i32(std::istringstream &in, ItemId &value)
    {
        std::int64_t parsed_value = 0;
        if (!(in >> parsed_value))
        {
            return false;
        }

        if (parsed_value < static_cast<std::int64_t>(std::numeric_limits<ItemId>::min()) ||
            parsed_value > static_cast<std::int64_t>(std::numeric_limits<ItemId>::max()))
        {
            return false;
        }

        value = static_cast<ItemId>(parsed_value);
        return true;
    }

    bool parse_param_line(std::string_view line, std::string_view name, ItemId &out_value)
    {
        if (!starts_with(line, name))
        {
            return false;
        }

        const std::size_t assign_pos = line.find(":=");
        if (assign_pos == std::string_view::npos)
        {
            return false;
        }

        std::string_view value_text = trim(line.substr(assign_pos + 2U));
        if (!value_text.empty() && value_text.back() == ';')
        {
            value_text.remove_suffix(1U);
            value_text = trim(value_text);
        }

        return try_parse_i32(value_text, out_value);
    }

    InputFormat detect_format(std::string_view content) noexcept
    {
        const bool has_ampl_markers = content.find("param : V : p w :=") != std::string_view::npos &&
                                      content.find("set E :=") != std::string_view::npos;
        return has_ampl_markers ? InputFormat::AmplLike : InputFormat::NumericCompact;
    }
} // namespace

void DCKPInstance::set_error(std::string message)
{
    last_error_ = std::move(message);
}

bool DCKPInstance::is_valid_item(ItemId item) const noexcept
{
    return item >= 0 && item < n_items_;
}

void DCKPInstance::clear_data() noexcept
{
    n_items_ = 0;
    capacity_ = 0;
    profits_.clear();
    weights_.clear();
    conflicts_.clear();
    conflict_graph_.clear();
}

void DCKPInstance::clear() noexcept
{
    clear_data();
    last_error_.clear();
}

bool DCKPInstance::read_from_file(const std::filesystem::path &file_path)
{
    clear_data();
    last_error_.clear();

    std::ifstream input(file_path, std::ios::binary);
    if (!input)
    {
        set_error("Nao foi possivel abrir o arquivo: " + file_path.string());
        return false;
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();

    if (!input.good() && !input.eof())
    {
        set_error("Falha ao ler o arquivo: " + file_path.string());
        return false;
    }

    std::string content = buffer.str();
    if (content.empty())
    {
        set_error("Arquivo vazio: " + file_path.string());
        return false;
    }

    if (content.size() >= 3U &&
        static_cast<unsigned char>(content[0]) == 0xEFU &&
        static_cast<unsigned char>(content[1]) == 0xBBU &&
        static_cast<unsigned char>(content[2]) == 0xBFU)
    {
        content.erase(0, 3);
    }

    const bool parsed = detect_format(content) == InputFormat::AmplLike
                            ? parse_ampl_like(content)
                            : parse_numeric_compact(content);

    if (!parsed)
    {
        clear_data();
        return false;
    }

    return true;
}

bool DCKPInstance::parse_numeric_compact(std::string_view content)
{
    std::istringstream in(std::string{content});
    std::vector<ItemId> tokens;
    tokens.reserve(4096);

    std::int64_t parsed_value = 0;
    while (in >> parsed_value)
    {
        if (parsed_value < static_cast<std::int64_t>(std::numeric_limits<ItemId>::min()) ||
            parsed_value > static_cast<std::int64_t>(std::numeric_limits<ItemId>::max()))
        {
            set_error("Token numerico fora do intervalo suportado para int32.");
            return false;
        }
        tokens.push_back(static_cast<ItemId>(parsed_value));
    }

    if (!in.eof())
    {
        set_error("Formato numerico compacto invalido: token nao numerico encontrado.");
        return false;
    }

    if (tokens.size() < 3U)
    {
        set_error("Cabecalho invalido no formato numerico compacto.");
        return false;
    }

    const ItemId parsed_n_items = tokens[0];
    if (parsed_n_items <= 0)
    {
        set_error("Numero de itens deve ser positivo.");
        return false;
    }

    n_items_ = parsed_n_items;
    const std::size_t item_count = static_cast<std::size_t>(n_items_);

    const std::size_t values_start = 3U;
    const std::size_t weights_start = values_start + item_count;
    const std::size_t conflicts_start = weights_start + item_count;

    if (tokens.size() < conflicts_start)
    {
        set_error("Arquivo numerico compacto incompleto: faltam profits/weights.");
        return false;
    }

    const std::size_t remaining_tokens = tokens.size() - conflicts_start;
    const ItemId second_header_value = tokens[1];
    const ItemId third_header_value = tokens[2];

    const auto layout_matches = [remaining_tokens](ItemId candidate_conflicts) -> bool
    {
        if (candidate_conflicts < 0)
        {
            return false;
        }

        const std::uint64_t expected_tokens =
            static_cast<std::uint64_t>(candidate_conflicts) * 2ULL;
        return static_cast<std::uint64_t>(remaining_tokens) == expected_tokens;
    };

    const bool second_is_conflicts = layout_matches(second_header_value);
    const bool third_is_conflicts = layout_matches(third_header_value);

    ItemId declared_conflicts = 0;
    ItemId parsed_capacity = 0;

    if (second_is_conflicts && !third_is_conflicts)
    {
        declared_conflicts = second_header_value;
        parsed_capacity = third_header_value;
    }
    else if (!second_is_conflicts && third_is_conflicts)
    {
        declared_conflicts = third_header_value;
        parsed_capacity = second_header_value;
    }
    else if (second_is_conflicts && third_is_conflicts)
    {
        declared_conflicts = second_header_value;
        parsed_capacity = third_header_value;
    }
    else
    {
        set_error("Cabecalho inconsistente: nao foi possivel inferir a quantidade de conflitos pelo tamanho do arquivo.");
        return false;
    }

    if (parsed_capacity < 0)
    {
        set_error("Capacidade nao pode ser negativa.");
        return false;
    }

    if (declared_conflicts < 0)
    {
        set_error("Numero de conflitos nao pode ser negativo.");
        return false;
    }

    capacity_ = parsed_capacity;

    profits_.assign(item_count, Value{0});
    weights_.assign(item_count, Value{0});

    for (std::size_t index = 0; index < item_count; ++index)
    {
        profits_[index] = tokens[values_start + index];
        weights_[index] = tokens[weights_start + index];

        if (weights_[index] < 0)
        {
            set_error("Peso negativo encontrado no formato numerico compacto.");
            return false;
        }
    }

    std::vector<Conflict> raw_conflicts;
    raw_conflicts.reserve(static_cast<std::size_t>(declared_conflicts));

    for (std::size_t edge = 0; edge < static_cast<std::size_t>(declared_conflicts); ++edge)
    {
        const std::size_t offset = conflicts_start + (2U * edge);
        raw_conflicts.emplace_back(tokens[offset], tokens[offset + 1U]);
    }

    if (!normalize_conflicts(raw_conflicts, 1))
    {
        return false;
    }

    build_conflict_graph();
    return true;
}

bool DCKPInstance::parse_ampl_like(std::string_view content)
{
    enum class State
    {
        Header,
        Values,
        WaitingEdges,
        Edges,
        Done,
    };

    State state = State::Header;
    std::optional<ItemId> parsed_n_items;
    std::optional<ItemId> parsed_capacity;

    std::vector<Value> parsed_profits;
    std::vector<Value> parsed_weights;
    std::vector<bool> seen_items;
    std::vector<Conflict> raw_conflicts;

    auto parse_value_record = [&](std::string_view record,
                                  bool &section_finished,
                                  std::size_t line_number) -> bool
    {
        section_finished = false;
        record = trim(record);

        if (record.empty())
        {
            return true;
        }

        if (record == ";")
        {
            section_finished = true;
            return true;
        }

        if (record.back() == ';')
        {
            section_finished = true;
            record.remove_suffix(1U);
            record = trim(record);
            if (record.empty())
            {
                return true;
            }
        }

        std::istringstream line_stream(std::string{record});
        ItemId item = 0;
        ItemId profit = 0;
        ItemId weight = 0;

        if (!try_read_i32(line_stream, item) ||
            !try_read_i32(line_stream, profit) ||
            !try_read_i32(line_stream, weight))
        {
            set_error("Linha " + std::to_string(line_number) +
                      ": registro invalido no bloco de valores (esperado: item profit weight).");
            return false;
        }

        std::string extra_token;
        if (line_stream >> extra_token)
        {
            set_error("Linha " + std::to_string(line_number) + ": tokens extras no bloco de valores.");
            return false;
        }

        if (!is_valid_item(item))
        {
            set_error("Linha " + std::to_string(line_number) +
                      ": indice de item fora do intervalo no bloco de valores.");
            return false;
        }

        if (weight < 0)
        {
            set_error("Linha " + std::to_string(line_number) + ": peso negativo no bloco de valores.");
            return false;
        }

        const std::size_t item_index = static_cast<std::size_t>(item);
        if (seen_items[item_index])
        {
            set_error("Linha " + std::to_string(line_number) + ": item duplicado no bloco de valores.");
            return false;
        }

        seen_items[item_index] = true;
        parsed_profits[item_index] = profit;
        parsed_weights[item_index] = weight;
        return true;
    };

    auto parse_edge_record = [&](std::string_view record,
                                 bool &section_finished,
                                 std::size_t line_number) -> bool
    {
        section_finished = false;
        record = trim(record);

        if (record.empty())
        {
            return true;
        }

        if (record == ";")
        {
            section_finished = true;
            return true;
        }

        if (record.back() == ';')
        {
            section_finished = true;
            record.remove_suffix(1U);
            record = trim(record);
            if (record.empty())
            {
                return true;
            }
        }

        std::istringstream line_stream(std::string{record});
        ItemId u = 0;
        ItemId v = 0;
        if (!try_read_i32(line_stream, u) || !try_read_i32(line_stream, v))
        {
            set_error("Linha " + std::to_string(line_number) +
                      ": registro invalido no bloco de arestas (esperado: u v).");
            return false;
        }

        std::string extra_token;
        if (line_stream >> extra_token)
        {
            set_error("Linha " + std::to_string(line_number) + ": tokens extras no bloco de arestas.");
            return false;
        }

        raw_conflicts.emplace_back(u, v);
        return true;
    };

    std::istringstream input(std::string{content});
    std::string line;
    std::size_t line_number = 0;

    while (std::getline(input, line))
    {
        ++line_number;

        std::string_view view = trim(line);
        const std::size_t comment_pos = view.find('#');
        if (comment_pos != std::string_view::npos)
        {
            view = trim(view.substr(0, comment_pos));
        }

        if (view.empty())
        {
            continue;
        }

        if (state == State::Header)
        {
            ItemId scalar_value = 0;
            if (parse_param_line(view, "param n", scalar_value))
            {
                parsed_n_items = scalar_value;
                continue;
            }

            if (parse_param_line(view, "param c", scalar_value))
            {
                parsed_capacity = scalar_value;
                continue;
            }

            if (starts_with(view, "param : V : p w :="))
            {
                if (!parsed_n_items.has_value() || !parsed_capacity.has_value())
                {
                    set_error("Cabecalho AMPL incompleto: parametros n e c devem aparecer antes do bloco de valores.");
                    return false;
                }

                if (*parsed_n_items <= 0)
                {
                    set_error("Parametro n deve ser positivo no formato AMPL.");
                    return false;
                }

                if (*parsed_capacity < 0)
                {
                    set_error("Parametro c nao pode ser negativo no formato AMPL.");
                    return false;
                }

                n_items_ = *parsed_n_items;
                capacity_ = *parsed_capacity;

                const std::size_t item_count = static_cast<std::size_t>(n_items_);
                parsed_profits.assign(item_count, Value{0});
                parsed_weights.assign(item_count, Value{0});
                seen_items.assign(item_count, false);

                state = State::Values;

                std::string_view remainder = trim(view.substr(std::string_view{"param : V : p w :="}.size()));
                if (!remainder.empty())
                {
                    bool section_finished = false;
                    if (!parse_value_record(remainder, section_finished, line_number))
                    {
                        return false;
                    }
                    if (section_finished)
                    {
                        state = State::WaitingEdges;
                    }
                }

                continue;
            }

            continue;
        }

        if (state == State::Values)
        {
            bool section_finished = false;
            if (!parse_value_record(view, section_finished, line_number))
            {
                return false;
            }

            if (section_finished)
            {
                state = State::WaitingEdges;
            }
            continue;
        }

        if (state == State::WaitingEdges)
        {
            if (starts_with(view, "set E :="))
            {
                state = State::Edges;

                std::string_view remainder = trim(view.substr(std::string_view{"set E :="}.size()));
                if (!remainder.empty())
                {
                    bool section_finished = false;
                    if (!parse_edge_record(remainder, section_finished, line_number))
                    {
                        return false;
                    }
                    if (section_finished)
                    {
                        state = State::Done;
                    }
                }

                continue;
            }

            continue;
        }

        if (state == State::Edges)
        {
            bool section_finished = false;
            if (!parse_edge_record(view, section_finished, line_number))
            {
                return false;
            }

            if (section_finished)
            {
                state = State::Done;
                break;
            }
        }
    }

    if (state == State::Header)
    {
        set_error("Formato AMPL invalido: bloco de valores nao encontrado.");
        return false;
    }

    if (state == State::Values)
    {
        set_error("Formato AMPL invalido: bloco de valores sem terminador ';'.");
        return false;
    }

    if (state == State::WaitingEdges)
    {
        set_error("Formato AMPL invalido: bloco de arestas 'set E :=' nao encontrado.");
        return false;
    }

    if (state == State::Edges)
    {
        set_error("Formato AMPL invalido: bloco de arestas sem terminador ';'.");
        return false;
    }

    const std::size_t expected_items = static_cast<std::size_t>(n_items_);
    const std::size_t loaded_items = static_cast<std::size_t>(
        std::count(seen_items.begin(), seen_items.end(), true));

    if (loaded_items != expected_items)
    {
        set_error("Bloco de valores incompleto: nem todos os itens de 0..n-1 foram informados.");
        return false;
    }

    profits_ = std::move(parsed_profits);
    weights_ = std::move(parsed_weights);

    if (!normalize_conflicts(raw_conflicts, 0))
    {
        return false;
    }

    build_conflict_graph();
    return true;
}

bool DCKPInstance::normalize_conflicts(const std::vector<Conflict> &raw_conflicts,
                                       int default_base)
{
    bool valid_zero_based = true;
    bool valid_one_based = true;
    bool saw_zero = false;
    bool saw_n = false;

    for (const Conflict &edge : raw_conflicts)
    {
        const ItemId u = edge.first;
        const ItemId v = edge.second;

        saw_zero = saw_zero || (u == 0) || (v == 0);
        saw_n = saw_n || (u == n_items_) || (v == n_items_);

        const bool in_zero_range = u >= 0 && u < n_items_ && v >= 0 && v < n_items_;
        const bool in_one_range = u >= 1 && u <= n_items_ && v >= 1 && v <= n_items_;

        valid_zero_based = valid_zero_based && in_zero_range;
        valid_one_based = valid_one_based && in_one_range;
    }

    if (!valid_zero_based && !valid_one_based)
    {
        set_error("Arestas de conflito com indices fora do intervalo permitido.");
        return false;
    }

    int base = default_base;
    if (valid_zero_based && !valid_one_based)
    {
        base = 0;
    }
    else if (!valid_zero_based && valid_one_based)
    {
        base = 1;
    }
    else
    {
        if (saw_zero && !saw_n)
        {
            base = 0;
        }
        else if (!saw_zero && saw_n)
        {
            base = 1;
        }
    }

    conflicts_.clear();
    conflicts_.reserve(raw_conflicts.size());

    for (const Conflict &edge : raw_conflicts)
    {
        ItemId u = static_cast<ItemId>(edge.first - base);
        ItemId v = static_cast<ItemId>(edge.second - base);

        if (!is_valid_item(u) || !is_valid_item(v))
        {
            set_error("Aresta de conflito invalida apos normalizacao de indice.");
            return false;
        }

        if (u == v)
        {
            continue;
        }

        if (u > v)
        {
            std::swap(u, v);
        }

        conflicts_.emplace_back(u, v);
    }

    std::sort(conflicts_.begin(), conflicts_.end());
    conflicts_.erase(std::unique(conflicts_.begin(), conflicts_.end()), conflicts_.end());
    return true;
}

void DCKPInstance::build_conflict_graph()
{
    conflict_graph_.assign(static_cast<std::size_t>(n_items_), {});

    for (const Conflict &edge : conflicts_)
    {
        const std::size_t u = static_cast<std::size_t>(edge.first);
        const std::size_t v = static_cast<std::size_t>(edge.second);
        conflict_graph_[u].push_back(edge.second);
        conflict_graph_[v].push_back(edge.first);
    }

    for (std::vector<ItemId> &adjacency : conflict_graph_)
    {
        std::sort(adjacency.begin(), adjacency.end());
        adjacency.erase(std::unique(adjacency.begin(), adjacency.end()), adjacency.end());
    }
}

bool DCKPInstance::has_conflict(ItemId item1, ItemId item2) const noexcept
{
    if (!is_valid_item(item1) || !is_valid_item(item2) || item1 == item2)
    {
        return false;
    }

    const std::vector<ItemId> &adj_1 = conflict_graph_[static_cast<std::size_t>(item1)];
    const std::vector<ItemId> &adj_2 = conflict_graph_[static_cast<std::size_t>(item2)];

    if (adj_1.size() <= adj_2.size())
    {
        return std::binary_search(adj_1.begin(), adj_1.end(), item2);
    }

    return std::binary_search(adj_2.begin(), adj_2.end(), item1);
}

DCKPInstance::ItemId DCKPInstance::conflict_degree(ItemId item) const noexcept
{
    if (!is_valid_item(item))
    {
        return 0;
    }

    const std::size_t degree = conflict_graph_[static_cast<std::size_t>(item)].size();
    if (degree > static_cast<std::size_t>(std::numeric_limits<ItemId>::max()))
    {
        return std::numeric_limits<ItemId>::max();
    }

    return static_cast<ItemId>(degree);
}

double DCKPInstance::conflict_density() const noexcept
{
    if (n_items_ <= 1)
    {
        return 0.0;
    }

    const double total_possible_edges =
        (static_cast<double>(n_items_) * static_cast<double>(n_items_ - 1)) / 2.0;

    if (total_possible_edges <= 0.0)
    {
        return 0.0;
    }

    return 100.0 * static_cast<double>(conflicts_.size()) / total_possible_edges;
}

void DCKPInstance::print(std::ostream &out) const
{
    out << "DCKP instance summary\n";
    out << "Items: " << n_items_ << '\n';
    out << "Capacity: " << capacity_ << '\n';
    out << "Conflicts: " << conflicts_.size() << '\n';
    out << "Conflict density: " << std::fixed << std::setprecision(2)
        << conflict_density() << "%\n";

    if (profits_.empty() || weights_.empty())
    {
        return;
    }

    const auto [min_profit_it, max_profit_it] = std::minmax_element(profits_.begin(), profits_.end());
    const auto [min_weight_it, max_weight_it] = std::minmax_element(weights_.begin(), weights_.end());

    const double avg_profit = std::accumulate(profits_.begin(), profits_.end(), 0.0) /
                              static_cast<double>(profits_.size());
    const double avg_weight = std::accumulate(weights_.begin(), weights_.end(), 0.0) /
                              static_cast<double>(weights_.size());

    out << "Profit range: [" << *min_profit_it << "-" << *max_profit_it
        << "], avg=" << avg_profit << '\n';
    out << "Weight range: [" << *min_weight_it << "-" << *max_weight_it
        << "], avg=" << avg_weight << '\n';
}

void DCKPInstance::print() const
{
    print(std::cout);
}

std::string_view DCKPInstance::last_error() const noexcept
{
    return last_error_;
}

DCKPInstance::ItemId DCKPInstance::n_items() const noexcept
{
    return n_items_;
}

DCKPInstance::ItemId DCKPInstance::capacity() const noexcept
{
    return capacity_;
}

DCKPInstance::ItemId DCKPInstance::n_conflicts() const noexcept
{
    return static_cast<ItemId>(conflicts_.size());
}

const std::vector<DCKPInstance::Value> &DCKPInstance::profits() const noexcept
{
    return profits_;
}

const std::vector<DCKPInstance::Value> &DCKPInstance::weights() const noexcept
{
    return weights_;
}

const std::vector<DCKPInstance::Conflict> &DCKPInstance::conflicts() const noexcept
{
    return conflicts_;
}

const std::vector<std::vector<DCKPInstance::ItemId>> &DCKPInstance::conflict_graph() const noexcept
{
    return conflict_graph_;
}

bool DCKPInstance::empty() const noexcept
{
    return n_items_ == 0;
}
