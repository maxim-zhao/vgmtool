#pragma once
#include <string>
#include <unordered_map>

#include "BcdVersion.h"

class BinaryData;

class Gd3Tag
{
public:
    Gd3Tag() = default;;
    ~Gd3Tag() = default;

    void from_binary(BinaryData& data);
    void to_binary(BinaryData& data) const;
    [[nodiscard]] bool empty() const;

    enum class Key
    {
        TitleEn,
        TitleJp,
        GameEn,
        GameJp,
        SystemEn,
        SystemJp,
        AuthorEn,
        AuthorJp,
        ReleaseDate,
        Creator,
        Notes
    };

    [[nodiscard]] std::wstring get_text(Key key) const;
    void set_text(Key key, const std::wstring& value);

    std::string write_to_text() const;

private:
    BcdVersion _version{};
    std::unordered_map<Key, std::wstring> _text;
};
