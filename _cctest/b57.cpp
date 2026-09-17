#include <charconv>
#include <string_view>
#include <ranges>
#include <optional>
#include <cstdio>

std::optional<int> parse_pid(std::string_view text) {
    if (text.empty()) return std::nullopt;
    if (!std::ranges::all_of(text, [](char ch) {
            return ch >= '0' && ch <= '9';
        })) return std::nullopt;

    int pid = 0;
    const auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), pid);
    if (ec != std::errc{} || ptr != text.data() + text.size() || pid <= 0)
        return std::nullopt;
    return pid;
}

int main(void) {
    for (auto s : {"1234", "12a4", "", "-1", "0"}) {
        if (auto p = parse_pid(s))
            printf("\"%s\" -> %d\n", s, *p);
        else
            printf("\"%s\" -> 无效\n", s);
    }
    return 0;
}