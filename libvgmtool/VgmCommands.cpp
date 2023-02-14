#include "VgmCommands.h"

#include <format>
#include <stdexcept>

void VgmCommands::MarkedCommand::check_marker(BinaryData& data) const
{
    if (const uint8_t marker = data.read_uint8(); marker != get_marker())
    {
        throw std::runtime_error(std::format("Read marker {:x} instead of expected {:x}", marker, get_marker()));
    }
}

void VgmCommands::NoDataCommand::from_data(BinaryData& data)
{
    check_marker(data);
}

void VgmCommands::NoDataCommand::to_data(BinaryData& data) const
{
    data.write_uint8(get_marker());
}

uint8_t VgmCommands::OneByteCommand::value() const
{
    return _value;
}

void VgmCommands::OneByteCommand::set_value(const uint8_t value)
{
    _value = value;
}

void VgmCommands::OneByteCommand::from_data(BinaryData& data)
{
    check_marker(data);
    _value = data.read_uint8();
}

void VgmCommands::OneByteCommand::to_data(BinaryData& data) const
{
    data.write_uint8(get_marker());
    data.write_uint8(_value);
}

uint8_t VgmCommands::RegisterDataCommand::register_() const
{
    return _register;
}

void VgmCommands::RegisterDataCommand::set_register(const uint8_t register_)
{
    _register = register_;
}

uint8_t VgmCommands::RegisterDataCommand::value() const
{
    return _value;
}

void VgmCommands::RegisterDataCommand::set_value(const uint8_t value)
{
    _value = value;
}

void VgmCommands::RegisterDataCommand::from_data(BinaryData& data)
{
    check_marker(data);
    _register = data.read_uint8();
    _value = data.read_uint8();
}

void VgmCommands::RegisterDataCommand::to_data(BinaryData& data) const
{
    data.write_uint8(get_marker());
    data.write_uint8(_register);
    data.write_uint8(_value);
}

uint8_t VgmCommands::PortRegisterDataCommand::port() const
{
    return _port;
}

void VgmCommands::PortRegisterDataCommand::set_port(uint8_t port)
{
    _port = port;
}

uint8_t VgmCommands::PortRegisterDataCommand::register_() const
{
    return _register;
}

void VgmCommands::PortRegisterDataCommand::set_register(uint8_t register_)
{
    _register = register_;
}

uint8_t VgmCommands::PortRegisterDataCommand::value() const
{
    return _value;
}

void VgmCommands::PortRegisterDataCommand::set_value(uint8_t value)
{
    _value = value;
}

void VgmCommands::PortRegisterDataCommand::from_data(BinaryData& data)
{
    check_marker(data);
    _port = data.read_uint8();
    _register = data.read_uint8();
    _value = data.read_uint8();
}

void VgmCommands::PortRegisterDataCommand::to_data(BinaryData& data) const
{
    data.write_uint8(get_marker());
    data.write_uint16(_port);
    data.write_uint16(_register);
    data.write_uint16(_value);
}

uint16_t VgmCommands::AddressDataCommand::address() const
{
    return _address;
}

void VgmCommands::AddressDataCommand::set_address(uint16_t address)
{
    _address = address;
}

uint8_t VgmCommands::AddressDataCommand::value() const
{
    return _value;
}

void VgmCommands::AddressDataCommand::set_value(uint8_t value)
{
    _value = value;
}

void VgmCommands::AddressDataCommand::from_data(BinaryData& data)
{
    check_marker(data);
    _address = data.read_uint16();
    _value = data.read_uint8();
}

void VgmCommands::AddressDataCommand::to_data(BinaryData& data) const
{
    data.write_uint8(get_marker());
    data.write_uint16(_address);
    data.write_uint8(_value);
}

void VgmCommands::Wait16bit::set_duration(uint16_t duration)
{
    _duration = duration;
}

void VgmCommands::Wait16bit::from_data(BinaryData& data)
{
    check_marker(data);
    _duration = data.read_uint16();
}

void VgmCommands::Wait16bit::to_data(BinaryData& data) const
{
    data.write_uint8(get_marker());
    data.write_uint16(_duration);
}

VgmCommands::Wait60th::Wait60th(): NoDataCommand(0x62, VgmHeader::Chip::Nothing)
{
    _duration = 44100 / 60;
}

