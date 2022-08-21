#include "VgmCommands.h"

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
