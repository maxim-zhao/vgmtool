#include "Gd3Tag.h"

#include <stdexcept>
#include <ranges>

#include "BinaryData.h"
#include "utils.h"

namespace
{
    const std::string GD3_IDENT("Gd3 ");
}


void Gd3Tag::from_binary(BinaryData& data)
{
    // Check header
    if (const auto& ident = data.read_ascii_string(4); ident != GD3_IDENT)
    {
        throw std::runtime_error(Utils::format("Invalid GD3 header ident \"%s\"", ident.c_str()));
    }

    _version.from_binary(data);

    if (_version.major() != 1 || _version.minor() != 0)
    {
        throw std::runtime_error(Utils::format("Invalid GD3 version %d.%02d", _version.major(), _version.minor()));
    }

    // Now read the strings
    const auto textLengthBytes = data.read_uint32();
    _text.clear();
    _text[Key::TitleEn] = data.read_null_terminated_utf16_string();
    _text[Key::TitleJp] = data.read_null_terminated_utf16_string();
    _text[Key::GameEn] = data.read_null_terminated_utf16_string();
    _text[Key::GameJp] = data.read_null_terminated_utf16_string();
    _text[Key::SystemEn] = data.read_null_terminated_utf16_string();
    _text[Key::SystemJp] = data.read_null_terminated_utf16_string();
    _text[Key::AuthorEn] = data.read_null_terminated_utf16_string();
    _text[Key::AuthorJp] = data.read_null_terminated_utf16_string();
    _text[Key::ReleaseDate] = data.read_null_terminated_utf16_string();
    _text[Key::Creator] = data.read_null_terminated_utf16_string();
    _text[Key::Notes] = data.read_null_terminated_utf16_string();

    // Validate header
    // This seems to often be the case due to bugs in the tools
    uint32_t stringLengths = 0;
    for (const auto& s : _text | std::views::values)
    {
        stringLengths += static_cast<uint32_t>(s.length() + 1);
    }
    if (stringLengths * 2 != textLengthBytes)
    {
//        throw std::runtime_error(Utils::format("GD3 text size does not match header, read %d bytes but header says %d", stringLengths * 2, textLengthBytes));
    }
}

void Gd3Tag::to_binary(BinaryData& data) const
{
    data.write_unterminated_ascii_string(GD3_IDENT);
    _version.to_binary(data);
    uint32_t textLength = 0;
    for (const auto& s : _text | std::views::values)
    {
        textLength += static_cast<uint32_t>(s.size()) * 2 + 2;
    }
    data.write_uint32(textLength);
    data.write_terminated_utf16_string(get_text(Key::TitleEn));
    data.write_terminated_utf16_string(get_text(Key::TitleJp));
    data.write_terminated_utf16_string(get_text(Key::GameEn));
    data.write_terminated_utf16_string(get_text(Key::GameJp));
    data.write_terminated_utf16_string(get_text(Key::SystemEn));
    data.write_terminated_utf16_string(get_text(Key::SystemJp));
    data.write_terminated_utf16_string(get_text(Key::AuthorEn));
    data.write_terminated_utf16_string(get_text(Key::AuthorJp));
    data.write_terminated_utf16_string(get_text(Key::ReleaseDate));
    data.write_terminated_utf16_string(get_text(Key::Creator));
    data.write_terminated_utf16_string(get_text(Key::Notes));
}

bool Gd3Tag::empty() const
{
    return _text.empty();
}

std::wstring Gd3Tag::get_text(const Key key) const
{
    const auto it = _text.find(key);
    if (it == _text.end())
    {
        return {};
    }
    return it->second;
}

void Gd3Tag::set_text(const Key key, const std::wstring& value)
{
    _text[key] = value;
}
