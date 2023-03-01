#pragma once
#include "BinaryData.h"

namespace VgmCommands
{
    // Base type for all commands, so we can hold them by pointers to base
    class ICommand
    {
    public:
        ICommand() = default;
        virtual ~ICommand() = default;

        ICommand(const ICommand& other) = delete;
        ICommand(ICommand&& other) noexcept = delete;
        ICommand& operator=(const ICommand& other) = delete;
        ICommand& operator=(ICommand&& other) noexcept = delete;

        virtual void from_data(BinaryData& data) = 0;
        virtual void to_data(BinaryData& data) const = 0;

        [[nodiscard]] virtual VgmHeader::Chip chip() const = 0;
    };

    // A command that has a static "marker" byte
    class MarkedCommand : public ICommand
    {
        uint8_t _marker = 0;
        VgmHeader::Chip _chip;

    public:
        MarkedCommand(uint8_t marker, VgmHeader::Chip chip) : _marker(marker), _chip(chip) {}

        [[nodiscard]] virtual uint8_t get_marker() const
        {
            return _marker;
        }

        [[nodiscard]] VgmHeader::Chip chip() const override
        {
            return _chip;
        }

    protected:
        void check_marker(BinaryData& data) const;
    };

    class NoDataCommand : public MarkedCommand
    {
    public:
        NoDataCommand(uint8_t marker, VgmHeader::Chip chip): MarkedCommand(marker, chip) {}

        void from_data(BinaryData& data) override;
        void to_data(BinaryData& data) const override;
    };

    // A command with a marker byte and one value byte
    class OneByteCommand : public MarkedCommand
    {
        uint8_t _value = 0;
    public:
        OneByteCommand(uint8_t marker, VgmHeader::Chip chip): MarkedCommand(marker, chip) {}

        [[nodiscard]] uint8_t value() const;
        void set_value(uint8_t value);
        void from_data(BinaryData& data) override;
        void to_data(BinaryData& data) const override;
    };

    // A command that has a marker, followed by a register number and a value byte
    class RegisterDataCommand : public MarkedCommand
    {
        uint8_t _register = 0;
        uint8_t _value = 0;
    public:
        RegisterDataCommand(uint8_t marker, VgmHeader::Chip chip): MarkedCommand(marker, chip) {}

        [[nodiscard]] uint8_t registerIndex() const;
        void set_register(uint8_t register_);
        [[nodiscard]] uint8_t value() const;
        void set_value(uint8_t value);

        void from_data(BinaryData& data) override;
        void to_data(BinaryData& data) const override;
    };

    // A command that has a marker, followed by a port number, a register number and a value byte
    class PortRegisterDataCommand : public MarkedCommand
    {
        uint8_t _port = 0;
        uint8_t _register = 0;
        uint8_t _value = 0;
    public:
        PortRegisterDataCommand(uint8_t marker, VgmHeader::Chip chip): MarkedCommand(marker, chip) {}

        [[nodiscard]] uint8_t port() const;
        void set_port(uint8_t port);
        [[nodiscard]] uint8_t register_() const;
        void set_register(uint8_t register_);
        [[nodiscard]] uint8_t value() const;
        void set_value(uint8_t value);

        void from_data(BinaryData& data) override;
        void to_data(BinaryData& data) const override;
    };

    // A command that has a marker, followed by a 16-bit address and a value byte
    class AddressDataCommand : public MarkedCommand
    {
        uint16_t _address = 0;
        uint8_t _value = 0;
    public:
        AddressDataCommand(uint8_t marker, VgmHeader::Chip chip): MarkedCommand(marker, chip) {}

        [[nodiscard]] uint16_t address() const;
        void set_address(uint16_t address);
        [[nodiscard]] uint8_t value() const;
        void set_value(uint8_t value);

        void from_data(BinaryData& data) override;
        void to_data(BinaryData& data) const override;
    };

    class GGStereo : public OneByteCommand
    {
    public:
        GGStereo() : OneByteCommand(0x4f, VgmHeader::Chip::SN76489) {}
    };

    class SN76489 : public OneByteCommand
    {
    public:
        SN76489(): OneByteCommand(0x50, VgmHeader::Chip::SN76489) {}
    };

    class YM2413 : public RegisterDataCommand
    {
    public:
        YM2413(): RegisterDataCommand(0x51, VgmHeader::Chip::YM2413) {}
    };

    class YM2612Port0 : public RegisterDataCommand
    {
    public:
        YM2612Port0(): RegisterDataCommand(0x52, VgmHeader::Chip::YM2612) {}
    };

