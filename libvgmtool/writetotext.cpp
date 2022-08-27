#include <cstdio>
#include <cstdlib>
#include <zlib.h>
#include <cmath>
#include "writetotext.h"

#include <format>
#include <fstream>
#include <string>
#include <vector>

#include "IVGMToolCallback.h"
#include "vgm.h"
#include "utils.h"
#include "VgmFile.h"
#include <libpu8.h>

#define ON(x) ((x)?" on":"off")

// converts a frequency in Hz to a standard MIDI note representation
std::string note_name(const double frequencyHz)
{
    if (frequencyHz < 1)
    {
        return "notanote";
    }

    const double midiNote = (log(frequencyHz) - log(440)) / log(2) * 12 + 69;
    const int nearestNote = static_cast<int>(std::round(midiNote));
    const char* noteNames[] = {"A", "A#", "B", "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#"};
    return std::format(
        "{:>2}{:<2} {:>+3}",
        noteNames[abs(nearestNote - 21) % 12],
        (nearestNote - 24) / 12 + 1,
        static_cast<int>((midiNote - nearestNote) * 100 + ((nearestNote < midiNote)
            ? +0.5
            : -0.5))
    );
}

std::string make_noise_description(const std::string& prefix, uint32_t clock, int shift)
{
    return std::format(
        "{} ({}Hz)",
        prefix,
        clock / 32 / (16 << shift));
}

