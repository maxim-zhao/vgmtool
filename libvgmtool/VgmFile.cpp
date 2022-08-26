#include "VgmFile.h"

#include <format>
#include <stdexcept>

#include "BinaryData.h"

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
    //const auto byteCount = endOffset - dataOffset;

    data.seek(dataOffset);

    _data.from_data(data, _header.loop_offset(), endOffset);

    // Check for orphaned data
    if (data.offset() < endOffset)
    {
        throw std::runtime_error(std::format("Unconsumed data in VGM file at offset {:x}", data.offset()));
    }
}

void VgmFile::save_file(const std::string& filename)
{
    BinaryData data;

    // First the header TODO write this last, but we need to know its size
    _header.to_binary(data);

    // Then the data
    // TODO if the header size changes then the pointers need to be rewritten
    // TODO data.add_range(_data);

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

void VgmFile::check_header(bool fix)
{
    // Check lengths
    auto totalSampleCount = 0u;
    auto loopStartSampleCount = 0u;

    for (const auto* pCommand : _data.commands())
    {
        if (auto* pWait = dynamic_cast<const VgmCommands::IWait*>(pCommand); pWait != nullptr)
        {
            totalSampleCount += pWait->duration();
        }
        else if (auto* pLoop = dynamic_cast<const VgmCommands::LoopPoint*>(pCommand); pLoop != nullptr)
        {
            loopStartSampleCount = totalSampleCount;
        }
    }

    const auto loopSampleCount = totalSampleCount - loopStartSampleCount;

    if (_header.loop_sample_count() != loopSampleCount || _header.sample_count() != totalSampleCount)
    {
        if (fix)
        {
            _header.set_sample_count(totalSampleCount);
            _header.set_loop_sample_count(loopSampleCount);
        }
        else
        {
            throw std::runtime_error(std::format(
                "Lengths:\n"
                "In file:\n"
                "Total: {} samples = {:.2} seconds\n"
                "Loop: {} samples = {:.2} seconds\n"
                "In header:\n"
                "Total: {} samples = {:.2} seconds\n"
                "Loop: {} samples = {:.2} seconds",
                _header.sample_count(), _header.sample_count() / 44100.0,
                _header.loop_sample_count(), _header.loop_sample_count() / 44100.0,
                totalSampleCount, totalSampleCount / 44100.0,
                loopSampleCount, loopSampleCount / 44100.0));
        }
    }
}