    class YM2612Port1 : public RegisterDataCommand
    {
    public:
        YM2612Port1(): RegisterDataCommand(0x53, VgmHeader::Chip::YM2612) {}
    };

    class YM2151 : public RegisterDataCommand
    {
    public:
        YM2151(): RegisterDataCommand(0x54, VgmHeader::Chip::YM2151) {}
    };

    class YM2203 : public RegisterDataCommand
    {
    public:
        YM2203(): RegisterDataCommand(0x55, VgmHeader::Chip::YM2203) {}
    };

    class YM2608Port0 : public RegisterDataCommand
    {
    public:
        YM2608Port0(): RegisterDataCommand(0x56, VgmHeader::Chip::YM2608) {}
    };

    class YM2608Port1 : public RegisterDataCommand
    {
    public:
        YM2608Port1(): RegisterDataCommand(0x57, VgmHeader::Chip::YM2608) {}
    };

    class YM2610Port0 : public RegisterDataCommand
    {
    public:
        YM2610Port0(): RegisterDataCommand(0x58, VgmHeader::Chip::YM2610) {}
    };

    class YM2610Port1 : public RegisterDataCommand
    {
    public:
        YM2610Port1(): RegisterDataCommand(0x59, VgmHeader::Chip::YM2610) {}
    };

    class YM3812 : public RegisterDataCommand
    {
    public:
        YM3812(): RegisterDataCommand(0x5a, VgmHeader::Chip::YM3812) {}
    };

    class YM3526 : public RegisterDataCommand
    {
    public:
        YM3526(): RegisterDataCommand(0x5b, VgmHeader::Chip::YM3526) {}
    };

    class Y8950 : public RegisterDataCommand
    {
    public:
        Y8950(): RegisterDataCommand(0x5c, VgmHeader::Chip::Y8950) {}
    };

    class YMZ280B : public RegisterDataCommand
    {
    public:
        YMZ280B(): RegisterDataCommand(0x5d, VgmHeader::Chip::YMZ280B) {}
    };

    class YMF262Port0 : public RegisterDataCommand
    {
    public:
        YMF262Port0(): RegisterDataCommand(0x5e, VgmHeader::Chip::YMF262) {}
    };

    class YMF262Port1 : public RegisterDataCommand
    {
    public:
        YMF262Port1(): RegisterDataCommand(0x5f, VgmHeader::Chip::YMF262) {}
    };

    class Wait
    {
    protected:
        uint16_t _duration = 0;
    public:
        [[nodiscard]] uint16_t duration() const
        {
            return _duration;
        }
    };

    class Wait16bit : public MarkedCommand, public Wait
    {
    public:
        explicit Wait16bit(): MarkedCommand(0x61, VgmHeader::Chip::Nothing) {}

        void set_duration(uint16_t duration);

        void from_data(BinaryData& data) override;
        void to_data(BinaryData& data) const override;
    };

    class Wait60th : public NoDataCommand, public Wait
    {
    public:
        Wait60th();
    };

    class Wait50th : public NoDataCommand, public Wait
    {
    public:
        Wait50th();
    };

    class Wait4bit : public Wait, public ICommand
    {
        int _sampleCount = 0;
    public:
        [[nodiscard]] int sample_count() const
        {
            return _sampleCount;
        }

        void set_sample_count(int sampleCount);

        void from_data(BinaryData& data) override;
        void to_data(BinaryData& data) const override;

        [[nodiscard]] VgmHeader::Chip chip() const override
        {
            return VgmHeader::Chip::Nothing;
        }
    };

    class End : public MarkedCommand
    {
    public:
        End(): MarkedCommand(0x66, VgmHeader::Chip::Nothing) {}

        void from_data(BinaryData& data) override;
        void to_data(BinaryData& data) const override;
    };

    class DataBlock : public MarkedCommand
    {
    public:
        DataBlock(): MarkedCommand(0x67, VgmHeader::Chip::Nothing) {}

        void from_data(BinaryData& data) override;
        void to_data(BinaryData& data) const override;
    private:
        uint8_t _type{};
        BinaryData _data;
    };

    class PcmRamWrite : public MarkedCommand
    {
    public:
        PcmRamWrite(): MarkedCommand(0x68, VgmHeader::Chip::SegaPCM) {}

        void from_data(BinaryData& data) override;
        void to_data(BinaryData& data) const override;
    private:
        uint8_t _chipType{};
        uint32_t _sourceOffset{};
        uint32_t _destinationOffset{};
        uint32_t _size{};
    };