// Write VGM data from filename to filename.txt
// Incomplete handling of YM2612
// No handling of YM2151
// YM2413 needs checking
// TODO: display GD3 too - maybe use UTF-8?
void write_to_text(const std::string& filename, const IVGMToolCallback& callback, bool toStdOut, const std::string& outputFilename)
{
    int SampleCount = 0;
    int b0, b1, b2;
    const std::vector<std::string> ym2413Instruments{
        "User instrument",
        "Violin", "Guitar", "Piano", "Flute", "Clarinet",
        "Oboe", "Trumpet", "Organ", "Horn", "Synthesizer",
        "Harpsichord", "Vibraphone", "Synthesizer Bass",
        "Acoustic Bass", "Electric Guitar"
    };
    const std::vector<std::string> ym2413RhythmInstrumentNames{
        "High hat", "Cymbal", "Tom-tom", "Snare drum", "Bass drum"
    };
    int ym2413FNumbers[9] = {0};
    char ym2413Blocks[9] = {0};
    int rhythmMode = 1;
    long int filePos;
    int ym2612TimerA = 0;

    // Initial values
    // ---------------------------------- PSG ---------------------------------------
    // tone, vol for each channel
    // Tone freqs: 0, 2, 4, test using !(PSGLatchedRegister%2)&&(PSGLatchedRegister<5)
    unsigned short int psgRegisters[8] = {0, 0xf, 0, 0xf, 0, 0xf, 0, 0xf};
    int psgLatchedRegister = 0;

    if (!Utils::file_exists(filename))
    {
        return;
    }

    std::ostream* out;
    std::ofstream outputFile;

    if (toStdOut)
    {
        out = &std::cout;
    }
    else
    {
        auto outFilename = filename.empty()
            ? filename + ".txt"
            : outputFilename;
        outputFile.open(outFilename.c_str(), std::ios::trunc | std::ios::out);
        out = &outputFile;
        callback.show_status(std::format("Writing VGM data to text in \"{}\"...", outFilename));
    }

    VgmFile file;
    file.load_file(filename);

    // SN76489 noise descriptions are based on the clock rate
    std::vector<std::string> noiseTypes
    {
        make_noise_description("high", file.header().clock(VgmHeader::Chip::SN76489), 0),
        make_noise_description("med", file.header().clock(VgmHeader::Chip::SN76489), 1),
        make_noise_description("low", file.header().clock(VgmHeader::Chip::SN76489), 2),
        "ch 2"
    };

    *out << std::format("VGM Header:\n")
        << std::format("End-of-file offset   {:#08x} (absolute)\n", file.header().eof_offset())
        << std::format("VGM version          {}\n", file.header().version().string())
        << std::format("PSG speed            {} Hz\n", file.header().clock(VgmHeader::Chip::SN76489))
        << std::format("PSG noise feedback   {}\n", file.header().sn76489_feedback())
        << std::format("PSG shift register width {} bits\n", file.header().sn76489_shift_register_width())
        << std::format("YM2413 speed         {} Hz\n", file.header().clock(VgmHeader::Chip::YM2413))
        << std::format("YM2612 speed         {} Hz\n", file.header().clock(VgmHeader::Chip::YM2612))
        << std::format("YM2151 speed         {} Hz\n", file.header().clock(VgmHeader::Chip::YM2151))
        << std::format("GD3 tag offset       0x{:08x} (absolute)\n", file.header().gd3_offset())
        << std::format("Total length         {} samples ({:.2f}s)\n", file.header().sample_count(),
            file.header().sample_count() / 44100.0)
        << std::format("Loop point offset    0x{:08x} (absolute)\n", file.header().loop_offset(), file.header().loop_sample_count())
        << std::format("Loop length          {} samples ({:.2f}s)\n", file.header().loop_sample_count(),
            file.header().loop_sample_count() / 44100.0)
        << std::format("Recording rate       {} Hz\n", file.header().frame_rate())
        << "\nVGM data:\n";

    gzFile in = gzopen(filename.c_str(), "rb");
    gzseek(in, 0x40, SEEK_SET);

    do
    {
        filePos = gztell(in);
        if (filePos == static_cast<long>(file.header().loop_offset()))
        {
            *out << "------- Loop point -------\n";
        }
        b0 = gzgetc(in);
        *out << std::format("0x{:08x}: {:02x} ", filePos, b0);
        constexpr int SAMPLES_PER_MINUTE = 60 * 44100;
        switch (b0)
        {
        case VGM_GGST: // GG stereo (1 byte data)
            {
                auto mask = gzgetc(in);
                *out << std::format("{:02x}    GG st:  ", mask);
                static const std::string bits("012N012N");
                for (int i = 0; i < 8; ++i)
                {
                    if ((mask >> i & 1) == 0)
                    {
                        *out << '-';
                    }
                    else
                    {
                        *out << bits[i];
                    }
                }
                *out << '\n';
            }
            break;
        case VGM_PSG: // PSG write (1 byte data)
            b1 = gzgetc(in);
            *out << std::format("{:02x}    PSG:    ", b1);
            if ((b1 & 0x80) != 0)
            {
                // Latch/data byte   %1 cc t dddd
                *out << "Latch/data: ";
                psgLatchedRegister = b1 >> 4 & 0x07;
                psgRegisters[psgLatchedRegister] =
                    (psgRegisters[psgLatchedRegister] & 0x3f0) // zero low 4 bits
                    | (b1 & 0xf); // and replace with data
            }
            else
            {
                // Data byte
                *out << "Data:       ";
                if (((psgLatchedRegister % 2) == 0) && psgLatchedRegister < 5)
                {
                    // Tone register
                    psgRegisters[psgLatchedRegister] =
                        (psgRegisters[psgLatchedRegister] & 0x00f) // zero high 6 bits
                        | ((b1 & 0x3f) << 4); // and replace with data
                }
                else
                {
                    // Other register
                    psgRegisters[psgLatchedRegister] = b1 & 0x0f; // Replace with data
                }
            }
        // Analyse:
            switch (psgLatchedRegister)
            {
            case 0:
            case 2:
            case 4: // Tone registers
                {
                    double frequencyHz = psgRegisters[psgLatchedRegister] == 0u
                        ? 0.0
                        : static_cast<double>(file.header().clock(VgmHeader::Chip::SN76489)) / 32 / psgRegisters
                        [psgLatchedRegister];
                    *out << std::format(
                        "Tone ch {} -> 0x{:03x} = {:8.2f} Hz = {}\n",
                        psgLatchedRegister / 2, // Channel
                        psgRegisters[psgLatchedRegister], // Value
                        frequencyHz, // Frequency
                        note_name(frequencyHz).c_str() // Note
                    );
                }
                break;
            case 6: // Noise
                *out << std::format(
                    "Noise: {}, {}\n",
                    (b1 & 0x4) == 0x4 ? "white" : "synchronous",
                    noiseTypes[b1 & 0x3]);
                break;
            default: // Volume
                *out << std::format(
                    "Volume: ch {} -> 0x{:x} = {}%\n",
                    psgLatchedRegister / 2,
                    psgRegisters[psgLatchedRegister],
                    (15 - psgRegisters[psgLatchedRegister]) * 100 / 15 // TODO this is wrong!
                );
                break;
            } // end switch
            break;
        case VGM_YM2413: // YM2413
            b1 = gzgetc(in);
            b2 = gzgetc(in);
            *out << std::format("{:02x} {:02x} YM2413: ", b1, b2);
            switch (b1 >> 4)
            {
            // go by 1st digit first
            case 0x0: // User tone / percussion
                switch (b1)
                {
                case 0x00:
                case 0x01:
                    *out << std::format(
                        "Tone user inst ({}): MSWH 0x{:x}, key scale rate {}, sustain {}, vibrato {}, AM {}\n",
                        b1 == 1 ? "car" : "mod",
                        b2 & 0xf,
                        b2 & 1 << 4,
                        ON(b2& 1<<5),
                        ON(b2& 1<<6),
                        ON(b2& 1<<7));
                    break;
                case 0x02:
                    *out << std::format(
                        "Tone user inst (mod): key scale level {}, total level 0x{:x}\n",
                        b2 >> 6,
                        b2 & 0x3f);
                    break;
                case 0x03:
                    *out << std::format(
                        "Tone user inst (car): key scale level {}, rectification distortion to car {}, mod {}, FM feedback {}\n",
                        b2 >> 6,
                        ON(b2& 1<<3),
                        ON(b2& 1<<4),
                        b2 & 0x7);
                    break;
                case 0x04:
                case 0x05:
                    *out << std::format(
                        "Tone user inst ({}): attack rate 0x{:x}, decay rate 0x{:x}\n",
                        (b1 - 4) != 0
                        ? "car"
                        : "mod",
                        b2 & 0xf,
                        b2 >> 4);
                    break;
                case 0x06:
                case 0x07:
                    *out << std::format(
                        "Tone user inst ({}): sustain level 0x{:x}, release rate 0x{:x}\n",
                        (b1 - 6) != 0
                        ? "car"
                        : "mod", b2 & 0xf, b2 >> 4);
                    break;
                case 0x0E: // Percussion
                    {
                        rhythmMode = b2 & 0x20;
                        *out << "Percussion (" << ON(rhythmMode) << ")";
                        for (int bitIndex = 0; bitIndex < 5; ++bitIndex)
                        {
                            if ((b2 >> bitIndex & 1) != 0)
                            {
                                *out << ", " << ym2413RhythmInstrumentNames[bitIndex];
                            }
                        }
                        *out << "\n";
                    }
                    break;
                default:
                    *out << std::format("Invalid register 0x{:x}\n", b1);
                    break;
                }
                break;
            case 0x1: // Tone F-number low 8 bits
                if (b1 > 0x18)
                {
                    *out << std::format("Invalid register 0x{:x}\n", b1);
                }
                else
                {
                    int channel = b1 & 0xf;
                    ym2413FNumbers[channel] = (ym2413FNumbers[channel] & 0x100) | b2; // Update low bits of F-number
                    double frequencyHz = static_cast<double>(ym2413FNumbers[channel]) * file.header().clock(VgmHeader::Chip::YM2413)
                        / 72 / (1
                            << (19 - ym2413Blocks[channel]));
                    *out << std::format("Tone F-num low bits: ch {} -> {:03d}({}) = {:8.2f} Hz = {}",
                        channel,
                        ym2413FNumbers[channel],
                        ym2413Blocks[channel],
                        frequencyHz,
                        note_name(frequencyHz).c_str());
                    if (b1 >= 0x16)
                    {
                        *out << " OR Percussion F-num\n";
                    }
                    else
                    {
                        *out << "\n";
                    }
                }
                break;
            case 0x2: // Tone more stuff including key
                if (b1 > 0x28)
                {
                    *out << std::format("Invalid register 0x{:x}\n", b1);
                }
                else
                {
                    if (b2 & 0x10)
                    {
                        // key ON
                        int chan = b1 & 0xf;
                        double freq;
                        ym2413FNumbers[chan] = ym2413FNumbers[chan] & 0xff | (b2 & 1) << 8;
                        ym2413Blocks[chan] = (b2 & 0xE) >> 1;
                        freq = static_cast<double>(ym2413FNumbers[chan]) * file.header().clock(VgmHeader::Chip::YM2413) / 72 / (1 <<
                            (19 -
                                ym2413Blocks[chan]));
                        *out << std::format(
                            "Tone F-n/bl/sus/key: ch {} -> {:03d}({}) = {:8.2f} Hz = {}; sustain {}, key {}\n",
                            chan,
                            ym2413FNumbers[chan],
                            ym2413Blocks[chan],
                            freq,
                            note_name(freq).c_str(),
                            ON(b2&0x20),
                            ON(b2&0x10));
                    }
                    else
                    {
                        *out << std::format("Tone F-n/bl/sus/key (ch {}, key off)", b1 & 0xf);
                        if (b1 >= 0x26)
                        {
                            *out << " OR Percussion F-num/bl\n";
                        }
                        else
                        {
                            *out << "\n";
                        }
                    }
                }
                break;
            case 0x3: // Tone instruments and volume/percussion volume
                if (b1 >= YM2413NumRegs)
                {
                    *out << std::format("Invalid register 0x{:02x}\n", b1);
                }
                else
                {
                    *out << std::format("Tone vol/instrument: ch {} -> vol 0x{:x} = {:3}%; inst 0x{:x} = {:>17}",
                        b1 & 0xf,
                        b2 & 0xf,
                        static_cast<int>((15 - (b2 & 0xf)) / 15.0 * 100),
                        b2 >> 4,
                        ym2413Instruments[b2 >> 4].c_str());
                    if (b1 >= 0x36)
                    {
                        int i1 = 0, i2 = -1;
                        switch (b1)
                        {
                        case 0x36:
                            i1 = 4;
                            break;
                        case 0x37:
                            i1 = 3;
                            i2 = 0;
                            break;
                        case 0x38:
                            i1 = 1;
                            i2 = 2;
                            break;
                        }
                        *out << std::format(" OR Percussion volume:   {} -> vol 0x{:x} = {:3}%",
                            ym2413RhythmInstrumentNames[i1].c_str(), b2 & 0xf,
                            static_cast<int>((15 - (b2 & 0xf)) / 15.0 * 100));
                        if (i2 > -1)
                        {
                            *out << std::format("; {} -> vol 0x{:x} = {:3}%", ym2413RhythmInstrumentNames[i2].c_str(), b2 >> 4,
                                static_cast<int>((15 - (b2 >> 4)) / 15.0 * 100));
                        }
                    }
                    *out << "\n";
                }
                break;
            default:
                *out << std::format("Invalid register 0x{:02x}\n", b1);
                break;
            }
            break;
        case VGM_YM2612_0: // YM2612 port 0
            b1 = gzgetc(in); // Port
            b2 = gzgetc(in); // Data
            *out << std::format("{:02x} {:02x} YM2612: ", b1, b2);
            switch (b1 >> 4)
            {
            // Go by first digit first
            case 0x2:
                switch (b1)
                {
                case 0x22: // LFO
                    if (b2 & 0x8)
                    {
                        // ON
                        static const std::vector<std::string> lfoFreqs{
                            "3.98", "5.56", "6.02", "6.37", "6.88", "9.63", "48.1", "72.2"
                        };
                        *out << std::format("Low Frequency Oscillator: {} Hz\n", lfoFreqs[b2 & 7]);
                    }
                    else
                    {
                        // OFF
                        *out << "Low Frequency Oscillator: disabled\n";
                    }
                    break;
                case 0x24: // Timer A MSB
                    ym2612TimerA = ym2612TimerA & 0x3 | b2 << 2;
                    *out << std::format("Timer A MSBs: length {} = {} μs\n", ym2612TimerA, 18 * (1024 - ym2612TimerA));
                    break;
                case 0x25: // Timer A LSB
                    ym2612TimerA = ym2612TimerA & 0x3fc | b2 & 0x3;
                    *out << std::format("Timer A LSBs: length {} = {} μs\n", ym2612TimerA, 18 * (1024 - ym2612TimerA));
                    break;
                case 0x26: // Timer B
                    *out << std::format("Timer B: length {} = {} μs\n", b2, 288 * (256 - b2));
                    break;
                case 0x27: // Timer control/ch 3 mode
                    *out << "Timer control/ch 3 mode: ";
                    *out << std::format("timer A {}, set flag on overflow {}{}, ", ON(b2&0x1), ON(b2&4),
                        b2 & 0x10
                        ? ", reset flag"
                        : "");
                    *out << std::format("timer B {}, set flag on overflow {}{}, ", ON(b2&0x2), ON(b2&8),
                        b2 & 0x20
                        ? ", reset flag"
                        : "");
                    *out << std::format("ch 3 {}\n",
                        (b2 & 0xc0) == 0
                        ? "normal mode"
                        : (b2 & 0xc0) == 0x80
                        ? "special mode"
                        : "invalid mode bits");
                    break;
                case 0x28: // Operator enabling
                    *out << std::format("Operator control: channel {} -> ", b2 & 0x7);
                    for (int i = 3; i >= 0; --i)
                    {
                        if (((b2 >> (i + 4)) & 1) == 0)
                        {
                            *out << '-';
                        }
                        else
                        {
                            *out << '1' + i; // TODO is this right?
                        }
                    }
                    break;
                case 0x2a: // DAC
                    *out << std::format("DAC -> {}\n", b2);
                    break;
                case 0x2b: // DAC enable
                    *out << std::format("DAC {}\n", ON(b2&0x80));
                    break;
                default:
                    *out << std::format("invalid data (port 0 reg 0x{:02x} data 0x{:02x})\n", b1, b2);
                    break;
                }
                break;
            case 0x3: // Detune/Multiple
                {
                    int ch = (b2 & 0xf) % 4;
                    int op = (b2 & 0xf) / 4;
                    *out << "Detune/multiple: ";
                    if (ch != 4)
                    {
                        // Valid
                        *out << std::format("ch {} op {} frequency*", ch, op);
                        // Multiple:
                        if ((b2 & 0xf) == 0)
                        {
                            *out << "0.5";
                        }
                        else
                        {
                            *out << (b2 & 0xf);
                        }
                        // Detune:
                        if ((b2 >> 4 & 0x3) != 0)
                        {
                            // detune !=0
                            *out << std::format(
                                "*(1{}{}epsilon)",
                                b2 & 0x40 ? '+' : '-',
                                b2 >> 4 & 0x3);
                        }
                        *out << "\n";
                    }
                    else
                    {
                        // Invalid
                        *out << "Invalid data\n";
                    }
                }
                break;
            case 0x4: // Total level
                {
                    int ch = (b2 & 0xf) % 4;
                    int op = (b2 & 0xf) / 4;
                    *out << "Total level: ";
                    if (ch != 4)
                    {
                        // Valid
                        *out << std::format(
                            "ch {} op {} -> 0x{:02x} = {:3}%%\n",
                            ch,
                            op,
                            b2 & 0x7f,
                            static_cast<int>((127 - (b2 & 0x7f)) / 127.0 * 100));
                    }
                    else
                    {
                        // Invalid
                        *out << "Invalid data\n";
                    }
                }
                break;
            case 0x5: // Rate scaling/Attack rate
                {
                    int ch = (b2 & 0xf) % 4;
                    int op = (b2 & 0xf) / 4;
                    *out << "Rate scaling/attack rate: ";
                    if (ch != 4)
                    {
                        // Valid
                        *out << std::format(
                            "ch {} op {} RS 1/{}, AR {}\n",
                            ch,
                            op,
                            1 << (3 - (b2 >> 6)),
                            b2 & 0x1f);
                    }
                    else
                    {
                        // Invalid
                        *out << "Invalid data\n";
                    }
                }
                break;
            case 0x6: // Amplitude modulation/1st decay rate
                {
                    int ch = (b2 & 0xf) % 4;
                    int op = (b2 & 0xf) / 4;
                    *out << "Amplitude modulation/1st decay rate: ";
                    if (ch != 4)
                    {
                        // Valid
                        *out << std::format(
                            "ch {} op {} AM {}, D1R {}\n",
                            ch,
                            op,
                            ON(b2>>7),
                            b2 & 0x1f);
                    }
                    else
                    {
                        // Invalid
                        *out << "Invalid data\n";
                    }
                }
                break;
            case 0x7: // 2nd decay rate
                {
                    int ch = (b2 & 0xf) % 4;
                    int op = (b2 & 0xf) / 4;
                    *out << "2nd decay rate: ";
                    if (ch != 4)
                    {
                        // Valid
                        *out << std::format("ch {} op {} D2R {}\n", ch, op, b2 & 0x1f);
                    }
                    else
                    {
                        // Invalid
                        *out << "Invalid data\n";
                    }
                }
                break;
            case 0x8: // 1st decay end level
                {
                    int ch = (b2 & 0xf) % 4;
                    int op = (b2 & 0xf) / 4;
                    *out << "1st decay end level/release rate: ";
                    if (ch != 4)
                    {
                        // Valid
                        *out << std::format("ch {} op {} D1L {}, RR {}\n", ch, op, (b2 >> 4) * 8, (b2 & 0xf) << 1 & 1);
                        // TODO this is illogical
                    }
                    else
                    {
                        // Invalid
                        *out << "Invalid data\n";
                    }
                }
                break;
            default:
                *out << std::format("port 0 reg 0x{:02x} data 0x{:02x}\n", b1, b2);
                break;
            }
            break;
        case VGM_YM2612_1: // YM2612 port 1
            b1 = gzgetc(in);
            b2 = gzgetc(in);
            *out << std::format("{:02x} {:02x} YM2612: ", b1, b2);
            *out << std::format("port 1 reg 0x{:02x} data 0x{:02x}\n", b1, b2);
            break;
        case VGM_YM2151: // YM2151
            b1 = gzgetc(in);
            b2 = gzgetc(in);
            *out << std::format("{:02x} {:02x} YM2151 reg 0x{:02x} data 0x{:02x}\n", b1, b2, b1, b2);
            break;
        case 0x55: // Reserved up to 0x5f
        case 0x56: // All have 2 bytes of data
        case 0x57:
        case 0x58:
        case 0x59:
        case 0x5a:
        case 0x5b:
        case 0x5c:
        case 0x5d:
        case 0x5e:
        case 0x5f:
            b1 = gzgetc(in);
            b2 = gzgetc(in);
            *out << std::format("{:02x} {:02x} YM????\n", b1, b2);
            break;
        case VGM_PAUSE_WORD: // Wait n samples
            b1 = gzgetc(in);
            b2 = gzgetc(in);
            SampleCount += b1 | b2 << 8;
            *out << std::format(
                "{:02x} {:02x} Wait:   {:5} samples ({:7.2f} ms) (total {:8} samples ({}:{:05.2f}))\n",
                b1,
                b2,
                b1 | b2 << 8,
                (b1 | b2 << 8) / 44.1,
                SampleCount,
                SampleCount / SAMPLES_PER_MINUTE,
                SampleCount % SAMPLES_PER_MINUTE / 44100.0);
            break;
        case VGM_PAUSE_60TH: // Wait 1/60 s
            SampleCount += LEN60TH;
            *out << std::format(
                "      Wait:     735 samples (1/60s)      (total {:8} samples ({}:{:05.2f}))\n", 
                SampleCount,
                SampleCount / SAMPLES_PER_MINUTE,
                SampleCount % SAMPLES_PER_MINUTE / 44100.0);
            break;
        case VGM_PAUSE_50TH: // Wait 1/50 s
            SampleCount += LEN50TH;
            *out << std::format("      Wait:   882 samples (1/50s)      (total {:8} samples ({}:{:05.2f}))\n", SampleCount,
                SampleCount / SAMPLES_PER_MINUTE, SampleCount % SAMPLES_PER_MINUTE / 44100.0);
            break;
        case 0x70:
        case 0x71:
        case 0x72:
        case 0x73:
        case 0x74:
        case 0x75:
        case 0x76:
        case 0x77:
        case 0x78:
        case 0x79:
        case 0x7a:
        case 0x7b:
        case 0x7c:
        case 0x7d:
        case 0x7e:
        case 0x7f: // Wait 1-16 samples
            b1 = (b0 & 0xf) + 1;
            SampleCount += b1;
            *out << std::format("      Wait:   {:5} samples ({:7.2f} ms) (total {:8} samples ({}:{:05.2f}))\n", b1, b1 / 44.1,
                SampleCount, SampleCount / SAMPLES_PER_MINUTE, SampleCount % SAMPLES_PER_MINUTE / 44100.0);
            break;
        case VGM_END: // End of sound data... report
            *out << "End of music data\n";
            break;
        default:
            *out << "Unknown/invalid data\n";
            break;
        }
    }
    while
#ifdef STOPWRITEAT4KB
    ((gztell(in)<0x1000) && (b0!=EOF));
#else
    (b0 != EOF);
#endif;

    gzclose(in);

    if (!file.gd3().empty())
    {
        *out << "\n\nGD3 tag:\n";
        *out << std::format("Title (EN):\t{}\n", u8narrow(file.gd3().get_text(Gd3Tag::Key::TitleEn)));
    }

    if (!toStdOut)
    {
        outputFile.close();
    }

    callback.show_status("Write to text complete");
}
