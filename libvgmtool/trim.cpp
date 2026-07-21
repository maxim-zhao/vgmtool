#include <cstdlib>
#include <cstdio>
#include "trim.h"

#include <filesystem>
#include <zlib.h>
#include "vgm.h"
#include "gd3.h"
#include "IStatusCallback.h"
#include "optimise.h"
#include "SN76489State.h"
#include "utils.h"
#include "VgmFile.h"
#include "VgmCommands.h"

//----------------------------------------------------------------------------------------------
// Creates a log of the trim for future reference
// in a file called "editpoints.txt" in the VGM's directory
//----------------------------------------------------------------------------------------------
void log_trim(const std::string& filename, const int start, const int loop, const int end, const IStatusCallback& callback)
{
    const auto slashPos = filename.find_last_of("\\/");
    if (slashPos == std::string::npos)
    {
        return;
    }

    // make filename to log to
    const auto logFilename = filename.substr(0, slashPos + 1) + "editpoints.txt";

    FILE* f = fopen(logFilename.c_str(), "a"); // open file for append

    if (f == nullptr)
    {
        callback.error("Error opening " + logFilename);
    }
    else
    {
        fprintf(f, "Filename: %s\n"
            "Start:    %d\n"
            "Loop:     %d\n"
            "End:      %d\n\n",
            filename.substr(slashPos + 1).c_str(),
            start,
            loop,
            end);
    }

    fclose(f);
}

// Merges data by storing writes to a buffer
// When it gets to a pause, it writes every value that has changed since
// the last write (at the last pause, or state write), and stores the
// current state for comparison next time.


struct TPSGState
{
    uint8_t GGStereo; // GG stereo byte
    uint16_t ToneFreqs[3];
    uint8_t NoiseByte;
    uint8_t Volumes[4];
    uint8_t PSGFrequencyLowBits, Channel;
    bool NoiseUpdated;
};

static TPSGState LastWrittenPSGState = {
    .GGStereo = 0xff, // GG stereo - all on
    .ToneFreqs = {0, 0, 0}, // Tone channels - off
    .NoiseByte = 0xe5, // Noise byte - white, medium
    .Volumes = {15, 15, 15, 15}, // Volumes - all off
    .PSGFrequencyLowBits = 0,
    .Channel = 4, // PSG low bits, channel - 4 so no values needed
    .NoiseUpdated = false
};

uint8_t LastWrittenYM2413Regs[YM2413NumRegs] = ""; // zeroes it

// The game could have moved a key from on to off to on within 1
// frame/pause... so I need to keep a check on that >:(
// Bits
// 0-4 = percussion bits
// 5-13 = tone channels
static int KeysLifted = 0;
static int KeysPressed = 0;

const int YM2413StateRegWriteFlags[YM2413NumRegs] = {
    //  Chooses bits when writing states (start/loop)
    //  Regs:
    //  0    1    2    3    4    5    6    7    8    9    a    b    c    d    e    f
    //  00-07: user-definable tone channel - left at 0xff for now
    //  0E:    rhythm mode control - only bit 5 since rest are unused/keys
    //         Was 0x20, now 0x00 (bugfix?)
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    //  10-18: tone F-number low bits - want all bits
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    //  20-28: tone F-number high bit, octave set, "key" & sustain
    //  0x3f = all
    //  0x2f = all but key
    //  0x1f = all but sustain
    //  0x0f = all but key and sustain
    0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    //  30-38: instrument number/volume - want all bits
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};

const int YM2413RegWriteFlags[YM2413NumRegs] = {
    //  Chooses bits when copying data -> want everything
    //  Regs:
    //  0    1    2    3    4    5    6    7    8    9    a    b    c    d    e    f
    //  00-07: user-definable tone channel - left at 0xff for now
    //  0E:    rhythm mode control - all (bits 0-5)
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0x00,
    //  10-18: tone F-number low bits - want all bits
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    //  20-28: tone F-number high bit, octave set, "key" & sustain
    //  0x3f = all (bits 0-5)
    0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    //  30-38: instrument number/volume - want all bits
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};

