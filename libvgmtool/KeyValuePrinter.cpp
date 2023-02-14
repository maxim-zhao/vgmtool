#include "KeyValuePrinter.h"

#include <algorithm>
#include <format>
#include <sstream>
#include <ranges>

void KeyValuePrinter::add(const std::string& key, const std::string& value)
{
    _pairs.emplace_back(key, value);
}

std::string KeyValuePrinter::string() const
{
    std::ostringstream ss;
    const auto maxKeyWidth = std::ranges::max(_pairs | std::views::transform([](const auto& pair) { return pair.first.length(); })) + 1;
    for (const auto& [key, value] : _pairs)
    {
        ss << std::format("{: <{}} {}\n", key + ":", maxKeyWidth, value);
    }
    return ss.str();
}
