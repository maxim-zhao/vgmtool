#pragma once
#include <string>

class BinaryData;

class BcdVersion
{
public:
    BcdVersion() = default;

    void from_binary(BinaryData& data);
    void to_binary(BinaryData& data) const;

    [[nodiscard]] int major() const
    {
        return _major;
    }

    [[nodiscard]] int minor() const
    {
        return _minor;
    }

    void set_major(const int major)
    {
        _major = major;
    }

    void set_minor(const int minor)
    {
        _minor = minor;
    }

    [[nodiscard]] bool at_least(int major, int minor) const;
    [[nodiscard]] std::string string();

private:
    static int from_bcd(uint32_t bcd);

    int _major;
    int _minor;
};