void WriteVGMInfo(const gzFile out, long* pauselength, TPSGState* PSGState, uint8_t YM2413Regs[YM2413NumRegs])
{
    int i;
    if (!*pauselength)
    {
        return;
    }
    // If pause=zero do nothing so only the last write to each register before a pause is written
    // Write PSG stuff
    if (PSGState->GGStereo != LastWrittenPSGState.GGStereo)
    {
        // GG stereo
        gzputc(out, VGM_GGST);
        gzputc(out, PSGState->GGStereo);
    }
    for (i = 0; i < 3; ++i)
    {
        // Tone channel frequencies
        if (PSGState->ToneFreqs[i] != LastWrittenPSGState.ToneFreqs[i])
        {
            gzputc(out, VGM_PSG);
            gzputc(out,
                0x80 | // bit 7 = 1, 4 = 0 -> PSG tone byte 1
                (i << 5) | // bits 5 and 6 -> select channel
                (PSGState->ToneFreqs[i] & 0xf) // bits 0 to 3 -> low 4 bits of freq
            );
            gzputc(out, VGM_PSG);
            gzputc(out,
                // bits 6 and 7 = 0 -> PSG tone byte 2
                (PSGState->ToneFreqs[i] >> 4) // bits 0 to 5 -> high 6 bits of freq
            );
        }
    }
    if (PSGState->NoiseUpdated)
    {
        // Writing to the noise register resets the LFSR so I have to always repeat writes
        gzputc(out, VGM_PSG); // 1-stage write
        gzputc(out, PSGState->NoiseByte);
        PSGState->NoiseUpdated = 0;
    }
    for (i = 0; i < 4; ++i)
    {
        // All 4 channels volumes
        if (PSGState->Volumes[i] != LastWrittenPSGState.Volumes[i])
        {
            gzputc(out, VGM_PSG);
            gzputc(out,
                0x90 | // bits 4 and 7 = 1 -> volume
                (i << 5) | // bits 5 and 6 -> select channel
                PSGState->Volumes[i] // bits 0 to 4 -> volume
            );
        }
    }
    // Write YM2413 stuff
    for (i = 0; i < YM2413NumRegs; ++i)
    {
        if (YM2413RegWriteFlags[i])
        {
            if (
                ((YM2413Regs[i] & YM2413RegWriteFlags[i]) != LastWrittenYM2413Regs[i]) &&
                (i != 0x0e) // not percussion
            )
            {
                gzputc(out, VGM_YM2413); // YM2413
                gzputc(out, i); // Register
                gzputc(out, (YM2413Regs[i] & YM2413RegWriteFlags[i])); // Value
                LastWrittenYM2413Regs[i] = YM2413Regs[i] & YM2413RegWriteFlags[i];
            }
        }
    }
    // Percussion after tone
    if (LastWrittenYM2413Regs[0x0e] != (YM2413Regs[0x0e] & YM2413RegWriteFlags[0x0e]))
    {
        gzputc(out, VGM_YM2413); // YM2413
        gzputc(out, 0x0e); // Register
        gzputc(out, (YM2413Regs[0x0e] & YM2413RegWriteFlags[0x0e])); // Value
        LastWrittenYM2413Regs[0x0e] = YM2413Regs[0x0e] & YM2413RegWriteFlags[0x0e];
    }
    // Now we have to handle keys which have gone from lifted to
    // pressed since the last write - if they were pressed at the last
    // write, that means they've gone Down-Up-Down, and we need to tell
    // the YM2413 this since it would otherwise be missed by the
    // optimiser
    if (KeysLifted & KeysPressed)
    {
        // At least one key was lifted and pressed
        // First the percussion - find which keys have done DUD
        uint8_t PercussionKeysDUD = LastWrittenYM2413Regs[0x0e] & 0x1f // 1 for each inst currently set to on
            & KeysLifted & KeysPressed; // 1 for each inst on-off-on
        int ToneKeysUD = KeysLifted & KeysPressed & 0x1fe0;
        if (ToneKeysUD)
        {
            // At least one tone key went from up to down. So look to
            // see if any of them were D before because if so, they've
            // gone DUD
            for (i = 0; i < 9; ++i)
            {
                // for each tone channel
                if (
                    (ToneKeysUD & (1 << (5 + i))) && // if it's gone UD
                    (LastWrittenYM2413Regs[0x20 + i] & 0x10) // and was D before
                )
                {
                    // debug marker
                    //          gzputc(out, 0x00);gzputc(out, 0x00);gzputc(out, 0x00);gzputc(out, 0x00);gzputc(out, 0x00);gzputc(out, 0x00);gzputc(out, 0x00);gzputc(out, 0x00);gzputc(out, 0x00);
                    gzputc(out, VGM_YM2413); // YM2413
                    gzputc(out, 0x20 + i); // Register
                    gzputc(out,
                        (LastWrittenYM2413Regs[0x20 + i] & 0x2f) // Turn off the key
                    ); // Value
                    gzputc(out, VGM_YM2413); // YM2413
                    gzputc(out, 0x20 + i); // Register
                    gzputc(out, LastWrittenYM2413Regs[0x20 + i]);
                }
            }
        }
        // Do percussion last in case any ch6-8 writes changed it
        if (
            (PercussionKeysDUD) && // At least one key has gone UDU
            (LastWrittenYM2413Regs[0x0e] & 0x20) // Percussion is on
        )
        {
            gzputc(out, VGM_YM2413); // YM2413
            gzputc(out, 0x0e); // Register
            gzputc(out,
                (LastWrittenYM2413Regs[0x0e] ^ PercussionKeysDUD) // Turn off the ones I want
                | 0x20
            ); // Value
            gzputc(out, VGM_YM2413); // YM2413
            gzputc(out, 0x0e); // Register
            gzputc(out, LastWrittenYM2413Regs[0x0e]);
        }
    }
    KeysLifted = 0;
    KeysPressed = 0;
    // then pause
    write_pause(out, *pauselength);
    *pauselength = 0; // maybe not needed
    // and record what we've done
    LastWrittenPSGState = *PSGState;
}