VgmCommands::Wait50th::Wait50th(): NoDataCommand(0x63, VgmHeader::Chip::Nothing)
{
    _duration = 44100 / 50;
}

void VgmCommands::Wait4bit::set_sample_count(const int sampleCount)
{
    if (sampleCount <= 0 || sampleCount >= 16)
    {
        throw std::runtime_error(std::format("Length out of range 1..16: {}", sampleCount));
    }
    _sampleCount = sampleCount;
}

void VgmCommands::Wait4bit::from_data(BinaryData& data)
{
    const auto b = data.read_uint8();
    if ((b & 0xf0) != 0x70)
    {
        throw std::runtime_error(std::format("Read marker {:x} instead of expected 0x70..0x7f", b));
    }
    _sampleCount = (b & 0xf) + 1;
}

void VgmCommands::Wait4bit::to_data(BinaryData& data) const
{
    data.write_uint8(static_cast<uint8_t>((_sampleCount - 1) | 0x70));
}

void VgmCommands::End::from_data(BinaryData& data)
{
    check_marker(data);
}

void VgmCommands::End::to_data(BinaryData& data) const
{
    data.write_uint8(0x66);
}

void VgmCommands::DataBlock::from_data(BinaryData& data)
{
    check_marker(data);
    if (const auto dummyMarker = data.read_uint8(); dummyMarker != 0x66)
    {
        throw std::runtime_error(std::format("Invalid dummy marker value 0x{:02x} should be 0x66", dummyMarker));
    }
    _type = data.read_uint8();
    const auto size = data.read_uint32();
    _data.write_range(data.read_range(size));
}