    class YM2612Sample : public Wait, public ICommand
    {
    public:
        void from_data(BinaryData& data) override;
        void to_data(BinaryData& data) const override;

        [[nodiscard]] VgmHeader::Chip chip() const override
        {
            return VgmHeader::Chip::Nothing;
        }
    };

    class DacStreamControlSetup : public MarkedCommand
    {
        uint8_t _streamId{};
        uint8_t _chipType{};
        bool _useSecondChip{};
        uint8_t _port{};
        uint8_t _command{};
    public:
        DacStreamControlSetup(): MarkedCommand(0x90, VgmHeader::Chip::GenericDAC) {}

        void from_data(BinaryData& data) override;
        void to_data(BinaryData& data) const override;
    };

    class DacStreamSetData : public MarkedCommand
    {
        uint8_t _streamId{};
        uint8_t _dataBankId{};
        uint8_t _stepSize{};
        uint8_t _stepBase{};
    public:
        DacStreamSetData(): MarkedCommand(0x91, VgmHeader::Chip::GenericDAC) {}

        void from_data(BinaryData& data) override;
        void to_data(BinaryData& data) const override;
    };

    class DacStreamSetFrequency : public MarkedCommand
    {
        uint8_t _streamId{};
        uint32_t _frequency{};
    public:
        DacStreamSetFrequency(): MarkedCommand(0x92, VgmHeader::Chip::GenericDAC) {}

        void from_data(BinaryData& data) override;
        void to_data(BinaryData& data) const override;
    };

    class DacStreamStart : public MarkedCommand
    {
        uint8_t _streamId{};
        uint32_t _dataStartOffset{};
        uint8_t _lengthMode{};
        uint32_t _dataLength{};
    public:
        DacStreamStart(): MarkedCommand(0x93, VgmHeader::Chip::GenericDAC) {}

        void from_data(BinaryData& data) override;
        void to_data(BinaryData& data) const override;
    };

    class DacStreamStop : public MarkedCommand
    {
        uint8_t _streamId{};
    public:
        DacStreamStop(): MarkedCommand(0x94, VgmHeader::Chip::GenericDAC) {}

        void from_data(BinaryData& data) override;
        void to_data(BinaryData& data) const override;
    };

    class DacStreamStartFast : public MarkedCommand
    {
        uint8_t _streamId{};
        uint16_t _blockId{};
        uint8_t _flags{};
    public:
        DacStreamStartFast(): MarkedCommand(0x95, VgmHeader::Chip::GenericDAC) {}

        void from_data(BinaryData& data) override;
        void to_data(BinaryData& data) const override;
    };

    class AY8910 : public RegisterDataCommand
    {
    public:
        AY8910(): RegisterDataCommand(0xa0, VgmHeader::Chip::AY8910) {}
    };

    class RF5C68Register : public RegisterDataCommand
    {
    public:
        RF5C68Register(): RegisterDataCommand(0xb0, VgmHeader::Chip::RF5C68) {}
    };

    class RF5C164Register : public RegisterDataCommand
    {
    public:
        RF5C164Register(): RegisterDataCommand(0xb1, VgmHeader::Chip::RF5C164) {}
    };

    class PWM : public MarkedCommand
    {
        int _register{};
        int _value{};
    public:
        PWM(): MarkedCommand(0xb2, VgmHeader::Chip::PWM) {}
        void from_data(BinaryData& data) override;
        void to_data(BinaryData& data) const override;
    };

    class SegaPCM : public AddressDataCommand
    {
    public:
        SegaPCM(): AddressDataCommand(0xc0, VgmHeader::Chip::SegaPCM) {}
    };

    class RF5C68Memory : public AddressDataCommand
    {
    public:
        RF5C68Memory(): AddressDataCommand(0xc1, VgmHeader::Chip::RF5C68) {}
    };

    class RF5C164Memory : public AddressDataCommand
    {
    public:
        RF5C164Memory(): AddressDataCommand(0xc2, VgmHeader::Chip::RF5C164) {}
    };

    class YMF278B : public PortRegisterDataCommand
    {
    public:
        YMF278B(): PortRegisterDataCommand(0xd0, VgmHeader::Chip::YMF278B) {}
    };

    class YMF271 : public PortRegisterDataCommand
    {
    public:
        YMF271(): PortRegisterDataCommand(0xd1, VgmHeader::Chip::YMF271) {}
    };