void WritePSGState(const gzFile out, TPSGState& PSGState)
{
    int i;
    // GG stereo
    gzputc(out, VGM_GGST);
    gzputc(out, PSGState.GGStereo);
    // Tone channel frequencies
    for (i = 0; i < 3; ++i)
    {
        gzputc(out, VGM_PSG);
        gzputc(out,
            0x80 | // bit 7 = 1, 4 = 0 -> PSG tone byte 1
            (i << 5) | // bits 5 and 6 -> select channel
            (PSGState.ToneFreqs[i] & 0xf) // bits 0 to 3 -> low 4 bits of freq
        );
        gzputc(out, VGM_PSG);
        gzputc(out,
            // bits 6 and 7 = 0 -> PSG tone byte 2
            (PSGState.ToneFreqs[i] >> 4) // bits 0 to 5 -> high 6 bits of freq
        );
    }
    // Noise
    gzputc(out, VGM_PSG); // 1-stage write
    gzputc(out, PSGState.NoiseByte);
    PSGState.NoiseUpdated = false;

    // All 4 channels volumes
    for (i = 0; i < 4; ++i)
    {
        gzputc(out, VGM_PSG);
        gzputc(out,
            0x90 | // bits 4 and 7 = 1 -> volume
            (i << 5) | // bits 5 and 6 -> select channel
            PSGState.Volumes[i] // bits 0 to 4 -> volume
        );
    }
    LastWrittenPSGState = PSGState;
}

void WriteYM2413State(const gzFile out, uint8_t YM2413Regs[YM2413NumRegs], const int IsStart)
{
    for (int i = 0; i < YM2413NumRegs; ++i)
    {
        if (YM2413StateRegWriteFlags[i])
        {
            gzputc(out, VGM_YM2413); // YM2413
            gzputc(out, i); // Register
            gzputc(out, (YM2413Regs[i] & YM2413StateRegWriteFlags[i])); // Value
            LastWrittenYM2413Regs[i] = YM2413Regs[i] & YM2413StateRegWriteFlags[i];
        }
    }

    if (IsStart)
    {
        // Write percussion
        gzputc(out, VGM_YM2413);
        gzputc(out, 0x0e);
        gzputc(out, YM2413Regs[0x0e] & YM2413RegWriteFlags[0x0e]);
        LastWrittenYM2413Regs[0x0e] = YM2413Regs[0x0e] & YM2413RegWriteFlags[0x0e];
    }
}

