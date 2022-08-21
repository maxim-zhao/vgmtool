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
        virtual uint8_t get_marker() = 0;
    };

    class OneByteCommand : public ICommand
    {
        uint8_t _data = 0;
    public:
        [[nodiscard]] uint8_t data() const;
        void set_data(uint8_t data);
        void from_data(BinaryData& data) override;
        void to_data(BinaryData& data) const override;
    };

    class GGStereo : public OneByteCommand
    {
    public:
        uint8_t get_marker() override
        {
            return 0x4f;
        }
    };

    class SN76489 : public OneByteCommand
    {
    public:
        uint8_t get_marker() override
        {
            return 0x50;
        }
    };

    class AddressDataCommand : public ICommand
    {
        uint8_t _register = 0;
        uint8_t _data = 0;
    public:
        [[nodiscard]] uint8_t register_() const;
        void set_register(uint8_t register_);
        [[nodiscard]] uint8_t data() const;
        void set_data(uint8_t data);

        void from_data(BinaryData& data) override;
        void to_data(BinaryData& data) const override;
    };

    class YM2413 : public AddressDataCommand
    {
    public:
        uint8_t get_marker() override
        {
            return 0x51;
        }
    };

    class YM2612Port0 : public AddressDataCommand
    {
    public:
        uint8_t get_marker() override
        {
            return 0x52;
        }
    };

    class YM2612Port1 : public AddressDataCommand
    {
    public:
        uint8_t get_marker() override
        {
            return 0x53;
        }
    };

    class YM2151 : public AddressDataCommand
    {
    public:
        uint8_t get_marker() override
        {
            return 0x54;
        }
    };

    class YM2203 : public AddressDataCommand
    {
    public:
        uint8_t get_marker() override
        {
            return 0x55;
        }
    };

    class YM2608Port0 : public AddressDataCommand
    {
    public:
        uint8_t get_marker() override
        {
            return 0x56;
        }
    };

    class YM2608Port1 : public AddressDataCommand
    {
    public:
        uint8_t get_marker() override
        {
            return 0x57;
        }
    };

    class YM2610Port0 : public AddressDataCommand
    {
    public:
        uint8_t get_marker() override
        {
            return 0x58;
        }
    };

    class YM2610Port1 : public AddressDataCommand
    {
    public:
        uint8_t get_marker() override
        {
            return 0x59;
        }
    };

    class YM3812 : public AddressDataCommand
    {
    public:
        uint8_t get_marker() override
        {
            return 0x5a;
        }
    };

    class YM3526 : public AddressDataCommand
    {
    public:
        uint8_t get_marker() override
        {
            return 0x5b;
        }
    };

    class Y8950 : public AddressDataCommand
    {
    public:
        uint8_t get_marker() override
        {
            return 0x5c;
        }
    };

    class YMZ280B : public AddressDataCommand
    {
    public:
        uint8_t get_marker() override
        {
            return 0x5d;
        }
    };

    class YMF262Port0 : public AddressDataCommand
    {
    public:
        uint8_t get_marker() override
        {
            return 0x5e;
        }
    };

    class YMF262Port1 : public AddressDataCommand
    {
    public:
        uint8_t get_marker() override
        {
            return 0x5f;
        }
    };

    class IWait
    {
    public:
        virtual ~IWait() = default;
        [[nodiscard]] virtual uint16_t duration() const = 0;
    };

    class Wait : ICommand, IWait
    {
        uint16_t _duration = 0;
    public:
        [[nodiscard]] uint16_t duration() const override;
        void set_duration(uint16_t duration);

        void from_data(BinaryData& data) override;
        void to_data(BinaryData& data) const override;

        uint8_t get_marker() override
        {
            return 0x61;
        }
    };

    class NoDataCommand : ICommand
    {
    public:
        void from_data(BinaryData& ) override {}
        void to_data(BinaryData& ) const override {}
    };

    class Wait60thCommand : NoDataCommand, IWait
    {
    public:
        [[nodiscard]] uint16_t duration() const override
        {
            return 44100 / 60;
        }

        uint8_t get_marker() override
        {
            return 0x62;
        }
    };

    class Wait50thCommand : NoDataCommand, IWait
    {
    public:
        [[nodiscard]] uint16_t duration() const override
        {
            return 44100 / 50;
        }

        uint8_t get_marker() override
        {
            return 0x63;
        }
    };

    class End : NoDataCommand
    {
    public:
        uint8_t get_marker() override
        {
            return 0x66;
        }
    };
};
