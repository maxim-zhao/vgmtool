#pragma once

#include <vector>
#include <string>

class KeyValuePrinter
{
public:
    KeyValuePrinter() = default;

    void add(const std::string& key, const std::string& value);

    [[nodiscard]] std::string string() const;

private:
    std::vector<std::pair<std::string, std::string>> _pairs;
};
