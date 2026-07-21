#include <cstdio>
#include "vgm.h"

#include <filesystem>

#include "IStatusCallback.h"
#include "utils.h"
#include "VgmFile.h"

#define BUFFER_SIZE 5*1024 // 5KB buffer size for mass copying

//----------------------------------------------------------------------------------------------
// Arrays defining what YM2413 bits are valid, and which are keys (or act similarly to keys)
//----------------------------------------------------------------------------------------------
const int YM2413ValidBits[YM2413NumRegs] = {
    //0    1    2    3    4    5    6    7    8    9    a    b    c    d    e    f
    //User inst------------------------------ Unused----------------------- Rhythm Test
    0xff, 0xff, 0xff, 0xdf, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0x00,
    //F-num low 8 bits---------------------------- Unused----------------------------
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    //F-num MSB/block/sus/key--------------------- Unused----------------------------
    0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    //Instrument/volume---------------------------
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};
const int YM2413KeyBits[YM2413NumRegs] = {
    //0    1    2    3    4    5    6    7    8    9    a    b    c    d    e    f
    //User inst------------------------------ Unused----------------------- Rhythm Test
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0x00,
    //F-num low 8 bits---------------------------- Unused----------------------------
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    //F-num MSB/block/sus/key--------------------- Unused----------------------------
    0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    //Instrument/volume---------------------------
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
const int YM2612ValidBits[YM2612NumRegs] = {
    // Note: I say unnecessary bits are invalid, eg. timers
    //0    1    2    3    4    5    6    7    8    9    a    b    c    d    e    f
    //Unused-------------------------------------------------------------------------
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x00x
    //Unused-------------------------------------------------------------------------
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x01x
    //Unused--- LFO  Unused         Timer B   Keys      DAC------ Unused-------------
    //                    Timer A--      Timers, 3/6 mode
    //                                             Unused
    0x00, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xf7, 0x00, 0xff, 0x80, 0x00, 0x00, 0x00, 0x00, // 0x02x
    //Detune/multiple      --------------      --------------      --------------
    0x7f, 0x7f, 0x7f, 0x00, 0x7f, 0x7f, 0x7f, 0x00, 0x7f, 0x7f, 0x7f, 0x00, 0x7f, 0x7f, 0x7f, 0x00, // 0x03x
    //Total level         --------------      --------------      --------------
    0x7f, 0x7f, 0x7f, 0x00, 0x7f, 0x7f, 0x7f, 0x00, 0x7f, 0x7f, 0x7f, 0x00, 0x7f, 0x7f, 0x7f, 0x00, // 0x04x
    // Rate scaling, attack rate--------      --------------      --------------
    0xdf, 0xdf, 0xdf, 0x00, 0xdf, 0xdf, 0xdf, 0x00, 0xdf, 0xdf, 0xdf, 0x00, 0xdf, 0xdf, 0xdf, 0x00, // 0x05x
    // First decay rate; amplitude modulation --------------      --------------
    0x9f, 0x9f, 0x9f, 0x00, 0x9f, 0x9f, 0x9f, 0x00, 0x9f, 0x9f, 0x9f, 0x00, 0x9f, 0x9f, 0x9f, 0x00, // 0x06x
    // Secondary decay rate-------------      --------------      --------------
    0x1f, 0x1f, 0x1f, 0x00, 0x1f, 0x1f, 0x1f, 0x00, 0x1f, 0x1f, 0x1f, 0x00, 0x1f, 0x1f, 0x1f, 0x00, // 0x07x
    // Secondary amplitude; release rate      --------------      --------------
    0xff, 0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0x00, // 0x08x
    // SSG-EG - do not use (?)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x09x
    // F-num LSB          Bl, F-num MSB       Ch3 special mode    --------------
    0xff, 0xff, 0xff, 0x00, 0x7f, 0x7f, 0x7f, 0x00, 0xff, 0xff, 0xff, 0x00, 0x7f, 0x7f, 0x7f, 0x00, // 0x0ax
    // Feedback, algorithm Stereo, LFO          Invalid!
    0x3f, 0x3f, 0x3f, 0x00, 0xef, 0xef, 0xef, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x0bx
    // Above 0xb7 is invalid
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x0cx
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x0dx
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x0ex
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x0fx
    // Part II (port 1): the same again, but only between 0x30 and 0xb7
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x10x
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x11x
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x12x
    //Detune/multiple      --------------      --------------      --------------
    0x7f, 0x7f, 0x7f, 0x00, 0x7f, 0x7f, 0x7f, 0x00, 0x7f, 0x7f, 0x7f, 0x00, 0x7f, 0x7f, 0x7f, 0x00, // 0x13x
    //Total level         --------------      --------------      --------------
    0x7f, 0x7f, 0x7f, 0x00, 0x7f, 0x7f, 0x7f, 0x00, 0x7f, 0x7f, 0x7f, 0x00, 0x7f, 0x7f, 0x7f, 0x00, // 0x14x
    // Rate scaling, attack rate--------      --------------      --------------
    0xdf, 0xdf, 0xdf, 0x00, 0xdf, 0xdf, 0xdf, 0x00, 0xdf, 0xdf, 0xdf, 0x00, 0xdf, 0xdf, 0xdf, 0x00, // 0x15x
    // First decay rate; amplitude modulation --------------      --------------
    0x9f, 0x9f, 0x9f, 0x00, 0x9f, 0x9f, 0x9f, 0x00, 0x9f, 0x9f, 0x9f, 0x00, 0x9f, 0x9f, 0x9f, 0x00, // 0x16x
    // Secondary decay rate-------------      --------------      --------------
    0x1f, 0x1f, 0x1f, 0x00, 0x1f, 0x1f, 0x1f, 0x00, 0x1f, 0x1f, 0x1f, 0x00, 0x1f, 0x1f, 0x1f, 0x00, // 0x17x
    // Secondary amplitude; release rate      --------------      --------------
    0xff, 0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0x00, // 0x18x
    // SSG-EG - do not use (?)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x19x
    // F-num LSB          Bl, F-num MSB       Ch3 special mode    --------------
    0xff, 0xff, 0xff, 0x00, 0x7f, 0x7f, 0x7f, 0x00, 0xff, 0xff, 0xff, 0x00, 0x7f, 0x7f, 0x7f, 0x00, // 0x1ax
    // Feedback, algorithm Stereo, LFO
    0x3f, 0x3f, 0x3f, 0x00, 0xef, 0xef, 0xef // 0x1bx
};

bool OldVGMHeader::is_valid() const
{
    return strncmp(VGMIdent, "Vgm ", 4) == 0;
}

//----------------------------------------------------------------------------------------------
// Writes a pause command to the file
// Modifies the length parameter to zero when done
//----------------------------------------------------------------------------------------------
void write_pause(gzFile out, long int pauselength)
{
    if (pauselength == 0)
    {
        return; // If zero do nothing
    }
    while (pauselength > 0xffff)
    {
        // If over 0xffff, write 0xffffs
        gzputc(out, VGM_PAUSE_WORD);
        gzputc(out, 0xff);
        gzputc(out, 0xff);
        pauselength -= 0xffff;
    }
    switch (pauselength)
    {
    case (LEN60TH * 2):
        gzputc(out, VGM_PAUSE_60TH);
    // fall through
    case LEN60TH:
        gzputc(out, VGM_PAUSE_60TH);
        break;
    case (LEN50TH * 2):
        gzputc(out, VGM_PAUSE_50TH);
    // fall through
    case LEN50TH:
        gzputc(out, VGM_PAUSE_50TH);
        break;
    default:
        if (pauselength <= 16)
        {
            gzputc(out, 0x70 + pauselength - 1); // 1-byte pause 1..16
            //      } else if(pauselength<256) {
            //        gzputc(out, VGM_PAUSE_BYTE);         // 2-byte pause 1..255
            //        gzputc(out, pauselength);
        }
        else
        {
            gzputc(out, VGM_PAUSE_WORD);
            gzputc(out, (pauselength & 0xff)); // 3-byte pause 1..65535
            gzputc(out, (pauselength >> 8));
        }
        break;
    }
}

//----------------------------------------------------------------------------------------------
// Writes the header to the file
// Assumes you are writing the original header with minor modifications, so it
// doesn't check anything (like the GD3 offset, EOF offset).
//----------------------------------------------------------------------------------------------
void write_vgm_header(const std::string& filename, OldVGMHeader VGMHeader, const IStatusCallback& callback)
{
    char copybuffer[BUFFER_SIZE];
    int AmtRead;

    if (!Utils::file_exists(filename))
    {
        return;
    }

    callback.verbose_message("Updating VGM header...");

    const auto outfilename = Utils::make_temp_filename(filename);

    gzFile in = gzopen(filename.c_str(), "rb");
    gzFile out = gzopen(outfilename.c_str(), "wb0");

    gzwrite(out, &VGMHeader, sizeof(VGMHeader));
    gzseek(in, sizeof(VGMHeader), SEEK_SET);

    do
    {
        AmtRead = gzread(in, copybuffer, BUFFER_SIZE);
        if (gzwrite(out, copybuffer, AmtRead) != AmtRead)
        {
            // Error copying file
            callback.error(std::format("Error copying data to temporary file {}!", outfilename));
            gzclose(in);
            gzclose(out);
            std::filesystem::remove(outfilename);
            return;
        }
    }
    while (AmtRead > 0);

    gzclose(in);
    gzclose(out);

    Utils::replace_file(filename, outfilename);

    callback.verbose_message("VGM header update complete");
}


//----------------------------------------------------------------------------------------------
// Counts samples in file
// Corrects header if necessary
// TODO: rewrite as a CheckHeader() function to check everything at once
//----------------------------------------------------------------------------------------------
void check_lengths(const std::string& filename, bool showResults, const IStatusCallback& callback)
{
    if (!Utils::file_exists(filename))
    {
        return;
    }

    callback.verbose_message("Counting samples...");

    gzFile in = gzopen(filename.c_str(), "rb");

    // Read header
    OldVGMHeader vgmHeader;
    gzread(in, &vgmHeader, sizeof(vgmHeader));

    if (!vgmHeader.is_valid())
    {
        // no VGM marker
        callback.error("File is not a VGM file! (no \"Vgm \")");
        gzclose(in);
        return;
    }

    int sampleCount = 0;
    int loopSampleCount = 0;

    for (auto atEnd = false; !atEnd;)
    {
        if (gztell(in) == static_cast<int>(vgmHeader.LoopOffset) + LOOPDELTA)
        {
            loopSampleCount = sampleCount;
        }
        switch (gzgetc(in))
        {
        case VGM_GGST: // GG stereo (1 byte data)
        case VGM_PSG: // PSG write (1 byte data)
            gzgetc(in);
            break;
        case VGM_YM2413: // YM2413
        case VGM_YM2612_0: // YM2612 port 0
        case VGM_YM2612_1: // YM2612 port 1
        case VGM_YM2151: // YM2151
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
            gzgetc(in);
            gzgetc(in);
            break;
        case VGM_PAUSE_WORD: // Wait n samples
            {
                const auto b1 = gzgetc(in);
                const auto b2 = gzgetc(in);
                sampleCount += Utils::make_word(b1, b2);
                break;
            }
        case VGM_PAUSE_60TH: // Wait 1/60 s
            sampleCount += LEN60TH;
            break;
        case VGM_PAUSE_50TH: // Wait 1/50 s
            sampleCount += LEN50TH;
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
            sampleCount += ((gzgetc(in)) & 0xf) + 1;
            break;
        case VGM_END: // End of sound data... report
            if (loopSampleCount == -1)
            {
                loopSampleCount = sampleCount;
            }
            loopSampleCount = sampleCount - loopSampleCount;
            // Change its meaning! Now it's the number of samples in the loop

            if (showResults)
            {
                callback.message(std::format(
                    "Lengths:\n"
                    "In file:\n"
                    "Total: {} samples = {:.2f} seconds\n"
                    "Loop: {} samples = {:.2f} seconds\n"
                    "In header:\n"
                    "Total: {} samples = {:.2f} seconds\n"
                    "Loop: {} samples = {:.2f} seconds",
                    sampleCount,
                    sampleCount / 44100.0,
                    loopSampleCount,
                    loopSampleCount / 44100.0,
                    vgmHeader.TotalLength,
                    vgmHeader.TotalLength / 44100.0,
                    vgmHeader.LoopLength,
                    vgmHeader.LoopLength / 44100.0
                ));
            }
            gzclose(in);
            if (
                (sampleCount != static_cast<int>(vgmHeader.TotalLength)) ||
                (loopSampleCount != static_cast<int>(vgmHeader.LoopLength))
            )
            {
                // Need to repair header
                callback.verbose_message("Correcting header...");
                vgmHeader.TotalLength = sampleCount;
                vgmHeader.LoopLength = loopSampleCount;
                if (loopSampleCount == 0)
                {
                    vgmHeader.LoopOffset = 0;
                }
                write_vgm_header(filename, vgmHeader, callback);
            }
            atEnd = true;
            break;
        }
    }
}


//----------------------------------------------------------------------------------------------
// Go through file, if I find a 1/50th or a 1/60th then I'll assume that's correct
//----------------------------------------------------------------------------------------------
int detect_rate(const VgmFile& /*file*/)
{
    /* TODO 
    const auto& data = file.data();
    for (size_t i = 0; i < data.size(); ++i)
    {
        switch (data[i])
        {
        case VGM_GGST: // GG stereo
        case VGM_PSG: // PSG write
            ++i;
            break;
        case VGM_YM2413: // YM2413
        case VGM_YM2612_0: // YM2612 port 0
        case VGM_YM2612_1: // YM2612 port 1
        case VGM_YM2151: // YM2151
        case 0x55:
        case 0x56:
        case 0x57:
        case 0x58:
        case 0x59:
        case 0x5a:
        case 0x5b:
        case 0x5c:
        case 0x5d:
        case 0x5e:
        case 0x5f: // Reserved up to 0x5f
        case VGM_PAUSE_WORD:
            // Check if it's a multiple of 1/50 or 1/60s (but not both)
            {
                const auto low = data[++i];
                const auto high = data[++i];
                const auto duration = Utils::make_word(low, high);
                if (duration % LEN50TH == 0 && duration % LEN60TH != 0)
                {
                    return 50;
                }
                if (duration % LEN50TH != 0 && duration % LEN60TH == 0)
                {
                    return 60;
                }
            }
            break;
        case VGM_PAUSE_60TH: // Wait 1/60 s
            return 60;
        case VGM_PAUSE_50TH: // Wait 1/50 s
            return 50;
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
        case VGM_END: // End of sound data
        default:
            break;
        }
    }
    */
    return 0;
}

// Reads in header from file
// Shows an error if it's not a VGM file
// returns success/failure
bool ReadVGMHeader(gzFile f, OldVGMHeader* header, const IStatusCallback& callback)
{
    gzread(f, header, sizeof(OldVGMHeader));
    if (!header->is_valid())
    {
        // no VGM marker
        callback.error("File is not a VGM file! (no \"Vgm \")");
        return false;
    }
    return true;
}

// Count how many writes there are to each chip/channel, 
// expecting that this and the chip/channel stripper
// ought to be equally capable
void GetWriteCounts(const std::string& filename, std::vector<int>& PSGwrites, std::vector<int>& YM2413writes,
                    std::vector<int>& YM2612writes, std::vector<int>& YM2151writes,
                    std::vector<int>& reservedwrites, const IStatusCallback& callback)
{
    int b0, b1, b2;
    int i;
    int Channel = 0; // for tracking PSG latched register

    // Initialise all to zero
    PSGwrites.clear();
    PSGwrites.resize(NumPSGTypes, 0);
    YM2413writes.clear();
    YM2413writes.resize(NumYM2413Types, 0);
    YM2612writes.clear();
    YM2612writes.resize(NumYM2612Types, 0);
    YM2151writes.clear();
    YM2151writes.resize(NumYM2151Types, 0);
    reservedwrites.clear();
    reservedwrites.resize(NumReservedTypes, 0);

    if (!Utils::file_exists(filename))
    {
        return;
    }
    // explicitly check for filename==NULL because that signals that we're supposed to return all zero

    gzFile in = gzopen(filename.c_str(), "rb");

    // Read header
    OldVGMHeader VGMHeader;
    if (!ReadVGMHeader(in, &VGMHeader, callback))
    {
        gzclose(in);
        return;
    }

    callback.verbose_message("Scanning for chip data...");

    gzseek(in, VGM_DATA_OFFSET, SEEK_SET);

    do
    {
        b0 = gzgetc(in);
        switch (b0)
        {
        case VGM_GGST: // GG stereo
            gzseek(in, 1, SEEK_CUR);
            ++PSGwrites[PSGGGst];
            break;
        case VGM_PSG: // PSG write (1 byte data)
            b1 = gzgetc(in);
            if (b1 & 0x80)
            {
                Channel = ((b1 >> 5) & 0x03); // Latch/data byte   %1 cc t dddd
            }
            ++PSGwrites[PSGTone0 + Channel];
            break;
        case VGM_YM2413: // YM2413
            b1 = gzgetc(in);
            b2 = gzgetc(in);
            switch (b1 >> 4)
            {
            // go by 1st digit first
            case 0x0: // User tone / percussion
                switch (b1)
                {
                case 0x00:
                case 0x01:
                case 0x02:
                case 0x03:
                case 0x04:
                case 0x05:
                case 0x06:
                case 0x07:
                    ++YM2413writes[YM2413UserInst];
                    break;
                case 0x0E: // Percussion
                    for (i = 0; i < 5; ++i)
                    {
                        if ((b2 >> i) & 1)
                        {
                            ++YM2413writes[YM2413PercHH + i];
                        }
                    }
                    break;
                default:
                    ++YM2413writes[YM2413Invalid]; // invalid reg
                    break;
                }
                break;
            case 0x1: // Tone F-number low 8 bits
                if (b1 > 0x18)
                {
                    ++YM2413writes[YM2413Invalid]; // invalid reg
                }
                else
                {
                    ++YM2413writes[YM2413Tone1 + b1 & 0xf];
                }
                break;
            case 0x2: // Tone more stuff including key
                if (b1 > 0x28)
                {
                    ++YM2413writes[YM2413Invalid]; // invalid reg
                }
                else
                {
                    ++YM2413writes[YM2413Tone1 - b1 & 0xf];
                }
                break;
            case 0x3: // Tone instruments and volume
                if (b1 >= YM2413NumRegs)
                {
                    ++YM2413writes[YM2413Invalid]; // invalid reg
                }
                else
                {
                    ++YM2413writes[YM2413Tone1 + b1 & 0xf];
                }
                break;
            default:
                ++YM2413writes[YM2413Invalid]; // invalid reg
                break;
            }
            break;
        case VGM_YM2612_0: // YM2612 port 0
        case VGM_YM2612_1: // YM2612 port 1
            ++YM2612writes[YM2612All];
            gzseek(in, 2, SEEK_CUR);
            break;
        case VGM_YM2151: // YM2151
            ++YM2151writes[YM2151All];
            gzseek(in, 2, SEEK_CUR);
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
            ++reservedwrites[ReservedAll];
            gzseek(in, 2, SEEK_CUR);
            break;
        case VGM_PAUSE_WORD: // Wait n samples
            gzseek(in, 2, SEEK_CUR);
            break;
        case VGM_PAUSE_60TH: // Wait 1/60 s
        case VGM_PAUSE_50TH: // Wait 1/50 s
            break;
        //    case VGM_PAUSE_BYTE:  // Wait n samples
        //      gzseek(in, 1, SEEK_CUR);
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
            // no data to skip
            break;
        case VGM_END: // End of sound data
            b0 = EOF; // make it break out
            break;
        default:
            ++reservedwrites[ReservedAll];
            break;
        }
    }
    while (b0 != EOF);

    gzclose(in);

    callback.verbose_message("Scan for chip data complete");
}