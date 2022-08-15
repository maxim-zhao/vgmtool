#include "VgmFile.h"

#include <stdexcept>

#include "BinaryData.h"
#include "utils.h"

VgmFile::VgmFile(const std::string& filename)
{
    // Read file
    BinaryData data(filename);

    // Parse header
    _header.from_binary(data);

    // Do we have more (or less) data than expected?
    if (data.size() != _header.eof_offset())
    {
        throw std::runtime_error(Utils::format(
            "Unexpected data in file \"%s\": EOF offset is 0x%x but file size is 0x%x",
            filename.c_str(),
            _header.eof_offset(),
            data.size()));
    }

    const auto gd3Offset = _header.gd3_offset();
    if (gd3Offset > 0)
    {
        data.seek(gd3Offset);
        _gd3Tag.from_binary(data);
    }

    // We copy the data as a blob
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
