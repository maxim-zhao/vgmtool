#include "convert.h"
#include <zlib.h>
#include <cstdio>
#include <filesystem>

#include "IVGMToolCallback.h"
#include "vgm.h"
#include "utils.h"

struct TGYMXHeader
{
    char gym_id[4];
    char song_title[32];
    char game_title[32];
    char game_publisher[32];
    char dumper_emu[32];
    char dumper_person[32];
    char comments[256];
    uint32_t looped;
    uint32_t compressed;
};

void spread_dac(gzFile in, gzFile out)
{
    // Spread remaining DAC data up until next pause evenly throughout frame

    // When this function is called, in and out are open and the last byte read was
    // the first 0x2a DAC data address byte.

    int numDacValues = 1;
    const int inFileDataStart = gztell(in);

    // 1. Count how many DAC values there are
    do
    {
        gzgetc(in); // Skip DAC data
        const auto data = gzgetc(in); // Read next byte (00 for wait or 01 for port 0 for DAC/timer)
        if ((data == 0) || (data == EOF))
        {
            break; // Exit when I find a pause or EOF
        }
        switch (data)
        {
        case 0x01:
            {
                const auto address = gzgetc(in); // Read next byte if not a wait (2a for DAC, 27 for timer)
                if (address == 0x2a)
                {
                    numDacValues++; // counter starts at 1 since if I'm here there's at least one
                }
                break;
            }
        case 0x02:
            gzgetc(in);
            break;
        default:
            break;
        }
    }
    while (true);

    // 2. Seek back to data start (01 of first 01 2a)
    gzseek(in, inFileDataStart - 2, SEEK_SET);

    // 3. Read data again, this time outputting it with wait commands between
    int i = 0;
    do
    {
        const auto data = gzgetc(in); // GYM data type byte
        if (data == 0 || data == EOF)
        {
            break;
        }

        switch (data)
        {
        case 0x01:
            {
                gzputc(out, VGM_YM2612_0);
                const auto address = gzgetc(in); // 2a for DAC, etc
                gzputc(out, address);
                gzputc(out, gzgetc(in));
                if (address == 0x2a)
                {
                    // Got to DAC data so let's pause
                    // Calculate pause length
                    const long waitLength = (LEN60TH * (i + 1) / numDacValues) - (LEN60TH * i / numDacValues);
                    // Write pause
                    write_pause(out, waitLength);
                    // Increment counter
                    ++i;
                }
                break;
            }
        case 0x02:
            gzputc(out, VGM_YM2612_1);
            gzputc(out, gzgetc(in));
            gzputc(out, gzgetc(in));
            break;
        case 0x03:
            gzputc(out, VGM_PSG);
            gzputc(out, gzgetc(in));
            break;
        default:
            break;
        }
    }
    while (true);

    // 4. Finished!
}

