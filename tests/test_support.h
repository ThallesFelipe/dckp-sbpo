#pragma once

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <system_error>

#define DCKP_CHECK(expr)                                                  \
    do                                                                    \
    {                                                                     \
        if (!(expr))                                                      \
        {                                                                 \
            std::cerr << __FILE__ << ':' << __LINE__                      \
                      << " CHECK failed: " #expr << '\n';                 \
            return 1;                                                     \
        }                                                                 \
    } while (0)

#define DCKP_CHECK_EQ(a, b)                                               \
    do                                                                    \
    {                                                                     \
        const auto _dckp_a = (a);                                         \
        const auto _dckp_b = (b);                                         \
        if (!(_dckp_a == _dckp_b))                                        \
        {                                                                 \
            std::cerr << __FILE__ << ':' << __LINE__                      \
                      << " EQ failed: " #a " == " #b                      \
                      << " (" << _dckp_a << " vs " << _dckp_b << ")\n";   \
            return 1;                                                     \
        }                                                                 \
    } while (0)

namespace dckp_test
{
    /**
     * @brief Creates a unique temporary file path in the OS temp dir.
     * The file is not created, only named.
     */
    inline std::filesystem::path makeTempPath(std::string_view stem, std::string_view ext = ".txt")
    {
        std::random_device rd;
        std::mt19937_64 eng(rd());
        std::uniform_int_distribution<std::uint64_t> dist;
        std::ostringstream name;
        name << stem << '_' << std::hex << dist(eng) << ext;
        return std::filesystem::temp_directory_path() / name.str();
    }

    /**
     * @brief RAII helper that writes @p content to a fresh temp file and
     * deletes the file on destruction.
     */
    class ScopedTempFile
    {
    public:
        ScopedTempFile(std::string_view stem, std::string_view content)
            : path_(makeTempPath(stem))
        {
            std::ofstream out(path_, std::ios::binary);
            out << content;
        }

        ~ScopedTempFile()
        {
            std::error_code ec;
            std::filesystem::remove(path_, ec);
        }

        ScopedTempFile(const ScopedTempFile &) = delete;
        ScopedTempFile &operator=(const ScopedTempFile &) = delete;

        [[nodiscard]] const std::filesystem::path &path() const noexcept { return path_; }

    private:
        std::filesystem::path path_;
    };
}