// Merges data by storing writes to a buffer
// When it gets to a pause, it writes every value that has changed since
// the last write (at the last pause, or state write), and stores the
// current state for comparison next time.
void trim(const std::string& filename,
          int start,
          int loop,
          int end,
          bool overWrite,
          bool logTrims,
          const IStatusCallback& callback,
          std::string outFilename)
{
    callback.verbose_message(std::format("Trimming {}: start {}, loop {}, end {}", filename, start, loop, end));

    if (!Utils::file_exists(filename))
    {
        return;
    }

    if (logTrims)
    {
        log_trim(filename, start, loop, end, callback);
    }

    uint8_t YM2413Regs[YM2413NumRegs]{};

    if ((start > end) || (loop > end) || ((loop > -1) && (loop < start)))
    {
        callback.error(std::format("Impossible edit points: start={}, loop={}, end={}", start, loop, end));
        return;
    }

    auto in = gzopen(filename.c_str(), "rb");

    // Read header
    OldVGMHeader vgmHeader;
    if (!ReadVGMHeader(in, &vgmHeader, callback))
    {
        gzclose(in);
        return;
    }

    auto fileSizeBefore = vgmHeader.EoFOffset + EOFDELTA;

    gzseek(in, VGM_DATA_OFFSET, SEEK_SET);

    callback.verbose_message("Trimming VGM data...");

    if (outFilename.empty())
    {
        if (auto dotPosition = filename.find_last_of(".\\/");
            dotPosition == std::string::npos || filename[dotPosition] != '.')
        {
            // No dot, just append
            outFilename = filename + " (trimmed).vgm";
        }
        else
        {
            outFilename = filename.substr(0, dotPosition) + " (trimmed).vgm";
        }
    }

    auto out = gzopen(outFilename.c_str(), "wb0"); // No compression, since I'll recompress it later

    // Write header (update it later)
    gzwrite(out, &vgmHeader, sizeof(vgmHeader));
    gzseek(out, VGM_DATA_OFFSET, SEEK_SET);

    if (end > static_cast<int>(vgmHeader.TotalLength))
    {
        callback.message(std::format(
            "End point ({} samples) beyond end of file!\nUsing maximum value of {} samples instead",
            end,
            vgmHeader.TotalLength));
        end = static_cast<int>(vgmHeader.TotalLength);
    }

    TPSGState currentPsgState = {
        0xff, // GG stereo - all on
        {0, 0, 0}, // Tone channels - off
        0xe5, // Noise byte - white, medium
        {15, 15, 15, 15}, // Volumes - all off
        0, 4, // PSG low bits, channel - 4 so no values needed, 
        false
    };

    bool writtenStart = false;
    bool writtenLoop = false;
    long int pauseLength = 0;
    int lastFirstByteWritten = -1;
    long sampleCount = 0;

    bool havePSG = false;
    bool haveYM2413 = false;
    // Check for use of chips
    for (auto atEnd = false; !atEnd;)
    {
        switch (gzgetc(in))
        {
        case VGM_GGST:
        case VGM_PSG:
            havePSG = true;
            gzgetc(in);
            break;
        case VGM_YM2413:
            haveYM2413 = true;
            gzgetc(in);
            gzgetc(in);
            break;
        case VGM_YM2612_0: // YM2612 port 0
        case VGM_YM2612_1: // YM2612 port 1
        case VGM_YM2151: // YM2151
        case 0x55: // Reserved up to 0x5f
        case 0x56: // All have 2 bytes of data
        case 0x57: // which I discard :)
        case 0x58:
        case 0x59:
        case 0x5a:
        case 0x5b:
        case 0x5c:
        case 0x5d:
        case 0x5e:
        case 0x5f:
        case VGM_PAUSE_WORD: // Wait n samples
            // 2-byte commands
            gzgetc(in);
            gzgetc(in);
            break;
        case VGM_PAUSE_60TH:
        case VGM_PAUSE_50TH:
            // 0 bytes payload
            break;
        case VGM_END:
            atEnd = true;
            break;
        default:
            // Unknown
            break;
        }
    }

    // Back to start of data
    gzseek(in, VGM_DATA_OFFSET, SEEK_SET);


    for (auto atEnd = false; !atEnd;)
    {
        switch (const auto b0 = gzgetc(in))
        {
        case VGM_GGST: // GG stereo (1 byte data)
            {
                auto b1 = gzgetc(in);
                if (b1 != currentPsgState.GGStereo)
                {
                    // Do not copy it if the current stereo state is the same
                    currentPsgState.GGStereo = static_cast<uint8_t>(b1);
                    if ((writtenStart) && (vgmHeader.PSGClock))
                    {
                        WriteVGMInfo(out, &pauseLength, &currentPsgState, YM2413Regs);
                    }
                }
                break;
            }
        case VGM_PSG: // PSG write (1 byte data)
            {
                const auto b1 = gzgetc(in);
                switch (b1 & 0x90)
                {
                case 0x00: // fall through
                case 0x10: // second frequency byte
                    if (currentPsgState.Channel > 3)
                    {
                        break;
                    }
                    if (currentPsgState.Channel == 3)
                    {
                        // 2nd noise byte (Micro Machines title screen)
                        // Always write
                        currentPsgState.NoiseUpdated = true;
                        currentPsgState.NoiseByte = (b1 & 0xf) | 0xe0;
                        if ((writtenStart) && (vgmHeader.PSGClock))
                        {
                            WriteVGMInfo(out, &pauseLength, &currentPsgState, YM2413Regs);
                        }
                    }
                    else
                    {
                        int freq = (b1 & 0x3F) << 4 | currentPsgState.PSGFrequencyLowBits;
                        if (freq < PSGCutoff)
                        {
                            freq = 0;
                        }
                        if (currentPsgState.ToneFreqs[currentPsgState.Channel] != freq)
                        {
                            // Changes the freq
                            uint8_t firstByte = 0x80 | (currentPsgState.Channel << 5) | currentPsgState.
                                PSGFrequencyLowBits;
                            // 1st byte needed for this freq
                            if (firstByte != lastFirstByteWritten)
                            {
                                // If necessary, write 1st byte
                                if ((writtenStart) && (vgmHeader.PSGClock))
                                {
                                    WriteVGMInfo(out, &pauseLength, &currentPsgState, YM2413Regs);
                                }
                                lastFirstByteWritten = firstByte;
                            }
                            // Don't write if volume is off
                            if ((writtenStart) && (vgmHeader.PSGClock)
                                /*&& (CurrentPSGState.Volumes[CurrentPSGState.Channel])*/)
                            {
                                WriteVGMInfo(out, &pauseLength, &currentPsgState, YM2413Regs);
                            }
                            currentPsgState.ToneFreqs[currentPsgState.Channel] = static_cast<uint16_t>(freq);
                            // Write 2nd byte
                        }
                        break;
                    }
                case 0x80:
                    if ((b1 & 0x60) == 0x60)
                    {
                        // noise
                        // No "does it change" because writing resets the LFSR (ie. has an effect)
                        currentPsgState.NoiseUpdated = true;
                        currentPsgState.NoiseByte = static_cast<uint8_t>(b1);
                        currentPsgState.Channel = 3;
                        if ((writtenStart) && (vgmHeader.PSGClock))
                        {
                            WriteVGMInfo(out, &pauseLength, &currentPsgState, YM2413Regs);
                        }
                    }
                    else
                    {
                        // First frequency byte
                        currentPsgState.Channel = (b1 & 0x60) >> 5;
                        currentPsgState.PSGFrequencyLowBits = b1 & 0xF;
                    }
                    break;
                case 0x90: // set volume
                    {
                        auto chan = (b1 & 0x60) >> 5;
                        if (uint8_t vol = b1 & 0xF;
                            currentPsgState.Volumes[chan] != vol)
                        {
                            currentPsgState.Volumes[chan] = vol;
                            currentPsgState.Channel = 4;
                            // Only write volume change if we've got to the start and PSG is turned on
                            if ((writtenStart) && (vgmHeader.PSGClock))
                            {
                                WriteVGMInfo(out, &pauseLength, &currentPsgState, YM2413Regs);
                            }
                        }
                    }
                    break;
                } // end case
                break;
            }
        case VGM_YM2413: // YM2413
            {
                const auto b1 = gzgetc(in);
                const auto b2 = gzgetc(in);
                if ((b1 >= YM2413NumRegs) || !(YM2413RegWriteFlags[b1]))
                {
                    break; // Discard invalid register numbers
                }
                YM2413Regs[b1] = static_cast<uint8_t>(b2);

                // Check for percussion keys lifted or pressed
                if (b1 == 0x0e)
                {
                    KeysLifted |= (b2 ^ 0x1f) & 0x1f;
                    // OR the percussion keys with the inverse of the perc.
                    // keys to get a 1 stored there if the key has been lifted
                    KeysPressed |= (b2 & 0x1f);
                    // Do similar with the non-inverse to get the keys pressed
                }
                // Check for tone keys lifted or pressed
                if ((b1 >= 0x20) && (b1 <= 0x28))
                {
                    if (b2 & 0x10)
                    {
                        // Key was pressed
                        KeysPressed |= 1 << (b1 - 0x20 + 5);
                    }
                    else
                    {
                        KeysLifted |= 1 << (b1 - 0x20 + 5);
                    }
                    if (
                        (KeysLifted & KeysPressed & (1 << (b1 - 0x20 + 5))) // the key has gone UD
                        &&
                        (LastWrittenYM2413Regs[b1] & 0x10) // 0x10 == 00010000 == YM2413 tone key bit
                        // ie. the key was D before UD
                    )
                    {
                        KeysLifted = KeysLifted;
                    }
                }

                if ((writtenStart) && (vgmHeader.YM2413Clock))
                {
                    WriteVGMInfo(out, &pauseLength, &currentPsgState, YM2413Regs);
                }
                break;
            }
        case VGM_YM2612_0: // YM2612 port 0
        case VGM_YM2612_1: // YM2612 port 1
        case VGM_YM2151: // YM2151
            {
                const auto b1 = gzgetc(in);
                const auto b2 = gzgetc(in);
                gzputc(out, b0);
                gzputc(out, b1);
                gzputc(out, b2);
                break;
            }
        case 0x55: // Reserved up to 0x5f
        case 0x56: // All have 2 bytes of data
        case 0x57: // which I discard :)
        case 0x58:
        case 0x59:
        case 0x5a:
        case 0x5b:
        case 0x5c:
        case 0x5d:
        case 0x5e:
        case 0x5f:
            gzgetc(in);
            gzgetc(in);
            break;
        case VGM_PAUSE_WORD: // Wait n samples
            {
                auto b1 = gzgetc(in);
                auto b2 = gzgetc(in);
                sampleCount += b1 | (b2 << 8);
                pauseLength += b1 | (b2 << 8);
                if ((writtenStart) && ((sampleCount <= loop) || (loop == -1) || (writtenLoop)) && (sampleCount <=
                    end))
                {
                    WriteVGMInfo(out, &pauseLength, &currentPsgState, YM2413Regs);
                }
                break;
            }
        case VGM_PAUSE_60TH: // Wait 1/60 s
            sampleCount += LEN60TH;
            pauseLength += LEN60TH;
            if ((writtenStart) && ((sampleCount <= loop) || (loop == -1) || (writtenLoop)) && (sampleCount <= end))
            {
                WriteVGMInfo(out, &pauseLength, &currentPsgState, YM2413Regs);
            }
            break;
        case VGM_PAUSE_50TH: // Wait 1/50 s
            sampleCount += LEN50TH;
            pauseLength += LEN50TH;
            if ((writtenStart) && ((sampleCount <= loop) || (loop == -1) || (writtenLoop)) && (sampleCount <= end))
            {
                WriteVGMInfo(out, &pauseLength, &currentPsgState, YM2413Regs);
            }
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
            {
                auto waitLength = (b0 & 0xf) + 1;
                sampleCount += waitLength;
                pauseLength += waitLength;
                if ((writtenStart) && ((sampleCount <= loop) || (loop == -1) || (writtenLoop)) && (sampleCount <=
                    end))
                {
                    WriteVGMInfo(out, &pauseLength, &currentPsgState, YM2413Regs);
                }
            }
            break;
        case VGM_END: // End of sound data
            gzclose(in);
            gzclose(out);
            callback.error(
                "Reached end of VGM data! There must be something wrong - try fixing the lengths for this file");
            return;
        default:
            break;
        }

        // Loop point
        if ((!writtenLoop) && (loop != -1) && (sampleCount >= loop))
        {
            if (writtenStart)
            {
                pauseLength = loop - (sampleCount - pauseLength); // Write any remaining pause up to the edit point
                WriteVGMInfo(out, &pauseLength, &currentPsgState, YM2413Regs);
            }
            pauseLength = sampleCount - loop; // and remember any left over
            // Remember offset
            vgmHeader.LoopOffset = static_cast<uint32_t>(gztell(out) - LOOPDELTA);
            // Write loop point initialisation... unless start = loop
            // because then the start initialisation will work
            if (loop != start)
            {
                if (havePSG)
                {
                    currentPsgState.NoiseByte &= 0xf7;
                    WritePSGState(out, currentPsgState);
                }
                if (haveYM2413)
                {
                    WriteYM2413State(out, YM2413Regs, true);
                }
            }
            writtenLoop = 1;
        }
        // Start point
        if ((!writtenStart) && (sampleCount > start))
        {
            if (havePSG)
            {
                currentPsgState.NoiseByte &= 0xf7;
                WritePSGState(out, currentPsgState);
            }
            if (haveYM2413)
            {
                WriteYM2413State(out, YM2413Regs, true);
            }
            // Remember any needed delay
            pauseLength = sampleCount - start;
            writtenStart = 1;
        }

        // End point
        if (sampleCount >= end)
        {
            // Write remaining delay
            pauseLength -= sampleCount - end;
            write_pause(out, pauseLength);
            pauseLength = 0; // maybe not needed
            // End of VGM data
            gzputc(out, VGM_END);
            break;
        }
    }

    // Copy GD3 tag
    if (vgmHeader.GD3Offset)
    {
        TGD3Header GD3Header;
        const auto NewGD3Offset = gztell(out) - GD3DELTA;
        callback.verbose_message("Copying GD3 tag...");
        gzseek(in, vgmHeader.GD3Offset + GD3DELTA, SEEK_SET);
        gzread(in, &GD3Header, sizeof(GD3Header));
        gzwrite(out, &GD3Header, sizeof(GD3Header));
        for (auto i = 0u; i < GD3Header.length; ++i)
        {
            // Copy strings
            gzputc(out, gzgetc(in));
        }
        vgmHeader.GD3Offset = static_cast<uint32_t>(NewGD3Offset);
    }
    vgmHeader.EoFOffset = static_cast<uint32_t>(gztell(out) - EOFDELTA);
    gzclose(in);
    gzclose(out);

    // Update header
    vgmHeader.TotalLength = end - start;
    if (loop > -1)
    {
        // looped
        vgmHeader.LoopLength = end - loop;
        // Already remembered offset
    }
    else
    {
        // not looped
        vgmHeader.LoopLength = 0;
        vgmHeader.LoopOffset = 0;
    }

    // Amend it with the updated header
    write_vgm_header(outFilename, vgmHeader, callback);

    optimise_vgm_pauses(outFilename, callback);

    Utils::compress(outFilename, callback);

    if (overWrite)
    {
        Utils::replace_file(filename, outFilename);
        outFilename = filename;
    }

    // Get output file size
    in = gzopen(outFilename.c_str(), "rb");
    gzread(in, &vgmHeader, sizeof(vgmHeader));
    gzclose(in);
    auto fileSizeAfter = vgmHeader.EoFOffset + EOFDELTA;

    callback.verbose_message("Trimming complete");

    callback.verbose_message(std::format(
        "File {} to {} Uncompressed file size {} -> {} bytes ({:+.2f}%)",
        (overWrite
            ? "optimised"
            : "trimmed"),
        outFilename,
        fileSizeBefore,
        fileSizeAfter,
        (static_cast<double>(fileSizeAfter) - fileSizeBefore) * 100 / fileSizeBefore));
}