    class PCMSeek : public MarkedCommand
    {
        uint32_t _address;
    public:
        PCMSeek(): MarkedCommand(0xe0, VgmHeader::Chip::SegaPCM), _address(0) {}
        void from_data(BinaryData& data) override;
        void to_data(BinaryData& data) const override;
    };

    // "Dual Chip" commands

    class GGStereo_Second : public OneByteCommand
    {
    public:
        GGStereo_Second() : OneByteCommand(0x3f, VgmHeader::Chip::SN76489) {}
    };

    class SN76489_Second : public OneByteCommand
    {
    public:
        SN76489_Second(): OneByteCommand(0x30, VgmHeader::Chip::SN76489) {}
    };

    class YM2413_Second : public RegisterDataCommand
    {
    public:
        YM2413_Second(): RegisterDataCommand(0xa1, VgmHeader::Chip::YM2413) {}
    };

    class YM2612Port0_Second : public RegisterDataCommand
    {
    public:
        YM2612Port0_Second(): RegisterDataCommand(0xa2, VgmHeader::Chip::YM2612) {}
    };

    class YM2612Port1_Second : public RegisterDataCommand
    {
    public:
        YM2612Port1_Second(): RegisterDataCommand(0xa3, VgmHeader::Chip::YM2612) {}
    };

    class YM2151_Second : public RegisterDataCommand
    {
    public:
        YM2151_Second(): RegisterDataCommand(0xa4, VgmHeader::Chip::YM2151) {}
    };

    class YM2203_Second : public RegisterDataCommand
    {
    public:
        YM2203_Second(): RegisterDataCommand(0xa5, VgmHeader::Chip::YM2203) {}
    };

    class YM2608Port0_Second : public RegisterDataCommand
    {
    public:
        YM2608Port0_Second(): RegisterDataCommand(0xa6, VgmHeader::Chip::YM2608) {}
    };

    class YM2608Port1_Second : public RegisterDataCommand
    {
    public:
        YM2608Port1_Second(): RegisterDataCommand(0xa7, VgmHeader::Chip::YM2608) {}
    };

    class YM2610Port0_Second : public RegisterDataCommand
    {
    public:
        YM2610Port0_Second(): RegisterDataCommand(0xa8, VgmHeader::Chip::YM2608) {}
    };

    class YM2610Port1_Second : public RegisterDataCommand
    {
    public:
        YM2610Port1_Second(): RegisterDataCommand(0xa9, VgmHeader::Chip::YM2610) {}
    };

    class YM3812_Second : public RegisterDataCommand
    {
    public:
        YM3812_Second(): RegisterDataCommand(0xaa, VgmHeader::Chip::YM2610) {}
    };

    class YM3526_Second : public RegisterDataCommand
    {
    public:
        YM3526_Second(): RegisterDataCommand(0xab, VgmHeader::Chip::YM3526) {}
    };

    class Y8950_Second : public RegisterDataCommand
    {
    public:
        Y8950_Second(): RegisterDataCommand(0xac, VgmHeader::Chip::Y8950) {}
    };

    class YMZ280B_Second : public RegisterDataCommand
    {
    public:
        YMZ280B_Second(): RegisterDataCommand(0xad, VgmHeader::Chip::YMZ280B) {}
    };

    class YMF262Port0_Second : public RegisterDataCommand
    {
    public:
        YMF262Port0_Second(): RegisterDataCommand(0xae, VgmHeader::Chip::YMF262) {}
    };

    class YMF262Port1_Second : public RegisterDataCommand
    {
    public:
        YMF262Port1_Second(): RegisterDataCommand(0xaf, VgmHeader::Chip::YMF262) {}
    };

    // Reserved commands. N is the data byte count, so it produces/consumes N+1 bytes.
    template <int N>
    class ReservedCommand : public ICommand
    {
        std::vector<uint8_t> _data;
    public:
        void from_data(BinaryData& data) override
        {
            for (auto i = 0; i <= N; ++i)
            {
                _data.push_back(data.read_uint8());
            }
        }

        void to_data(BinaryData& data) const override
        {
            for (const auto b : _data)
            {
                data.write_uint8(b);
            }
        }

        [[nodiscard]] VgmHeader::Chip chip() const override
        {
            return VgmHeader::Chip::Nothing;
        }
    };

    class LoopPoint : public ICommand
    {
    public:
        void from_data(BinaryData&) override {}
        void to_data(BinaryData&) const override {}

        [[nodiscard]] VgmHeader::Chip chip() const override
        {
            return VgmHeader::Chip::Nothing;
        }
    };
};
