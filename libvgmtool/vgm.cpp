#include <cstdio>
#include "vgm.h"

#include <filesystem>

#include "IVGMToolCallback.h"
#include "utils.h"

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
    //Detune/mutiple      --------------      --------------      --------------
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
    //Detune/mutiple      --------------      --------------      --------------
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

bool VGMHeader::is_valid() const
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
void write_vgm_header(const char* filename, VGMHeader VGMHeader, const IVGMToolCallback& callback)
{
    char copybuffer[BUFFER_SIZE];
    int AmtRead;

    if (!Utils::file_exists(filename))
    {
        return;
    }

    callback.show_status("Updating VGM header...");

    const auto outfilename = Utils::make_temp_filename(filename);

    gzFile in = gzopen(filename, "rb");
    gzFile out = gzopen(outfilename.c_str(), "wb0");

    gzwrite(out, &VGMHeader, sizeof(VGMHeader));
    gzseek(in, sizeof(VGMHeader), SEEK_SET);

    do
    {
        AmtRead = gzread(in, copybuffer, BUFFER_SIZE);
        if (gzwrite(out, copybuffer, AmtRead) != AmtRead)
        {
            // Error copying file
            callback.show_error(Utils::format("Error copying data to temporary file %s!", outfilename.c_str()));
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

    callback.show_status("VGM header update complete");
}

//----------------------------------------------------------------------------------------------
// Parse *in for chip data, setting BOOLs accordingly
// *in is an open gzFile
//----------------------------------------------------------------------------------------------
void get_used_chips(gzFile in, bool* UsesPSG, bool* UsesYM2413, bool* UsesYM2612, bool* UsesYM2151, bool* UsesReserved)
{
    if (UsesPSG)
    {
        *UsesPSG = false;
    }
    if (UsesYM2413)
    {
        *UsesYM2413 = false;
    }
    if (UsesYM2612)
    {
        *UsesYM2612 = false;
    }
    if (UsesYM2151)
    {
        *UsesYM2151 = false;
    }
    if (UsesReserved)
    {
        *UsesReserved = false;
    }

    gzseek(in, VGM_DATA_OFFSET, SEEK_SET);
    for (bool atEnd = false; !atEnd;)
    {
        auto b0 = gzgetc(in);
        switch (b0)
        {
        case VGM_GGST:
        case VGM_PSG:
            gzgetc(in);
            if (UsesPSG)
            {
                *UsesPSG = true;
            }
            break;
        case VGM_YM2413:
            gzgetc(in);
            gzgetc(in);
            if (UsesYM2413)
            {
                *UsesYM2413 = true;
            }
            break;
        case VGM_YM2612_0:
        case VGM_YM2612_1:
            gzgetc(in);
            gzgetc(in);
            if (UsesYM2612)
            {
                *UsesYM2612 = true;
            }
            break;
        case VGM_YM2151:
            gzgetc(in);
            gzgetc(in);
            if (UsesYM2151)
            {
                *UsesYM2151 = true;
            }
            break;
        case 0x55: // Reserved up to 0x5f
        case 0x56:
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
            if (UsesReserved)
            {
                *UsesReserved = true;
            }
            break;
        case VGM_PAUSE_WORD:
            gzgetc(in);
            gzgetc(in);
            break;
        case VGM_PAUSE_60TH:
        case VGM_PAUSE_50TH:
            break;
        //    case VGM_PAUSE_BYTE:
        //      gzgetc(in);
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
            break;
        case VGM_END:
            atEnd = true;
            break;
        }
    }
}


//----------------------------------------------------------------------------------------------
// Counts samples in file
// Corrects header if necessary
// TODO: rewrite as a CheckHeader() function to check everything at once
//----------------------------------------------------------------------------------------------
void check_lengths(const std::string& filename, bool showResults, const IVGMToolCallback& callback)
{
    if (!Utils::file_exists(filename))
    {
        return;
    }

    callback.show_status("Counting samples...");

    gzFile in = gzopen(filename.c_str(), "rb");

    // Read header
    VGMHeader vgmHeader;
    gzread(in, &vgmHeader, sizeof(vgmHeader));

    if (!vgmHeader.is_valid())
    {
        // no VGM marker
        callback.show_error("File is not a VGM file! (no \"Vgm \")");
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
                callback.show_message(Utils::format(
                    "Lengths:\n"
                    "In file:\n"
                    "Total: %d samples = %.2f seconds\n"
                    "Loop: %d samples = %.2f seconds\n"
                    "In header:\n"
                    "Total: %d samples = %.2f seconds\n"
                    "Loop: %d samples = %.2f seconds",
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
                callback.show_status("Correcting header...");
                vgmHeader.TotalLength = sampleCount;
                vgmHeader.LoopLength = loopSampleCount;
                if (loopSampleCount == 0)
                {
                    vgmHeader.LoopOffset = 0;
                }
                write_vgm_header(filename.c_str(), vgmHeader, callback);
            }
            atEnd = true;
            break;
        }
    }
}


//----------------------------------------------------------------------------------------------
// Go through file, if I find a 1/50th or a 1/60th then I'll assume that's correct
//----------------------------------------------------------------------------------------------
int detect_rate(char* filename, const IVGMToolCallback& callback)
{
    VGMHeader VGMHeader;
    int b0, b1, b2;

    if (!Utils::file_exists(filename))
    {
        return 0;
    }

    callback.show_status("Detecting VGM recording rate...");

    gzFile in = gzopen(filename, "rb");

    // Read header
    if (!ReadVGMHeader(in, &VGMHeader, callback))
    {
        callback.show_status("");
        gzclose(in);
        return 0;
    }

    gzseek(in, VGM_DATA_OFFSET, SEEK_SET);

    int Speed = 0;
    do
    {
        b0 = gzgetc(in);
        switch (b0)
        {
        case VGM_GGST: // GG stereo
        case VGM_PSG: // PSG write
            gzseek(in, 1, SEEK_CUR);
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
            break;
        case VGM_PAUSE_WORD:
            // check if it's a 1/60 or 1/50 interval
            b1 = gzgetc(in);
            b2 = gzgetc(in);
            switch (b1 | (b2 << 8))
            {
            case LEN60TH:
                Speed = 60;
                b0 = EOF;
                break;
            case LEN50TH:
                Speed = 50;
                b0 = EOF;
                break;
            }
            break;
        case VGM_PAUSE_60TH: // Wait 1/60 s
            Speed = 60;
            b0 = EOF;
            break;
        case VGM_PAUSE_50TH: // Wait 1/50 s
            Speed = 50;
            b0 = EOF;
            break;
        //    case VGM_PAUSE_BYTE:
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
            break;
        case VGM_END: // End of sound data
            b0 = EOF; // make it break out
            break;
        default:
            break;
        }
    }
    while (b0 != EOF);

    gzclose(in);

    if (Speed)
    {
        callback.show_status(Utils::format("VGM rate detected as %dHz", Speed));
    }
    else
    {
        callback.show_status("VGM rate not detected");
    }

    return Speed;
}

// Reads in header from file
// Shows an error if it's not a VGM file
// returns success/failure
bool ReadVGMHeader(gzFile f, VGMHeader* header, const IVGMToolCallback& callback)
{
    gzread(f, header, sizeof(VGMHeader));
    if (!header->is_valid())
    {
        // no VGM marker
        callback.show_error("File is not a VGM file! (no \"Vgm \")");
        return false;
    }
    return true;
}

// Count how many writes there are to each chip/channel, 
// expecting that this and the chip/channel stripper
// ought to be equally capable
void GetWriteCounts(char* filename, unsigned long PSGwrites[NumPSGTypes], unsigned long YM2413writes[NumYM2413Types],
                    unsigned long YM2612writes[NumYM2612Types], unsigned long YM2151writes[NumYM2151Types],
                    unsigned long reservedwrites[NumReservedTypes], const IVGMToolCallback& callback)
{
    VGMHeader VGMHeader;
    int b0, b1, b2;
    int i;
    int Channel = 0; // for tracking PSG latched register

    // Initialise all to zero
    memset(PSGwrites, 0, sizeof(int) * NumPSGTypes);
    memset(YM2413writes, 0, sizeof(int) * NumYM2413Types);
    memset(YM2612writes, 0, sizeof(int) * NumYM2612Types);
    memset(YM2151writes, 0, sizeof(int) * NumYM2151Types);
    memset(reservedwrites, 0, sizeof(int) * NumReservedTypes);

    if (!filename || !Utils::file_exists(filename))
    {
        return;
    }
    // explicitly check for filename==NULL because that signals that we're supposed to return all zero

    gzFile in = gzopen(filename, "rb");

    // Read header
    if (!ReadVGMHeader(in, &VGMHeader, callback))
    {
        gzclose(in);
        return;
    }

    callback.show_status("Scanning for chip data...");

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

    callback.show_status("Scan for chip data complete");
}


//----------------------------------------------------------------------------------------------
// Fills a TSystemState with default values
//----------------------------------------------------------------------------------------------
void ResetState(TSystemState* State)
{
    State->samplecount = 0;

    State->PSGState.GGStereo = 0xff;
    for (int i = 0; i < 4; i++)
    {
        State->PSGState.Registers[2 * i] = 0;
        State->PSGState.Registers[2 * i + 1] = 0xf;
    }
    State->PSGState.LatchedRegister = 0;

    memset(State->YM2413State.Registers, 0, YM2413NumRegs);

    memset(State->YM2612State.Registers, 0, YM2612NumRegs);

    // TODO: YM2151
}

// handles a chip write by updating the state in memory
// also keeps track of time passed
// unused parameters are zero where necessary
void WriteToState(TSystemState* state, int b0, int b1, int b2)
{
    int i;
    switch (b0)
    {
    case VGM_GGST: // GG stereo
        state->PSGState.GGStereo = b1;
        break;
    case VGM_PSG: // PSG write
        if (b1 & 0x80)
        {
            // latch byte
            state->PSGState.LatchedRegister = i = ((b1 >> 4) & 0x07); // 1 cc t dddd - get cct as register number
            state->PSGState.Registers[i] = (state->PSGState.Registers[i] & 0x3f0) | (b1 & 0xf);
            // update low 4 bits of register
        }
        else
        {
            // data byte
            i = state->PSGState.LatchedRegister;
            if ((i > -1) && (i < 8))
            {
                if (!(i % 2) && (i < 5))
                {
                    // if latched register is a tone channel
                    state->PSGState.Registers[i] = (state->PSGState.Registers[i] & 0x00f) | ((b1 & 0x3f) << 4);
                    // update the high 6 bits
                }
                else
                {
                    // otherwise it's noise/vol
                    state->PSGState.Registers[i] = b1 & 0x0f; // so update the low 4 bits
                }
            }
        }
        break;
    case VGM_YM2413: // YM2413
        if (b1 < YM2413NumRegs)
        {
            state->YM2413State.Registers[b1] = b2;
        }
        break;
    case VGM_YM2612_0: // YM2612 port 0
    case VGM_YM2612_1: // YM2612 port 1
        i = (b0 == VGM_YM2612_0 ? 0 : 0x100) + b1;
        if (i < YM2612NumRegs)
        {
            state->YM2612State.Registers[i] = b2;
        }
        break;
    case VGM_YM2151: // YM2151
        // TODO: this
        break;

    // I include these to acknowledge that I am deliberately not handling them here (I don't need to)
    // all do nothing
    case 0x55: // Reserved
    case 0x56:
    case 0x57:
    case 0x58:
    case 0x59:
    case 0x5a:
    case 0x5b:
    case 0x5c:
    case 0x5d:
    case 0x5e:
    case 0x5f:
        break;

    case VGM_PAUSE_WORD: // Wait n samples
        state->samplecount += Utils::make_word(b1, b2);
        break;
    //  case VGM_PAUSE_BYTE:  // Wait n samples
    //    state->samplecount+=b1;
    //    break;
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
        state->samplecount += (b0 & 0xf) + 1;
        break;
    case VGM_PAUSE_60TH: // Wait 1/60 s
        state->samplecount += LEN60TH;
        break;
    case VGM_PAUSE_50TH: // Wait 1/50 s
        state->samplecount += LEN50TH;
        break;
    case VGM_END: // End of sound data
        break;
    default:
        break;
    } // end switch
}

//----------------------------------------------------------------------------------------------
// Writes a TSystemState to *out, with key data if WriteKeys
//----------------------------------------------------------------------------------------------
void WriteStateToFile(gzFile out, TSystemState* State, bool WriteKeys)
{
    // Write a full system state
    int i;

    // First PSG
    if (State->UsesPSG)
    {
        // Write in groups, not register order
        // 1. Tone channel frequencies (regs 0, 2, 4)
        for (i = 0; i < 5; i += 2)
        {
            gzputc(out, VGM_PSG);
            gzputc(out, // 1st byte
                0x80 | // Marker  %1-------
                (i << 4) | // Channel %-cct----
                State->PSGState.Registers[i] & 0x0f // Data    %----dddd
            );
            gzputc(out, VGM_PSG);
            gzputc(out, // 2nd byte
                (State->PSGState.Registers[i] >> 4) & 0x3f // Data    %0-dddddd
            );
        }
        // 2. Noise
        gzputc(out, VGM_PSG);
        gzputc(out,
            0xe0 | // %1110----
            (State->PSGState.Registers[6] & 0x07) // %----dddd
        );
        // 3. Volumes (regs 1, 3, 5, 7)
        for (i = 1; i < 8; i += 2)
        {
            gzputc(out, VGM_PSG);
            gzputc(out,
                0x80 | // Marker  %1-------
                (i << 4) | // Channel %-cct----
                (State->PSGState.Registers[i] & 0x0f) // Data    %----dddd
            );
        }
        // 4. GG stereo
        gzputc(out, VGM_GGST);
        gzputc(out, State->PSGState.GGStereo);
    }

    // Then YM2413
    if (State->UsesYM2413)
    {
        for (i = 0; i < YM2413NumRegs; ++i)
        {
            if (YM2413ValidBits[i])
            {
                gzputc(out, VGM_YM2413);
                gzputc(out, i);
                gzputc(out,
                    State->YM2413State.Registers[i] & YM2413ValidBits[i] & (WriteKeys ? 0xff : (~YM2413KeyBits[i])));
                // Output, but zero key bits if it's not the starting point
            }
        }
    }

    // Then YM2612
    if (State->UsesYM2612)
    {
        for (i = 0; i < YM2612NumRegs; ++i)
        {
            if (YM2612ValidBits[i])
            {
                if (i < 0x100)
                {
                    gzputc(out, VGM_YM2612_0);
                }
                else
                {
                    gzputc(out, VGM_YM2612_1);
                }
                gzputc(out, (i & 0xff));
                // key bit handling
                if (
                    ((i & 0xff) != 0x28) // if it's not the keys register
                    || WriteKeys // OR, we want keys
                )
                {
                    gzputc(out, State->YM2612State.Registers[i] & YM2612ValidBits[i]);
                }
            }
        }
    }

    // TODO: YM2151 states
}
