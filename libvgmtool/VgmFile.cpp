#include "VgmFile.h"

#include <stdexcept>

#include "BinaryData.h"
#include "utils.h"

VgmFile::VgmFile(const std::string& filename)
{
    load_file(filename);
}

void VgmFile::load_file(const std::string& filename)
{
    // Read file
    BinaryData data(filename);

    // Parse header
    _header.from_binary(data);

    const auto gd3Offset = _header.gd3_offset();
    if (gd3Offset > 0)
    {
        data.seek(gd3Offset);
        _gd3Tag.from_binary(data);
    }

    // We copy the data as a blob, we don't parse it (yet)
    const auto dataOffset = _header.data_offset();
    data.seek(dataOffset);
    // The data either runs up to EOF or the GD3
    const auto endOffset = gd3Offset < dataOffset ? _header.eof_offset() : gd3Offset;
    if (endOffset <= dataOffset)
    {
        throw std::runtime_error("Invalid data offsets imply no data");
    }
    const auto byteCount = endOffset - dataOffset;
    data.copy_range(_data, dataOffset, byteCount);
}

void VgmFile::save_file(const std::string& filename)
{
    BinaryData data;

    // First the header TODO write this last, but we need to know its size
    _header.to_binary(data);

    // Then the data
    // TODO if the header size changes then the pointers need to be rewritten
    data.add_range(_data);

    // Then the GD3 tag. We could move this before the data now...
    if (!_gd3Tag.empty())
    {
        _header.set_gd3_offset(data.offset());
        _gd3Tag.to_binary(data);
    }
    else
    {
        _header.set_gd3_offset(0);
    }

    _header.set_eof_offset(data.size());

    data.seek(0u);
    // Write the header again
    _header.to_binary(data);

    // Finally, save to disk. We don't do compression here.
    data.save(filename + ".foo.vgm");
}
