#include <cstdio>
#include <cstdlib>
#include <zlib.h>
#include <cmath>
#include "writetotext.h"

#include <string>
#include <vector>

#include "IVGMToolCallback.h"
#include "vgm.h"
#include "utils.h"

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
    return Utils::format(
        "%2s%-2d %+3d", 
        noteNames[abs(nearestNote - 21) % 12], 
        (nearestNote - 24) / 12 + 1, 
        static_cast<int>((midiNote - nearestNote) * 100 + ((nearestNote < midiNote) ? +0.5 : -0.5))
    );
}

// Write VGM data from filename to filename.txt
// Incomplete handling of YM2612
// No handling of YM2151
// YM2413 needs checking
// TODO: display GD3 too - maybe use UTF-8?
void write_to_text(const std::string& filename, const IVGMToolCallback& callback)
{
    int SampleCount = 0;
    int b0, b1, b2;
    char tempstr[32] = "xxxx";
    int i;
    // We write into these
    std::vector<std::string> noiseTypes{{"high (", "med (", "low (", "ch 2"}};
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
    unsigned short int PSGRegisters[8] = {0, 0xf, 0, 0xf, 0, 0xf, 0, 0xf};
    int PSGLatchedRegister = 0;

    if (!file_exists(filename, callback))
    {
        return;
    }

    auto outFilename = filename + ".txt";

    callback.show_status(Utils::format("Writing VGM data to text in \"%s\"...", outFilename.c_str()));

    FILE* out = fopen(outFilename.c_str(), "w");

    gzFile in = gzopen(filename.c_str(), "rb");

    // Read header
    VGMHeader vgmHeader;
    if (!ReadVGMHeader(in, &vgmHeader, callback))
    {
        gzclose(in);
        return;
    }

    gzseek(in, 0x40, SEEK_SET);

    fprintf(out, 
        "VGM Header:\n"
        "VGM marker           \"%c%c%c%c\"\n"
        "End-of-file offset   0x%08x (relative) 0x%08x (absolute)\n"
        "VGM version          0x%08x (%x.%02x)\n"
        "PSG speed            %d Hz\n"
        "PSG noise feedback   %d\n"
        "PSG shift register width %d bits\n"
        "YM2413 speed         %d Hz\n"
        "YM2612 speed         %d Hz\n"
        "YM2151 speed         %d Hz\n"
        "GD3 tag offset       0x%08x (relative) 0x%08x (absolute)\n"
        "Total length         %d samples (%.2fs)\n"
        "Loop point offset    0x%08x (relative) 0x%08x (absolute)\n"
        "Loop length          %d samples (%.2fs)\n"
        "Recording rate       %d Hz\n\n"
        "VGM data:\n"
        , 
        vgmHeader.VGMIdent[0], 
        vgmHeader.VGMIdent[1], 
        vgmHeader.VGMIdent[2], 
        vgmHeader.VGMIdent[3], 
        vgmHeader.EoFOffset, 
        vgmHeader.EoFOffset + EOFDELTA, 
        vgmHeader.Version, 
        vgmHeader.Version >> 8, 
        vgmHeader.Version & 0xff, 
        vgmHeader.PSGClock, 
        vgmHeader.PSGWhiteNoiseFeedback, 
        vgmHeader.PSGShiftRegisterWidth, 
        vgmHeader.YM2413Clock, 
        vgmHeader.YM2612Clock, 
        vgmHeader.YM2151Clock, 
        vgmHeader.GD3Offset, 
        vgmHeader.GD3Offset + GD3DELTA, 
        vgmHeader.TotalLength, 
        vgmHeader.TotalLength / 44100.0, 
        vgmHeader.LoopOffset, 
        vgmHeader.LoopOffset + LOOPDELTA, 
        vgmHeader.LoopLength, 
        vgmHeader.LoopLength / 44100.0, 
        vgmHeader.RecordingRate
    );

    for (i = 0; i < 3; ++i)
    {
        noiseTypes[i] += _itoa(vgmHeader.PSGClock / 32 / (16 << i), tempstr, 10);
        noiseTypes[i] += "Hz)";
    }

    do
    {
        filePos = gztell(in);
        if (filePos == static_cast<long>(vgmHeader.LoopOffset) + LOOPDELTA)
        {
            fprintf(out, "------- Loop point -------\n");
        }
        b0 = gzgetc(in);
        fprintf(out, "0x%08lx: %02x ", filePos, b0);
        switch (b0)
        {
        case VGM_GGST: // GG stereo (1 byte data)
            b1 = gzgetc(in);
            strcpy(tempstr, "012N012N");
            for (i = 0; i < 8; ++i)
            {
                if (!(b1 >> i & 1))
                {
                    tempstr[7 - i] = '-';
                }
            }
            fprintf(out, "%02x    GG st:  %s\n", b1, tempstr);
            break;
        case VGM_PSG: // PSG write (1 byte data)
            b1 = gzgetc(in);
            fprintf(out, "%02x    PSG:    ", b1);
            if (b1 & 0x80)
            {
                // Latch/data byte   %1 cc t dddd
                fprintf(out, "Latch/data: ");
                PSGLatchedRegister = b1 >> 4 & 0x07;
                PSGRegisters[PSGLatchedRegister] =
                    (PSGRegisters[PSGLatchedRegister] & 0x3f0) // zero low 4 bits
                    | (b1 & 0xf); // and replace with data
            }
            else
            {
                // Data byte
                fprintf(out, "Data:       ");
                if (!(PSGLatchedRegister % 2) && PSGLatchedRegister < 5)
                {
                    // Tone register
                    PSGRegisters[PSGLatchedRegister] =
                        (PSGRegisters[PSGLatchedRegister] & 0x00f) // zero high 6 bits
                        | ((b1 & 0x3f) << 4); // and replace with data
                }
                else
                {
                    // Other register
                    PSGRegisters[PSGLatchedRegister] = b1 & 0x0f; // Replace with data
                }
            }
        // Analyse:
            switch (PSGLatchedRegister)
            {
            case 0:
            case 2:
            case 4: // Tone registers
                {
                    double frequencyHz;
                    if (PSGRegisters[PSGLatchedRegister])
                    {
                        frequencyHz = static_cast<double>(vgmHeader.PSGClock) / 32 / PSGRegisters[PSGLatchedRegister];
                    }
                    else
                    {
                        frequencyHz = 0.0;
                    }
                    fprintf(out, "Tone ch %d -> 0x%03x = %8.2f Hz = %s\n", 
                        PSGLatchedRegister / 2, // Channel
                        PSGRegisters[PSGLatchedRegister], // Value
                        frequencyHz, // Frequency
                        note_name(frequencyHz).c_str() // Note
                    );
                }
                break;
            case 6: // Noise
                fprintf(out, "Noise: %s, %s\n", 
                    (b1 & 0x4) == 0x4 ? "white" : "synchronous", 
                    noiseTypes[b1 & 0x3].c_str());
                break;
            default: // Volume
                fprintf(out, "Volume: ch %d -> 0x%x = %d%%\n", 
                    PSGLatchedRegister / 2, 
                    PSGRegisters[PSGLatchedRegister], 
                    (15 - PSGRegisters[PSGLatchedRegister]) * 100 / 15
                );
                break;
            } // end switch
            break;
        case VGM_YM2413: // YM2413
            b1 = gzgetc(in);
            b2 = gzgetc(in);
            fprintf(out, "%02x %02x YM2413: ", b1, b2);
            switch (b1 >> 4)
            {
            // go by 1st digit first
            case 0x0: // User tone / percussion
                switch (b1)
                {
                case 0x00:
                case 0x01:
                    fprintf(out, "Tone user inst (%s): MSWH 0x%1x, key scale rate %d, sustain %s, vibrato %s, AM %s\n", 
                        b1 ? "car" : "mod", b2 & 0xf, b2 & 1 << 4, ON(b2& 1<<5), ON(b2& 1<<6), ON(b2& 1<<7));
                    break;
                case 0x02:
                    fprintf(out, "Tone user inst (mod): key scale level %1d, total level 0x%x\n", b2 >> 6, b2 & 0x3f);
                    break;
                case 0x03:
                    fprintf(
                        out, 
                        "Tone user inst (car): key scale level %1d, rectification distortion to car %s, mod %s, FM feedback %1d\n", 
                        b2 >> 6, ON(b2& 1<<3), ON(b2& 1<<4), b2 & 0x7);
                    break;
                case 0x04:
                case 0x05:
                    fprintf(out, "Tone user inst (%s): attack rate 0x%1x, decay rate 0x%1x\n", b1 - 4 ? "car" : "mod", 
                        b2 & 0xf, b2 >> 4);
                    break;
                case 0x06:
                case 0x07:
                    fprintf(out, "Tone user inst (%s): sustain level 0x%1x, release rate 0x%1x\n", 
                        b1 - 6 ? "car" : "mod", b2 & 0xf, b2 >> 4);
                    break;
                case 0x0E: // Percussion
                    {
                        rhythmMode = b2 & 0x20;
                        std::string s = "Percussion (";
                        s += ON(rhythmMode);
                        s += ")";
                        for (int bitIndex = 0; bitIndex < 5; ++bitIndex)
                        {
                            if (b2 >> bitIndex & 1)
                            {
                                s += ", ";
                                s += ym2413RhythmInstrumentNames[bitIndex];
                            }
                        }
                        s += "\n";
                        fputs(s.c_str(), out);
                    }
                    break;
                default:
                    fprintf(out, "Invalid register 0x%x\n", b1);
                    break;
                }
                break;
            case 0x1: // Tone F-number low 8 bits
                if (b1 > 0x18)
                {
                    fprintf(out, "Invalid register 0x%x\n", b1);
                }
                else
                {
                    int channel = b1 & 0xf;
                    ym2413FNumbers[channel] = (ym2413FNumbers[channel] & 0x100) | b2; // Update low bits of F-number
                    double frequencyHz = static_cast<double>(ym2413FNumbers[channel]) * vgmHeader.YM2413Clock / 72 / (1 << (19 - ym2413Blocks[channel]));
                    fprintf(out, "Tone F-num low bits: ch %1d -> %03d(%1d) = %8.2f Hz = %s", 
                        channel, 
                        ym2413FNumbers[channel], 
                        ym2413Blocks[channel], 
                        frequencyHz, 
                        note_name(frequencyHz).c_str());
                    if (b1 >= 0x16)
                    {
                        fprintf(out, " OR Percussion F-num\n");
                    }
                    else
                    {
                        fprintf(out, "\n");
                    }
                }
                break;
            case 0x2: // Tone more stuff including key
                if (b1 > 0x28)
                {
                    fprintf(out, "Invalid register 0x%x\n", b1);
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
                        freq = static_cast<double>(ym2413FNumbers[chan]) * vgmHeader.YM2413Clock / 72 / (1 << (19 - ym2413Blocks[chan]));
                        fprintf(out, "Tone F-n/bl/sus/key: ch %1d -> %03d(%1d) = %8.2f Hz = %s; sustain %s, key %s\n", 
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
                        fprintf(out, "Tone F-n/bl/sus/key (ch %1d, key off)", b1 & 0xf);
                        if (b1 >= 0x26)
                        {
                            fprintf(out, " OR Percussion F-num/bl\n");
                        }
                        else
                        {
                            fprintf(out, "\n");
                        }
                    }
                }
                break;
            case 0x3: // Tone instruments and volume/percussion volume
                if (b1 >= YM2413NumRegs)
                {
                    fprintf(out, "Invalid register 0x%x\n", b1);
                }
                else
                {
                    fprintf(out, "Tone vol/instrument: ch %1d -> vol 0x%1x = %3d%%; inst 0x%1x = %-17s", 
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
                        fprintf(out, " OR Percussion volume:   %s -> vol 0x%1x = %3d%%", 
                            ym2413RhythmInstrumentNames[i1].c_str(), b2 & 0xf, 
                            static_cast<int>((15 - (b2 & 0xf)) / 15.0 * 100));
                        if (i2 > -1)
                        {
                            fprintf(out, "; %s -> vol 0x%1x = %3d%%", ym2413RhythmInstrumentNames[i2].c_str(), b2 >> 4, 
                                static_cast<int>((15 - (b2 >> 4)) / 15.0 * 100));
                        }
                    }
                    fprintf(out, "\n");
                }
                break;
            default:
                fprintf(out, "Invalid register 0x%x\n", b1);
                break;
            }
            break;
        case VGM_YM2612_0: // YM2612 port 0
            b1 = gzgetc(in); // Port
            b2 = gzgetc(in); // Data
            fprintf(out, "%02x %02x YM2612: ", b1, b2);
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
                        const char* LFOFreqs[] = {"3.98", "5.56", "6.02", "6.37", "6.88", "9.63", "48.1", "72.2"};
                        fprintf(out, "Low Frequency Oscillator: %s Hz\n", LFOFreqs[b2 & 7]);
                    }
                    else
                    {
                        // OFF
                        fprintf(out, "Low Frequency Oscillator: disabled\n");
                    }
                    break;
                case 0x24: // Timer A MSB
                    ym2612TimerA = ym2612TimerA & 0x3 | b2 << 2;
                    fprintf(out, "Timer A MSBs: length %d = %d �s\n", ym2612TimerA, 18 * (1024 - ym2612TimerA));
                    break;
                case 0x25: // Timer A LSB
                    ym2612TimerA = ym2612TimerA & 0x3fc | b2 & 0x3;
                    fprintf(out, "Timer A LSBs: length %d = %d �s\n", ym2612TimerA, 18 * (1024 - ym2612TimerA));
                    break;
                case 0x26: // Timer B
                    fprintf(out, "Timer B: length %d = %d �s\n", b2, 288 * (256 - b2));
                    break;
                case 0x27: // Timer control/ch 3 mode
                    fprintf(out, "Timer control/ch 3 mode: ");
                    fprintf(out, "timer A %s, set flag on overflow %s%s, ", ON(b2&0x1), ON(b2&4), 
                        b2 & 0x10 ? ", reset flag" : "");
                    fprintf(out, "timer B %s, set flag on overflow %s%s, ", ON(b2&0x2), ON(b2&8), 
                        b2 & 0x20 ? ", reset flag" : "");
                    fprintf(out, "ch 3 %s\n", 
                        (b2 & 0xc0) == 0
                            ? "normal mode"
                            : (b2 & 0xc0) == 1
                            ? "special mode"
                            : "invalid mode bits");
                    break;
                case 0x28: // Operator enabling
                    strcpy(tempstr, "1234");
                    for (i = 0; i < 4; ++i)
                    {
                        if (!((b2 >> (i + 4)) & 1))
                        {
                            tempstr[3 - i] = '-';
                        }
                    }
                    fprintf(out, "Operator control: channel %d -> %s\n", b2 & 0x7, tempstr);
                    break;
                case 0x2a: // DAC
                    fprintf(out, "DAC -> %d\n", b2);
                    break;
                case 0x2b: // DAC enable
                    fprintf(out, "DAC %s\n", ON(b2&0x80));
                    break;
                default:
                    fprintf(out, "invalid data (port 0 reg 0x%02x data 0x%02x)\n", b1, b2);
                    break;
                }
                break;
            case 0x3: // Detune/Multiple
                {
                    int ch = (b2 & 0xf) % 4;
                    int op = (b2 & 0xf) / 4;
                    fprintf(out, "Detune/multiple: ");
                    if (ch != 4)
                    {
                        // Valid
                        fprintf(out, "ch %d op %d ", ch, op);
                        // Multiple:
                        if ((b2 & 0xf) == 0)
                        {
                            strcpy(tempstr, "0.5");
                        }
                        else
                        {
                            sprintf(tempstr, "%d", b2 & 0xf);
                        }
                        fprintf(out, "frequency*%s", tempstr);
                        // Detune:
                        if (b2 >> 4 & 0x3)
                        {
                            // detune !=0
                            fprintf(out, "*(1%c%depsilon)", b2 & 0x40 ? '+' : '-', b2 >> 4 & 0x3);
                        }
                        fprintf(out, "\n");
                    }
                    else
                    {
                        // Invalid
                        fprintf(out, "Invalid data\n");
                    }
                }
                break;
            case 0x4: // Total level
                {
                    int ch = (b2 & 0xf) % 4;
                    int op = (b2 & 0xf) / 4;
                    fprintf(out, "Total level: ");
                    if (ch != 4)
                    {
                        // Valid
                        fprintf(out, "ch %d op %d ", ch, op);
                        fprintf(out, "-> 0x%02x = %3d%%\n", b2 & 0x7f, 
                            static_cast<int>((127 - (b2 & 0x7f)) / 127.0 * 100));
                    }
                    else
                    {
                        // Invalid
                        fprintf(out, "Invalid data\n");
                    }
                }
                break;
            case 0x5: // Rate scaling/Attack rate
                {
                    int ch = (b2 & 0xf) % 4;
                    int op = (b2 & 0xf) / 4;
                    fprintf(out, "Rate scaling/attack rate: ");
                    if (ch != 4)
                    {
                        // Valid
                        fprintf(out, "ch %d op %d ", ch, op);
                        fprintf(out, "RS 1/%d, AR %d\n", 1 << (3 - (b2 >> 6)), b2 & 0x1f);
                    }
                    else
                    {
                        // Invalid
                        fprintf(out, "Invalid data\n");
                    }
                }
                break;
            case 0x6: // Amplitude modulation/1st decay rate
                {
                    int ch = (b2 & 0xf) % 4;
                    int op = (b2 & 0xf) / 4;
                    fprintf(out, "Amplitude modulation/1st decay rate: ");
                    if (ch != 4)
                    {
                        // Valid
                        fprintf(out, "ch %d op %d ", ch, op);
                        fprintf(out, "AM %s, D1R %d\n", ON(b2>>7), b2 & 0x1f);
                    }
                    else
                    {
                        // Invalid
                        fprintf(out, "Invalid data\n");
                    }
                }
                break;
            case 0x7: // 2nd decay rate
                {
                    int ch = (b2 & 0xf) % 4;
                    int op = (b2 & 0xf) / 4;
                    fprintf(out, "2nd decay rate: ");
                    if (ch != 4)
                    {
                        // Valid
                        fprintf(out, "ch %d op %d ", ch, op);
                        fprintf(out, "D2R %d\n", b2 & 0x1f);
                    }
                    else
                    {
                        // Invalid
                        fprintf(out, "Invalid data\n");
                    }
                }
                break;
            case 0x8: // 1st decay end level
                {
                    int ch = (b2 & 0xf) % 4;
                    int op = (b2 & 0xf) / 4;
                    fprintf(out, "1st decay end level/release rate: ");
                    if (ch != 4)
                    {
                        // Valid
                        fprintf(out, "ch %d op %d ", ch, op);
                        fprintf(out, "D1L %d, RR %d\n", (b2 >> 4) * 8, (b2 & 0xf) << 1 & 1);
                    }
                    else
                    {
                        // Invalid
                        fprintf(out, "Invalid data\n");
                    }
                }
                break;
            default:
                fprintf(out, "port 0 reg 0x%02x data 0x%02x\n", b1, b2);
                break;
            }
            break;
        case VGM_YM2612_1: // YM2612 port 1
            b1 = gzgetc(in);
            b2 = gzgetc(in);
            fprintf(out, "%02x %02x YM2612: ", b1, b2);
            fprintf(out, "port 1 reg 0x%02x data 0x%02x\n", b1, b2);
            break;
        case VGM_YM2151: // YM2151
            b1 = gzgetc(in);
            b2 = gzgetc(in);
            fprintf(out, "%02x %02x YM2151 reg 0x%02x data 0x%02x\n", b1, b2, b1, b2);
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
            fprintf(out, "%02x %02x YM????\n", b1, b2);
            break;
        case VGM_PAUSE_WORD: // Wait n samples
            b1 = gzgetc(in);
            b2 = gzgetc(in);
            SampleCount += b1 | b2 << 8;
            fprintf(out, "%02x %02x Wait:   %5d samples (%7.2f ms) (total %8d samples (%d:%05.2f))\n", b1, b2, 
                b1 | b2 << 8, (b1 | b2 << 8) / 44.1, SampleCount, SampleCount / 2646000, 
                SampleCount % 2646000 / 44100.0);
            break;
        case VGM_PAUSE_60TH: // Wait 1/60 s
            SampleCount += LEN60TH;
            fprintf(out, "      Wait:     735 samples (1/60s)      (total %8d samples (%d:%05.2f))\n", SampleCount, 
                SampleCount / 2646000, SampleCount % 2646000 / 44100.0);
            break;
        case VGM_PAUSE_50TH: // Wait 1/50 s
            SampleCount += LEN50TH;
            fprintf(out, "      Wait:   882 samples (1/50s)      (total %8d samples (%d:%05.2f))\n", SampleCount, 
                SampleCount / 2646000, SampleCount % 2646000 / 44100.0);
            break;
        //    case VGM_PAUSE_BYTE: // Wait n samples (byte)
        //      b1=gzgetc(in);
        //      SampleCount+=b1;
        //      fprintf(out, "%02x    Wait:   %5d samples (%7.2f ms) (total %8d samples (%d:%05.2f))\n", b1, b1, b1/44.1, SampleCount, SampleCount/2646000, SampleCount%2646000/44100.0);
        //      break;
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
            fprintf(out, "      Wait:   %5d samples (%7.2f ms) (total %8d samples (%d:%05.2f))\n", b1, b1 / 44.1, 
                SampleCount, SampleCount / 2646000, SampleCount % 2646000 / 44100.0);
            break;
        case VGM_END: // End of sound data... report
            fputs("      End of music data\n", out);
            gzclose(in);
            fclose(out);
            return;
        default:
            fputs("Unknown/invalid data\n", out);
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
    fclose(out);

    callback.show_status("Write to text complete");
}
