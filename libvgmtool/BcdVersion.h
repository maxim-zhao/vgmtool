#pragma once

class BinaryData;

class BcdVersion
{
public:
    BcdVersion() = default;

    void from_binary(BinaryData& data);

    [[nodiscard]] int major() const
    {
        return _major;
    }

    [[nodiscard]] int minor() const
    {
        return _minor;
    }

private:
    static int from_bcd(int bcd);

    int _major;
    int _minor;
};
