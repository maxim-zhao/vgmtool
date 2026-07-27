#include "VgmFile.h"

#include <format>
#include <numeric>
#include <stdexcept>
#include <ranges>

#include "BinaryData.h"
#include "IStatusCallback.h"
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

    data.seek(dataOffset);

    // If we have a loop offset then load in two parts.
    // If not, leave the "loop" empty.
    if (_header.loop_offset() != 0)
    {
        _dataBeforeLoop.from_data(data, _header.loop_offset(), false);
        _dataWithLoop.from_data(data, endOffset, true);
    }
    else
    {
        _dataBeforeLoop.from_data(data, endOffset, true);
    }

    // Check for orphaned data
    if (data.offset() < endOffset)
    {
        throw std::runtime_error(std::format("Unconsumed data in VGM file at offset {:x}", data.offset()));
    }
}

void VgmFile::save_file(const std::string& filename, const IStatusCallback& callback, const bool verboseZopfli, const int compression)
{
    BinaryData data;

    // First the header TODO write this last, but we need to know its size
    _header.to_binary(data);

    // Then the data
    // TODO if the header size changes then the pointers need to be rewritten
    _header.set_data_offset(data.size());
    _dataBeforeLoop.to_binary(data);
    if (_dataWithLoop.commands().empty())
    {
        // No loop
        _header.set_loop_offset(0);
    }
    else
    {
        // We have a loop
        _header.set_loop_offset(data.size());
        _dataWithLoop.to_binary(data);
    }

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

    if (compression > 0)
    {
        data.compress(compression, callback, verboseZopfli);
    }

    // Finally, save to disk.
    data.save(filename);
}

void VgmFile::check_header(const bool fix, const IStatusCallback& callback)
{
    callback.verbose_message("Checking lengths...");
    // Check lengths
    auto countWaits = [](const CommandStream& stream)
    {
        // Make a view that is all the wait commands
        auto waits = stream.commands() 
            | std::ranges::views::transform([](const auto& x) { return std::dynamic_pointer_cast<const VgmCommands::Wait>(x); })
            | std::ranges::views::filter([](const auto& x) { return x != nullptr; });
        // Then accumulate all of their durations
        return std::accumulate(
            waits.begin(), 
            waits.end(), 
            0u, 
            [](auto acc, const auto& pWait) { return acc + pWait->duration(); });
    };

    auto loopSampleCount = countWaits(_dataWithLoop);
    auto totalSampleCount = countWaits(_dataBeforeLoop) + loopSampleCount;

    auto message = std::format(
                "Lengths:\n"
                "In file:\n"
                "Total: {} samples = {:.3f} seconds\n"
                "Loop: {} samples = {:.3f} seconds\n"
                "In header:\n"
                "Total: {} samples = {:.3f} seconds\n"
                "Loop: {} samples = {:.3f} seconds",
                _header.sample_count(), _header.sample_count() / 44100.0,
                _header.loop_sample_count(), _header.loop_sample_count() / 44100.0,
                totalSampleCount, totalSampleCount / 44100.0,
                loopSampleCount, loopSampleCount / 44100.0);
    callback.verbose_message(message);

    if (_header.loop_sample_count() != loopSampleCount || _header.sample_count() != totalSampleCount)
    {
        if (fix)
        {
            _header.set_sample_count(totalSampleCount);
            _header.set_loop_sample_count(loopSampleCount);
        }
        else
        {
            throw std::runtime_error(message);
        }
    }
}

void VgmFile::write_command_as_text(std::ostream& s, size_t& offset, int& time, SN76489State& psgState, YM2413State& ym2413State, const std::shared_ptr<const VgmCommands::ICommand>& pCommand)
{
    // File offset
    s << std::format("{:#010x} ", offset);
    // Get data so we can print it raw
    BinaryData scratch;
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
    case Chip::Nothing:
        if (const auto pWait = std::dynamic_pointer_cast<const VgmCommands::Wait>(pCommand))
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
            if (auto pSample = std::dynamic_pointer_cast<const VgmCommands::YM2612Sample>(pCommand))
            {
                s << "; emit sample";
            }
        }
        else if (auto pEndMarker = std::dynamic_pointer_cast<const VgmCommands::End>(pCommand))
        {
            s << "End of music data";
        }
        else if (auto pDataBlock = std::dynamic_pointer_cast<const VgmCommands::DataBlock>(pCommand))
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
    case Chip::SN76489:
        s << "SN76489: ";
        psgState.to_text(s, pCommand);
        break;
    case Chip::YM2413:
        s << "YM2413: ";
        ym2413State.to_text(pCommand, s);
        break;
    case Chip::YM2612:
        s << "YM2612";
        break;
    case Chip::YM2151: break;
    case Chip::SegaPCM: break;
    case Chip::RF5C68: break;
    case Chip::YM2203: break;
    case Chip::YM2608: break;
    case Chip::YM2610: break;
    case Chip::YM3812: break;
    case Chip::YM3526: break;
    case Chip::Y8950: break;
    case Chip::YMF262: break;
    case Chip::YMF278B: break;
    case Chip::YMF271: break;
    case Chip::YMZ280B: break;
    case Chip::RF5C164: break;
    case Chip::PWM: break;
    case Chip::AY8910: break;
    case Chip::GenericDAC: break;
    default:
        break;
    }
    s << "\n";
}

void VgmFile::write_to_text(std::ostream& s, const IStatusCallback& callback) const
{
    callback.verbose_message("Converting to text...");
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

    for (const auto& pCommand : _dataBeforeLoop.commands())
    {
        write_command_as_text(s, offset, time, psgState, ym2413State, pCommand);
    }

    if (!_dataWithLoop.commands().empty())
    {
        s << "=============== LOOP POINT ===============\n";
        for (const auto& pCommand : _dataWithLoop.commands())
        {
            write_command_as_text(s, offset, time, psgState, ym2413State, pCommand);
        }
    }

    if (!_gd3Tag.empty())
    {
        s << "\nGD3 tag:\n"
            << _gd3Tag.write_to_text();
    }

    callback.verbose_message("Write to text complete");
}
