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

void Convert::gymToVgm(const std::string& filename, gzFile in, gzFile out, VGMHeader& vgmHeader)
{
    // GYM format:
    // 00    wait
    // 01 aa dd  YM2612 port 0 address aa data dd
    // 02 aa dd  YM2612 port 1 address aa data dd
    // 03 dd  PSG data dd

    // Check for GYMX header
    TGYMXHeader header{};
    gzread(in, &header, sizeof(header));

    if (strncmp(header.gym_id, "GYMX", 4) == 0)
    {
        // File has a GYM header

        // Is the file compressed?
        if (header.compressed)
        {
            // Can't handle that
            throw std::runtime_error(Utils::format("Cannot convert compressed GYM \"%s\" - see vgmtool.txt", filename.c_str()));
        }

        // To do: put the tag information in a GD3, handle looping

        if (header.looped)
        {
            // Figure out the loop point in samples
            // Store it temporarily in LoopLength
            vgmHeader.LoopLength = (header.looped - 1) * LEN60TH;
        }

        // Seek to the GYM data
        gzseek(in, 428, SEEK_SET);
    }

    // To do:
    // GYMX handling
    // - GD3 from header
    // - looping (check it works)
    while (!gzeof(in))
    {
        if (header.looped && (vgmHeader.TotalLength == vgmHeader.LoopLength))
        {
            vgmHeader.LoopOffset = gztell(out) - LOOPDELTA;
        }
        switch (gzgetc(in))
        {
        case 0: // Wait 1/60s
            gzputc(out, VGM_PAUSE_60TH);
            vgmHeader.TotalLength += LEN60TH;
            break;
        case 1: // YM2612 port 0
            if (const auto registerNumber = gzgetc(in); registerNumber == 0x2a)
            {
                // DAC...
                spread_dac(in, out);
                vgmHeader.TotalLength += LEN60TH;
                // SpreadDAC absorbs the next wait command
            }
            else
            {
                gzputc(out, VGM_YM2612_0);
                gzputc(out, registerNumber);
                gzputc(out, gzgetc(in));
            }
            vgmHeader.YM2612Clock = 7670454; // 3579545*15/7
            break;
        case 2: // YM2612 port 1
            gzputc(out, VGM_YM2612_1);
            gzputc(out, gzgetc(in));
            gzputc(out, gzgetc(in));
            vgmHeader.YM2612Clock = 7670454;
            break;
        case 3: // PSG
            gzputc(out, VGM_PSG);
            gzputc(out, gzgetc(in));
            vgmHeader.PSGClock = 3579545;
            vgmHeader.PSGShiftRegisterWidth = 16;
            vgmHeader.PSGWhiteNoiseFeedback = 0x0009;
            break;
        default: // Ignore unwanted bytes & EOF
            break;
        }
    }

    if (header.looped)
    {
        // Convert LoopLength from AB to BC
        // A                     B                     C
        // start --------------- loop point -------- end
        vgmHeader.LoopLength = vgmHeader.TotalLength - vgmHeader.LoopLength;
    }
}

bool Convert::to_vgm(const std::string& filename, const IVGMToolCallback& callback)
{
    // Make output filename filename.ext.vgm
    const auto outFilename = filename + ".vgm";

    callback.show_status(Utils::format("Converting \"%s\" to VGM format...", filename.c_str()));

    enum class file_type
    {
        gym,
        cym,
        ssl
    } fileType;

    const auto& extension = Utils::to_lower(std::filesystem::path(filename).extension().string());
    if (extension == ".gym")
    {
        fileType = file_type::gym;
    }
    else if (extension == ".cym")
    {
        fileType = file_type::cym;
    }
    else if (extension == ".ssl")
    {
        fileType = file_type::ssl;
    }
    else
    {
        throw std::runtime_error(Utils::format(R"(Unable to convert "%s" to VGM: unknown extension "%s")",
            filename.c_str(), extension.c_str()));
    }

    // Open files
    gzFile in = gzopen(filename.c_str(), "rb");
    gzFile out = gzopen(outFilename.c_str(), "wb0");

    gzseek(out, VGM_DATA_OFFSET, SEEK_SET);

    // Fill in VGM header
    VGMHeader vgmHeader;
    vgmHeader.RecordingRate = 60;
    vgmHeader.Version = 0x110;

    try
    {
        switch (fileType)
        {
        case file_type::gym:
            gymToVgm(filename, in, out, vgmHeader);
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
                while (!gzeof(in))
                {
                    switch (gzgetc(in))
                    {
                    case 0: // Wait 1/60s
                        gzputc(out, VGM_PAUSE_60TH);
                        vgmHeader.TotalLength += LEN60TH;
                        break;
                    case 3: // PSG
                        gzputc(out, VGM_PSG);
                        gzputc(out, gzgetc(in));
                        vgmHeader.PSGClock = 3579545;
                        vgmHeader.PSGShiftRegisterWidth = 16;
                        vgmHeader.PSGWhiteNoiseFeedback = 0x0009;
                        break;
                    case 4: // GG stereo
                        gzputc(out, VGM_GGST);
                        gzputc(out, gzgetc(in));
                        break;
                    case 5: // YM2413 address
                        ym2413Address = gzgetc(in);
                        break;
                    case 6: // YM2413 data
                        gzputc(out, VGM_YM2413);
                        gzputc(out, ym2413Address);
                        gzputc(out, gzgetc(in));
                        vgmHeader.YM2413Clock = 3579545;
                        break;
                    default: // Ignore unwanted bytes & EOF
                        break;
                    }
                }
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
        write_vgm_header(outFilename, vgmHeader, callback);

        // Do a final compression round
        Utils::compress(outFilename);

        // Report
        callback.show_conversion_progress(Utils::format(R"(Converted "%s" to "%s")", filename.c_str(),
            outFilename.c_str()));
    }
    catch (const std::exception& e)
    {
        gzclose(out);
        gzclose(in);
        std::filesystem::remove(outFilename.c_str());
        callback.show_error(Utils::format("Error converting \"%s\" to VGM: %s", filename.c_str(), e.what()));
        return false;
    }

    return true;
}