static void add_pause(std::vector<std::shared_ptr<VgmCommands::ICommand>>& stream, int pauseLength)
{
    if (pauseLength == 0)
    {
        return;
    }

    // This is not quite optimal - it depends upon what the length modulo 0xffff is.
    // If it is not any of the <3 byte options, we would be better off emitting a
    // 16-bit wait that makes it so, if possible. This is unlikely to happen
    // very often.

    while (pauseLength > 0xffff)
    {
        const auto wait = std::make_shared<VgmCommands::Wait16bit>();
        wait->set_duration(0xffff);
        stream.push_back(wait);
        pauseLength -= 0xffff;
    }

    // Two one-byte commands are more efficient than a three-byte wait
    if (pauseLength == LEN60TH * 2)
    {
        stream.push_back(std::make_shared<VgmCommands::Wait60th>());
        stream.push_back(std::make_shared<VgmCommands::Wait60th>());
    }
    else if (pauseLength == LEN60TH)
    {
        stream.push_back(std::make_shared<VgmCommands::Wait60th>());
    }
    else if (pauseLength == LEN50TH * 2)
    {
        stream.push_back(std::make_shared<VgmCommands::Wait50th>());
        stream.push_back(std::make_shared<VgmCommands::Wait50th>());
    }
    else if (pauseLength == LEN50TH)
    {
        stream.push_back(std::make_shared<VgmCommands::Wait50th>());
    }
    else if (pauseLength <= 16)
    {
        const auto wait = std::make_shared<VgmCommands::Wait4bit>();
        wait->set_duration(pauseLength);
        stream.push_back(wait);
    }
    else
    {
        const auto wait = std::make_shared<VgmCommands::Wait16bit>();
        wait->set_duration(static_cast<uint16_t>(pauseLength));
        stream.push_back(wait);
    }
}