void VgmCommands::DataBlock::to_data(BinaryData& data) const
{
    data.write_uint8(get_marker());
    data.write_uint8(0x66);
    data.write_uint32(_data.size());
    data.write_range(_data.buffer());
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
    data.write_uint8(get_marker());
    if (const auto dummyMarker = data.read_uint8(); dummyMarker != 0x66)
    {
        throw std::runtime_error(std::format("Invalid dummy marker value 0x{:02x} should be 0x66", dummyMarker));
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
    data.write_uint8(get_marker());
    data.write_uint8(0x66);
    data.write_uint8(_chipType);
    data.write_uint24(_sourceOffset);
    data.write_uint24(_destinationOffset);
    data.write_uint24(_size);
}

void VgmCommands::YM2612Sample::from_data(BinaryData& data)
{
    const auto b = data.read_uint8();
    if ((b & 0xf0) != 0x80)
    {
        throw std::runtime_error(std::format("Read marker {:x} instead of expected 0x70..0x7f", b));
    }
    _duration = b & 0xf;
}

void VgmCommands::YM2612Sample::to_data(BinaryData& data) const
{
    data.write_uint8(static_cast<uint8_t>(0x80 | _duration));
}

void VgmCommands::DacStreamControlSetup::from_data(BinaryData& data)
{
    // Setup Stream Control:
    //   0x90 ss tt pp cc
    //       ss = Stream ID
    //       tt = Chip Type (see clock-order at header, e.g. YM2612 = 0x02)
    //             bit 7 is used to select the 2nd chip
    //       pp cc = write command/register cc at port pp
    check_marker(data);
    _streamId = data.read_uint8();
    const auto b = data.read_uint8();
    _chipType = b & 0x7f;
    _useSecondChip = (b & 0x80) != 0;
    _port = data.read_uint8();
    _command = data.read_uint8();
}

void VgmCommands::DacStreamControlSetup::to_data(BinaryData& data) const
{
    data.write_uint8(get_marker());
    data.write_uint8(_streamId);
    data.write_uint8(_chipType | (_useSecondChip ? 0x80 : 0x00));
    data.write_uint8(_port);
    data.write_uint8(_command);
}

void VgmCommands::DacStreamSetData::from_data(BinaryData& data)
{
    /*Set Stream Data:
      0x91 ss dd ll bb
          ss = Stream ID
          dd = Data Bank ID (see data block types 0x00..0x3f)
          ll = Step Size (how many data is skipped after every write, usually 1)
                Set to 2, if you're using an interleaved stream (e.g. for
                 left/right channel).
          bb = Step Base (data offset added to the Start Offset when starting
                stream playback, usually 0)
                If you're using an interleaved stream, set it to 0 in one stream
                and to 1 in the other one.
          Note: Step Size/Step Step are given in command-data-size
                 (i.e. 1 for YM2612, 2 for PWM), not bytes
    */
    check_marker(data);
    _streamId = data.read_uint8();
    _dataBankId = data.read_uint8();
    _stepSize = data.read_uint8();
    _stepBase = data.read_uint8();
}

void VgmCommands::DacStreamSetData::to_data(BinaryData& data) const
{
    data.write_uint8(get_marker());
    data.write_uint8(_streamId);
    data.write_uint8(_dataBankId);
    data.write_uint8(_stepSize);
    data.write_uint8(_stepBase);
}

void VgmCommands::DacStreamSetFrequency::from_data(BinaryData& data)
{
    /*Set Stream Frequency:
      0x92 ss ff ff ff ff
          ss = Stream ID
          ff = Frequency (or Sample Rate, in Hz) at which the writes are done
    */
    check_marker(data);
    _streamId = data.read_uint8();
    _frequency = data.read_uint32();
}

void VgmCommands::DacStreamSetFrequency::to_data(BinaryData& data) const
{
    data.write_uint8(get_marker());
    data.write_uint8(_streamId);
    data.write_uint32(_frequency);
}

void VgmCommands::DacStreamStart::from_data(BinaryData& data)
{
    /*Start Stream:
      0x93 ss aa aa aa aa mm ll ll ll ll
          ss = Stream ID
          aa = Data Start offset in data bank (byte offset in data bank)
                Note: if set to -1, the Data Start offset is ignored
          mm = Length Mode (how the Data Length is calculated)
                00 - ignore (just change current data position)
                01 - length = number of commands
                02 - length in milliseconds
                03 - play until end of data
                8? - (bit 7) Loop (automatically restarts when finished)
          ll = Data Length
    */
    check_marker(data);
    _streamId = data.read_uint8();
    _dataStartOffset = data.read_uint32();
    _lengthMode = data.read_uint8();
    _dataLength = data.read_uint32();
}

void VgmCommands::DacStreamStart::to_data(BinaryData& data) const
{
    data.write_uint8(get_marker());
    data.write_uint8(_streamId);
    data.write_uint32(_dataStartOffset);
    data.write_uint8(_lengthMode);
    data.write_uint32(_dataLength);
}

void VgmCommands::DacStreamStop::from_data(BinaryData& data)
{
    /*Stop Stream:
      0x94 ss
          ss = Stream ID
                Note: 0xFF stops all streams*/
    check_marker(data);
    _streamId = data.read_uint8();
}

void VgmCommands::DacStreamStop::to_data(BinaryData& data) const
{
    data.write_uint8(get_marker());
    data.write_uint8(_streamId);
}

void VgmCommands::DacStreamStartFast::from_data(BinaryData& data)
{
    /*Start Stream (fast call):
      0x95 ss bb bb ff
          ss = Stream ID
          bb = Block ID (number of the data block that's part of the data bank set
                with command 0x91)
          ff = Flags
                bit 0 - Loop (see command 0x93)

    */
    check_marker(data);
    _streamId = data.read_uint8();
    _blockId = data.read_uint16();
    _flags = data.read_uint8();
}

void VgmCommands::DacStreamStartFast::to_data(BinaryData& data) const
{
    data.write_uint8(get_marker());
    data.write_uint8(_streamId);
    data.write_uint16(_blockId);
    data.write_uint8(_flags);
}

void VgmCommands::PWM::from_data(BinaryData& data)
{
    check_marker(data);
    const auto b1 = data.read_uint8();
    const auto b2 = data.read_uint8();
    _register = b1 >> 4;
    _value = b2 | ((b1 & 0x0f) << 8);
}

void VgmCommands::PWM::to_data(BinaryData& data) const
{
    data.write_uint8(get_marker());
    data.write_uint8(static_cast<uint8_t>(((_register & 0xf) << 4) | ((_value & 0xf00) >> 8)));
    data.write_uint8(static_cast<uint8_t>(_value & 0x0ff));
}

void VgmCommands::PCMSeek::from_data(BinaryData& data)
{
    check_marker(data);
    _address = data.read_uint32();
}

void VgmCommands::PCMSeek::to_data(BinaryData& data) const
{
    data.write_uint8(get_marker());
    data.write_uint32(_address);
}
