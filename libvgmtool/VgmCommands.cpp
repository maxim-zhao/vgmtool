#include "VgmCommands.h"

#include <format>
#include <stdexcept>

VgmCommands::OneByteCommand::OneByteCommand(BinaryData& data)
{
    OneByteCommand::from_data(data);
}

uint8_t VgmCommands::OneByteCommand::data() const
{
    return _data;
}

void VgmCommands::OneByteCommand::set_data(const uint8_t data)
{
    _data = data;
}

void VgmCommands::OneByteCommand::from_data(BinaryData& data)
{
    _data = data.read_uint8();
}

void VgmCommands::OneByteCommand::to_data(BinaryData& data) const
{
    data.write_uint8(_data);
}

VgmCommands::AddressDataCommand::AddressDataCommand(BinaryData& data)
{
    AddressDataCommand::from_data(data);
}

uint8_t VgmCommands::AddressDataCommand::register_() const
{
    return _register;
}

void VgmCommands::AddressDataCommand::set_register(const uint8_t register_)
{
    _register = register_;
}

uint8_t VgmCommands::AddressDataCommand::data() const
{
    return _data;
}

void VgmCommands::AddressDataCommand::set_data(const uint8_t data)
{
    _data = data;
}

void VgmCommands::AddressDataCommand::from_data(BinaryData& data)
{
    _register = data.read_uint8();
    _data = data.read_uint8();
}

void VgmCommands::AddressDataCommand::to_data(BinaryData& data) const
{
    data.write_uint8(_register);
    data.write_uint8(_data);
}

VgmCommands::Wait::Wait(BinaryData& data)
{
    Wait::from_data(data);
}

void VgmCommands::Wait::from_data(BinaryData& data)
{
    _duration = data.read_uint16();
}

void VgmCommands::Wait::to_data(BinaryData& data) const
{
    data.write_uint16(_duration);
}

uint16_t VgmCommands::Wait::duration() const
{
    return _duration;
}

void VgmCommands::Wait::set_duration(const uint16_t duration)
{
    _duration = duration;
}

VgmCommands::DataBlock::DataBlock(BinaryData& data)
{
    DataBlock::from_data(data);
}

void VgmCommands::DataBlock::from_data(BinaryData& data)
{
    if (const auto marker = data.read_uint8(); marker != 0x66)
    {
        throw std::runtime_error(std::format("Invalid dummy marker value 0x{:02x} should be 0x66", marker));
    }
    _type = data.read_uint8();
    const auto size = data.read_uint32();
    _data.write_range(data.read_range(size));
}

void VgmCommands::DataBlock::to_data(BinaryData& data) const
{
    data.write_uint8(0x66);
    data.write_uint32(_data.size());
    data.write_range(_data.buffer());
}

VgmCommands::PcmRamWrite::PcmRamWrite(BinaryData& data)
{
    PcmRamWrite::from_data(data);
}

void VgmCommands::PcmRamWrite::from_data(BinaryData& data)
{
    /*
    VGM command 0x68 specifies a PCM RAM write. These are used to write data from
    data blocks to the RAM of a PCM chip. The data block format is:

      0x68 0x66 cc oo oo oo dd dd dd ss ss ss

    where:
      0x68 = VGM command
      0x66 = compatibility command to make older players stop parsing the stream
      cc   = chip type (equal to data block types 00..3F)
      oo oo oo (24 bits) = read offset in data block
      dd dd dd (24 bits) = write offset in chip's ram (affected by chip's
                            registers)
      ss ss ss (24 bits) = size of data, in bytes
        Since size can't be zero, a size of 0 bytes means 0x0100 0000 bytes.

    All unknown types must be skipped by the player.
    */
    if (const auto marker = data.read_uint8(); marker != 0x66)
    {
        throw std::runtime_error(std::format("Invalid dummy marker value 0x{:02x} should be 0x66", marker));
    }
    _chipType = data.read_uint8();
    _sourceOffset = data.read_uint24();
    _destinationOffset = data.read_uint24();
    _size = data.read_uint24();
    if (_size == 0)
    {
        _size = 0x1000000;
    }
}

void VgmCommands::PcmRamWrite::to_data(BinaryData& data) const
{
    data.write_uint8(0x66);
    data.write_uint8(_chipType);
    data.write_uint24(_sourceOffset);
    data.write_uint24(_destinationOffset);
    data.write_uint24(_size);
}