void trim_vgm_file(VgmFile& vgmFile, int start, int loop, int end, const IStatusCallback& callback)
{
    callback.verbose_message(std::format("Trimming VGM file: start {}, loop {}, end {}", start, loop, end));

    auto& header = vgmFile.header();

    // Validate edit points
    if ((start > end) || (loop > end) || ((loop > -1) && (loop < start)))
    {
        callback.error(std::format("Impossible edit points: start={}, loop={}, end={}", start, loop, end));
        return;
    }

    if (std::cmp_greater(end, header.sample_count()))
    {
        callback.message(std::format(
            "End point ({} samples) beyond end of file!\nUsing maximum value of {} samples instead",
            end,
            header.sample_count()));
        end = static_cast<int>(header.sample_count());
    }

    callback.verbose_message("Trimming VGM data...");

    // Copy all the commands from the VGM file into one big stream
    CommandStream allCommands;
    std::ranges::copy(vgmFile.data_before_loop().commands(), std::back_inserter(allCommands.commands()));
    std::ranges::copy(vgmFile.data_with_loop().commands(), std::back_inserter(allCommands.commands()));

    // Initialize tracking state(s)
    // TODO: make this extensible to more chips
    SN76489State currentPsgState(header);
    SN76489State lastWrittenPsgState(header);
    SN76489State loopPsgState(header);
    SN76489State startPsgState(header);
    //uint8_t ym2413Regs[YM2413NumRegs]{};
    //uint8_t lastWrittenYM2413Regs[YM2413NumRegs]{};

    CommandStream* currentStream = nullptr;
    vgmFile.data_before_loop().commands().clear();
    vgmFile.data_with_loop().commands().clear();

    // I want to walk through all the data...
    // While time < start, we just track chip state.
    // When time passes start, we emit a full image and any remaining wait until either loop or end.
    // While time < end, we track chip state and emit waits and deltas.
    // When time passes loop, we capture the chip state for later.
    // When time passes end, we emit the delta to get back to the loop state.
    int time = 0; // TODO extend times to 64 bit? 2^32 samples is 2^32 / 44100 = 27 hours, so probably not needed for now
    for (const auto& command : allCommands.commands())
    {
        // Every command maybe has some time, and maybe changes the chip state(s).
        auto pendingTime = 0;
        if (const auto wait = std::dynamic_pointer_cast<VgmCommands::Wait>(command))
        {
            pendingTime += wait->duration();
        }
        switch (command->chip())  // NOLINT(clang-diagnostic-switch-enum)
        {
        case VgmHeader::Chip::Nothing:
            break;
        case VgmHeader::Chip::SN76489:
            if (const auto ggStereo = std::dynamic_pointer_cast<VgmCommands::GGStereo>(command))
            {
                currentPsgState.add(ggStereo);
            }
            else if (const auto sn76489 = std::dynamic_pointer_cast<VgmCommands::SN76489>(command))
            {
                currentPsgState.add(sn76489);
            }
            break;
            // TODO lots more chips to handle
        default:
            // Pass through commands for other chips, if we are past the start.
            if (time > start)
            {
                currentStream->commands().push_back(command);
            }
            break;
        }

        if (pendingTime > 0)
        {
            if (currentStream != nullptr)
            {
                // Emit the delta...
                currentPsgState.copy_to_command_stream(*currentStream, lastWrittenPsgState, false);
            }
            // Add it on... but maybe not all of it.
            const auto timeBefore = time;
            const auto timeAfter = time + pendingTime;
            // ---|-------|-----|----
            //    start   loop  end
            // Start and loop can be the same.
            // We might have moved past one or more of these, and we want to take action on each.
            // Note that we want to trigger the start and loop when we have pending time
            // moving us *after* the point in question, so that we capture the state
            // just before the time is added.
            if (timeBefore <= start && timeAfter > start)
            {
                // We are passing the start point
                currentStream = &vgmFile.data_before_loop();
                // Remember the state
                startPsgState = currentPsgState;
                lastWrittenPsgState = currentPsgState;
                // Subtract any time before the start point from the pending time
                pendingTime -= (start - timeBefore);
                // Emit as much time as we need to get to the loop point, or end, or just all the pending time
                auto timeToEmit = pendingTime;
                if (loop > -1 && timeAfter >= loop)
                {
                    // We are also passing the loop point. Only emit time up to there (might be 0).
                    timeToEmit = loop - start;
                }
                else if (timeAfter >= end)
                {
                    // We are also passing the end point. Only emit time up to there.
                    timeToEmit = end - start;
                }
                add_pause(currentStream->commands(), timeToEmit);
                pendingTime -= timeToEmit;
                time += timeToEmit;
            }
            if (timeBefore <= loop && timeAfter > loop)
            {
                // We are passing the loop point
                // Capture the current state for later
                loopPsgState = currentPsgState;
                // Switch to the second stream
                currentStream = &vgmFile.data_with_loop();
                // And as much time as we need to get to the end point, or just all the pending time
                const auto timeToEmit = std::min(pendingTime, end - loop);
                add_pause(currentStream->commands(), timeToEmit);
                pendingTime -= timeToEmit;
                time += timeToEmit;
            }
            if (timeBefore < end && timeAfter >= end)
            {
                // We are reaching or passing the end point
                // Emit as much time as we need to get to the end point, or just all the pending time
                const auto timeToEmit = std::min(pendingTime, end - time);
                add_pause(currentStream->commands(), timeToEmit);
                // Emit a delta to get back to the loop state
                loopPsgState.copy_to_command_stream(*currentStream, currentPsgState, false);
                // Add an end marker
                currentStream->commands().push_back(std::make_shared<VgmCommands::End>());
                // And then we are done. Break the outer loop.
                break;
            }
            if (pendingTime > 0 && currentStream != nullptr)
            {
                // We are not passing any edit points, so just emit all the pending time
                add_pause(currentStream->commands(), pendingTime);
            }
            // Finally, remember the new time
            time = timeAfter;
        }
    }

    // Inject the start state at the beginning
    // TODO make this conditional on it being needed for each chip
    CommandStream startState;
    currentPsgState.copy_to_command_stream(startState, startPsgState, true);
    vgmFile.data_before_loop().commands().insert(
        vgmFile.data_before_loop().commands().begin(),
        std::make_move_iterator(startState.commands().begin()),
        std::make_move_iterator(startState.commands().end())
    );

    // Update header
    header.set_sample_count(end - start);
    if (loop > -1)
    {
        header.set_loop_sample_count(end - loop);
    }
    else
    {
        header.set_loop_sample_count(0);
    }

    // TODO report here on the timings in m:ss.fff
    callback.verbose_message(std::format(
        "Trimming complete: {} commands -> {} + {}",
        allCommands.commands().size(),
        vgmFile.data_before_loop().commands().size(),
        vgmFile.data_with_loop().commands().size()));
}
