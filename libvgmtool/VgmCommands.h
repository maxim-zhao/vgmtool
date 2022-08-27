#pragma once
#include "BinaryData.h"

namespace VgmCommands
{
    class ICommand
    {
    public:
        virtual ~ICommand() = default;
        virtual void from_data(BinaryData& data) = 0;
        virtual void to_data(BinaryData& data) const = 0;
    };

    template <uint8_t i>
    class IMarked
    {
    public:
        virtual ~IMarked() = default;

        static constexpr uint8_t get_marker_static()
        {
            return i;
        }

        [[nodiscard]] virtual uint8_t get_marker() const
        {
            return i;
        }
    };

    class OneByteCommand : public ICommand
    {
        uint8_t _data = 0;
    public:
        explicit OneByteCommand(BinaryData& data);

        [[nodiscard]] uint8_t data() const;
        void set_data(uint8_t data);
        void from_data(BinaryData& data) override;
        void to_data(BinaryData& data) const override;
    };

    class GGStereo : public OneByteCommand, public IMarked<0x4f>
    {
    public:
        explicit GGStereo(BinaryData& data): OneByteCommand(data) {}
    };

    class SN76489 : public OneByteCommand, public IMarked<0x50>
    {
    public:
        explicit SN76489(BinaryData& data)
            : OneByteCommand(data) {}
    };

    class AddressDataCommand : public ICommand
    {
        uint8_t _register = 0;
        uint8_t _data = 0;
    public:
        explicit AddressDataCommand(BinaryData& data);

        [[nodiscard]] uint8_t register_() const;
        void set_register(uint8_t register_);
        [[nodiscard]] uint8_t data() const;
        void set_data(uint8_t data);

        void from_data(BinaryData& data) override;
        void to_data(BinaryData& data) const override;
    };

    class YM2413 : public AddressDataCommand, public IMarked<0x51>
    {
    public:
        explicit YM2413(BinaryData& data)
            : AddressDataCommand(data) {}
    };

    class YM2612Port0 : public AddressDataCommand, public IMarked<0x52>
    {
    public:
        explicit YM2612Port0(BinaryData& data)
            : AddressDataCommand(data) {}
    };

    class YM2612Port1 : public AddressDataCommand, public IMarked<0x53>
    {
    public:
        explicit YM2612Port1(BinaryData& data)
            : AddressDataCommand(data) {}
    };

    class YM2151 : public AddressDataCommand, public IMarked<0x54>
    {
    public:
        explicit YM2151(BinaryData& data)
            : AddressDataCommand(data) {}
    };

    class YM2203 : public AddressDataCommand, public IMarked<0x55>
    {
    public:
        explicit YM2203(BinaryData& data)
            : AddressDataCommand(data) {}
    };

    class YM2608Port0 : public AddressDataCommand, public IMarked<0x56>
    {
    public:
        explicit YM2608Port0(BinaryData& data)
            : AddressDataCommand(data) {}
    };

    class YM2608Port1 : public AddressDataCommand, public IMarked<0x57>
    {
    public:
        explicit YM2608Port1(BinaryData& data)
            : AddressDataCommand(data) {}
    };

    class YM2610Port0 : public AddressDataCommand, public IMarked<0x58>
    {
    public:
        explicit YM2610Port0(BinaryData& data)
            : AddressDataCommand(data) {}
    };

    class YM2610Port1 : public AddressDataCommand, public IMarked<0x59>
    {
    public:
        explicit YM2610Port1(BinaryData& data)
            : AddressDataCommand(data) {}
    };

    class YM3812 : public AddressDataCommand, public IMarked<0x5a>
    {
    public:
        explicit YM3812(BinaryData& data)
            : AddressDataCommand(data) {}
    };

    class YM3526 : public AddressDataCommand, public IMarked<0x5b>
    {
    public:
        explicit YM3526(BinaryData& data)
            : AddressDataCommand(data) {}
    };

    class Y8950 : public AddressDataCommand, public IMarked<0x5c>
    {
    public:
        explicit Y8950(BinaryData& data)
            : AddressDataCommand(data) {}
    };

    class YMZ280B : public AddressDataCommand, public IMarked<0x5d>
    {
    public:
        explicit YMZ280B(BinaryData& data)
            : AddressDataCommand(data) {}
    };

    class YMF262Port0 : public AddressDataCommand, public IMarked<0x5e>
    {
    public:
        explicit YMF262Port0(BinaryData& data)
            : AddressDataCommand(data) {}
    };

    class YMF262Port1 : public AddressDataCommand, public IMarked<0x5f>
    {
    public:
        explicit YMF262Port1(BinaryData& data)
            : AddressDataCommand(data) {}
    };

    class IWait
    {
    public:
        virtual ~IWait() = default;
        [[nodiscard]] virtual uint16_t duration() const = 0;
    };

    class Wait : public ICommand, public IWait, public IMarked<0x61>
    {
        uint16_t _duration = 0;
    public:
        explicit Wait(BinaryData& data);

        [[nodiscard]] uint16_t duration() const override;
        void set_duration(uint16_t duration);

        void from_data(BinaryData& data) override;
        void to_data(BinaryData& data) const override;
    };

    class NoDataCommand : public ICommand
    {
    public:
        void from_data(BinaryData&) override {}
        void to_data(BinaryData&) const override {}
    };

    class Wait60th : public NoDataCommand, public IWait, public IMarked<0x62>
    {
    public:
        explicit Wait60th(BinaryData&) { }

        [[nodiscard]] uint16_t duration() const override
        {
            return 44100 / 60;
        }
    };

    class Wait50th : public NoDataCommand, public IWait, public IMarked<0x63>
    {
    public:
        explicit Wait50th(BinaryData&) { }

        [[nodiscard]] uint16_t duration() const override
        {
            return 44100 / 50;
        }
    };

    template <int marker>
    class WaitN : public NoDataCommand, public IWait, public IMarked<marker>
    {
    public:
        explicit WaitN(BinaryData&) { }

        [[nodiscard]] uint16_t duration() const override
        {
            return (marker & 1) + 1;
        }
    };

    class End : public NoDataCommand, public IMarked<0x66>
    {
    public:
        End(BinaryData&) { }
    };

    class DataBlock : public ICommand, public IMarked<0x67>
    {
    public:
        explicit DataBlock(BinaryData& data);
        void from_data(BinaryData& data) override;
        void to_data(BinaryData& data) const override;
        uint8_t _type{};
        BinaryData _data;
    };

    class PcmRamWrite : public ICommand, public IMarked<0x68>
    {
    public:
        explicit PcmRamWrite(BinaryData& data);
        void from_data(BinaryData& data) override;
        void to_data(BinaryData& data) const override;
        uint8_t _chipType{};
        uint32_t _sourceOffset{};
        uint32_t _destinationOffset{};
        uint32_t _size{};
    };

    template <int marker>
    class YM2612Sample : public NoDataCommand, public IWait, public IMarked<marker>
    {
    public:
        explicit YM2612Sample(BinaryData&) { }

        [[nodiscard]] uint16_t duration() const override
        {
            return (marker & 1) + 1;
        }
    };

    class LoopPoint : public NoDataCommand {};
};