bool Convert::to_vgm(const std::string& filename, file_type fileType, const IVGMToolCallback& callback)
{
    if (!Utils::file_exists(filename))
    {
        return false;
    }

    // Make output filename filename.gym.vgm
    const auto outFilename = filename + ".vgm";

    callback.show_status(Utils::format("Converting \"%s\" to VGM format...", filename.c_str()));

    // Open files
    gzFile in = gzopen(filename.c_str(), "rb");
    gzFile out = gzopen(outFilename.c_str(), "wb0");

    gzseek(out, VGM_DATA_OFFSET, SEEK_SET);

    // Fill in VGM header
    VGMHeader vgmHeader;
    vgmHeader.RecordingRate = 60;
    vgmHeader.Version = 0x110;

    switch (fileType)
    {
    case file_type::gym:
        {
            // GYM format:
            // 00    wait
            // 01 aa dd  YM2612 port 0 address aa data dd
            // 02 aa dd  YM2612 port 1 address aa data dd
            // 03 dd  PSG data dd

            TGYMXHeader GYMXHeader{};

            // Check for GYMX header
            gzread(in, &GYMXHeader, sizeof(GYMXHeader));

            if (strncmp(GYMXHeader.gym_id, "GYMX", 4) == 0)
            {
                // File has a GYM header

                // Is the file compressed?
                if (GYMXHeader.compressed)
                {
                    // Can't handle that
                    callback.show_conversion_progress(Utils::format("Cannot convert compressed GYM \"%s\" - see vgmtool.txt", filename.c_str()));
                    gzclose(out);
                    gzclose(in);
                    std::filesystem::remove(outFilename.c_str());
                    return false;
                }

                // To do: put the tag information in a GD3, handle looping

                if (GYMXHeader.looped)
                {
                    // Figure out the loop point in samples
                    // Store it temporarily in LoopLength
                    vgmHeader.LoopLength = (GYMXHeader.looped - 1) * LEN60TH;
                }

                // Seek to the GYM data
                gzseek(in, 428, SEEK_SET);
            }

            // To do:
            // GYMX handling
            // - GD3 from header
            // - looping (check it works)
            do
            {
                if (GYMXHeader.looped && (vgmHeader.TotalLength == vgmHeader.LoopLength))
                {
                    vgmHeader.LoopOffset = gztell(out) - LOOPDELTA;
                }
                auto b1 = gzgetc(in);
                switch (b1)
                {
                case 0: // Wait 1/60s
                    gzputc(out, VGM_PAUSE_60TH);
                    vgmHeader.TotalLength += LEN60TH;
                    break;
                case 1: // YM2612 port 0
                    b1 = gzgetc(in);
                    if (b1 == 0x2a)
                    {
                        // DAC...
                        spread_dac(in, out);
                        vgmHeader.TotalLength += LEN60TH;
                        // SpreadDAC absorbs the next wait command
                    }
                    else
                    {
                        gzputc(out, VGM_YM2612_0);
                        gzputc(out, b1);
                        b1 = gzgetc(in);
                        gzputc(out, b1);
                    }
                    vgmHeader.YM2612Clock = 7670454; // 3579545*15/7
                    break;
                case 2: // YM2612 port 1
                    gzputc(out, VGM_YM2612_1);
                    b1 = gzgetc(in);
                    gzputc(out, b1);
                    b1 = gzgetc(in);
                    gzputc(out, b1);
                    vgmHeader.YM2612Clock = 7670454;
                    break;
                case 3: // PSG
                    gzputc(out, VGM_PSG);
                    b1 = gzgetc(in);
                    gzputc(out, b1);
                    vgmHeader.PSGClock = 3579545;
                    vgmHeader.PSGShiftRegisterWidth = 16;
                    vgmHeader.PSGWhiteNoiseFeedback = 0x0009;
                    break;
                default: // Ignore unwanted bytes & EOF
                    break;
                }
            }
            while (!gzeof(in));
            if (GYMXHeader.looped)
            {
                // Convert LoopLength from AB to BC
                // A                     B                     C
                // start --------------- loop point -------- end
                vgmHeader.LoopLength = vgmHeader.TotalLength - vgmHeader.LoopLength;
            }
        }
        break;
    case file_type::ssl:
        {
            // SSL format:
            // 00    wait
            // 03 dd  PSG data dd
            // 04 dd  GG stereo dd
            // 05 aa  YM2413 address aa
            // 06 dd  YM2413 data dd
            int ym2413Address = 0;
            do
            {
                auto b1 = gzgetc(in);
                switch (b1)
                {
                case 0: // Wait 1/60s
                    gzputc(out, VGM_PAUSE_60TH);
                    vgmHeader.TotalLength += LEN60TH;
                    break;
                case 3: // PSG
                    gzputc(out, VGM_PSG);
                    b1 = gzgetc(in);
                    gzputc(out, b1);
                    vgmHeader.PSGClock = 3579545;
                    vgmHeader.PSGShiftRegisterWidth = 16;
                    vgmHeader.PSGWhiteNoiseFeedback = 0x0009;
                    break;
                case 4: // GG stereo
                    gzputc(out, VGM_GGST);
                    b1 = gzgetc(in);
                    gzputc(out, b1);
                    break;
                case 5: // YM2413 address
                    ym2413Address = gzgetc(in);
                    break;
                case 6: // YM2413 data
                    gzputc(out, VGM_YM2413);
                    gzputc(out, ym2413Address);
                    b1 = gzgetc(in);
                    gzputc(out, b1);
                    vgmHeader.YM2413Clock = 3579545;
                    break;
                default: // Ignore unwanted bytes & EOF
                    break;
                }
            }
            while (!gzeof(in));
            break;
        }
    case file_type::cym:
        // CYM format
        // 00    wait
        // aa dd  YM2151 address aa data dd
        do
        {
            auto b1 = gzgetc(in);
            switch (b1)
            {
            case 0: // Wait 1/60s
                gzputc(out, VGM_PAUSE_60TH);
                vgmHeader.TotalLength += LEN60TH;
                break;
            case EOF: // Needs specific handling this time
                break;
            default: // Other data
                gzputc(out, VGM_YM2151);
                gzputc(out, b1);
                b1 = gzgetc(in);
                gzputc(out, b1);
                vgmHeader.YM2151Clock = 7670454;
                break;
            }
        }
        while (!gzeof(in));
        break;
    }

    gzputc(out, VGM_END);

    // Fill in more of the VGM header
    vgmHeader.EoFOffset = gztell(out) - EOFDELTA;

    // Close files
    gzclose(out);
    gzclose(in);

    // Update header
    write_vgm_header(outFilename.c_str(), vgmHeader, callback);

    // Do a final compression round
    Utils::compress(outFilename);

    // Report
    callback.show_conversion_progress(Utils::format(R"(Converted "%s" to "%s")", filename.c_str(), outFilename.c_str()));
    return true;
}

