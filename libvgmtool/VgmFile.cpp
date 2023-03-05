#include "VgmFile.h"

#include <format>
#include <stdexcept>

#include "BinaryData.h"
#include "IVGMToolCallback.h"
#include "libpu8.h"
#include "SN76489State.h"
#include "utils.h"
#include "YM2413State.h"

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
    auto endOffset = _header.eof_offset();
    if (_header.gd3_offset() < _header.eof_offset() && _header.gd3_offset() > dataOffset)
    {
        endOffset = gd3Offset;
    }
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
    // TODO data.write_range(_data);

    // Then the GD3 tag. We can move this before the data now...
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
        if (auto* pWait = dynamic_cast<const VgmCommands::Wait*>(pCommand); pWait != nullptr)
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

void VgmFile::write_to_text(std::ostream& s, const IVGMToolCallback& callback) const
{
    // In order to write to text we need to do multiple things:
    // 1. Print the header
    // 2. Print the VGM commands themselves
    // 3. Maintain state from these commands in order to print the current state
    // 4. Print the GD3 tag

    // First the header...
    s << "VGM Header:\n"
        << _header.write_to_text()
        << "\nVGM data:\n";

    size_t offset = _header.data_offset();
    int time = 0;
    SN76489State psgState(_header);
    YM2413State ym2413State(_header);
    BinaryData scratch;

    for (const auto* pCommand : _data.commands())
    {
        // File offset
        s << std::format("{:#010x} ", offset);
        // Get data so we can print it raw
        scratch.reset();
        pCommand->to_data(scratch);
        // We only print the first 5 bytes...
        for (auto i = 0u; i < 5; ++i)
        {
            if (i >= scratch.buffer().size())
            {
                s << "   ";
            }
            else
            {
                s << std::format("{:02x} ", scratch.buffer()[i]);
            }
        }

        // Increment the offset accordingly
        offset += scratch.buffer().size();

        switch (pCommand->chip())
        {
        case VgmHeader::Chip::Nothing:
            if (auto* pWait = dynamic_cast<const VgmCommands::Wait*>(pCommand); pWait != nullptr)
            {
                const auto duration = pWait->duration();
                // It's a wait
                time += duration;
                s << std::format(
                    "Wait:   {:5} samples ({:7.2f} ms) (total {:8} samples ({}))",
                    duration,
                    duration / 44.1,
                    time,
                    Utils::samples_to_display_text(time, true));
                if (auto* pSample = dynamic_cast<const VgmCommands::YM2612Sample*>(pCommand); pSample != nullptr)
                {
                    s << "; emit sample";
                }
            }
            else if (auto* pLoopPoint = dynamic_cast<const VgmCommands::LoopPoint*>(pCommand); pLoopPoint != nullptr)
            {
                s << "=============== LOOP POINT ===============";
            }
            else if (auto* pEndMarker = dynamic_cast<const VgmCommands::End*>(pCommand); pEndMarker != nullptr)
            {
                s << "End of music data";
            }
            else if (auto* pDataBlock = dynamic_cast<const VgmCommands::DataBlock*>(pCommand); pDataBlock != nullptr)
            {
                s << std::format(
                    "Data block: type {:02x} length {}",
                    pDataBlock->type(),
                    pDataBlock->length());
            }
            else
            {
                s << "Unknown command";
            }
            break;
        case VgmHeader::Chip::SN76489:
            s << "SN76489: ";
            psgState.add_with_text(pCommand, s);
            break;
        case VgmHeader::Chip::YM2413:
            s << "YM2413: ";
            ym2413State.add_with_text(pCommand, s);
            break;
        case VgmHeader::Chip::YM2612:
            s << "YM2612";
            break;
        case VgmHeader::Chip::YM2151: break;
        case VgmHeader::Chip::SegaPCM: break;
        case VgmHeader::Chip::RF5C68: break;
        case VgmHeader::Chip::YM2203: break;
        case VgmHeader::Chip::YM2608: break;
        case VgmHeader::Chip::YM2610: break;
        case VgmHeader::Chip::YM3812: break;
        case VgmHeader::Chip::YM3526: break;
        case VgmHeader::Chip::Y8950: break;
        case VgmHeader::Chip::YMF262: break;
        case VgmHeader::Chip::YMF278B: break;
        case VgmHeader::Chip::YMF271: break;
        case VgmHeader::Chip::YMZ280B: break;
        case VgmHeader::Chip::RF5C164: break;
        case VgmHeader::Chip::PWM: break;
        case VgmHeader::Chip::AY8910: break;
        case VgmHeader::Chip::GenericDAC: break;
        default:
            break;
        }
        s << "\n";
    }

    if (!_gd3Tag.empty())
    {
        s << "\nGD3 tag:\n"
            << _gd3Tag.write_to_text();
    }

    callback.show_status("Write to text complete");
}
