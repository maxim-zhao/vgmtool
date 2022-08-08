#pragma once

// VGM file format definitions
// and generic VGM-related functions

#include <string>
#include <zlib.h>

// VGM data bytes
#define VGM_GGST        0x4f
#define VGM_PSG         0x50
#define VGM_YM2413      0x51
#define VGM_YM2612_0    0x52
#define VGM_YM2612_1    0x53
#define VGM_YM2151      0x54
#define VGM_PAUSE_WORD  0x61
#define VGM_PAUSE_60TH  0x62
#define VGM_PAUSE_50TH  0x63
#define VGM_END         0x66
// Also 0x7n = pause n+1

#define VGMIDENT 0x206d6756 // "Vgm "

class IVGMToolCallback;

struct VGMHeader
{
    char VGMIdent[4]{'V', 'g', 'm', ' '}; // "Vgm "
    uint32_t EoFOffset{}; // relative offset (from this point, 0x04) of the end of file
    uint32_t Version{0x0110}; // 0x00000110 for 1.10
    uint32_t PSGClock{}; // typically 3579545, 0 for no PSG
    uint32_t YM2413Clock{}; // typically 3579545, 0 for no YM2413
    uint32_t GD3Offset{}; // relative offset (from this point, 0x14) of the GD3 tag, 0 if not present
    uint32_t TotalLength{}; // in samples
    uint32_t LoopOffset{}; // relative again (to 0x1c), 0 if no loop
    uint32_t LoopLength{}; // in samples, 0 if no loop
    uint32_t RecordingRate{}; // in Hz, for speed-changing, 0 for no changing
    uint16_t PSGWhiteNoiseFeedback{0x0009}; // Feedback pattern for white noise generator; if <=v1.01, substitute default of 0x0009
    uint8_t PSGShiftRegisterWidth{16}; // Shift register width for noise channel; if <=v1.01, substitute default of 16
    uint8_t Reserved{};
    uint32_t YM2612Clock{}; // typically 3579545, 0 for no YM2612
    uint32_t YM2151Clock{}; // typically 3579545, 0 for no YM2151

public:
    bool is_valid() const;

    VGMHeader() = default;
};

#define EOFDELTA  0x04
#define GD3DELTA  0x14
#define LOOPDELTA 0x1c

#define VGM_DATA_OFFSET 0x40


// Size of registers array for YM2413 - uses 0x00-0x38, with some gaps
#define YM2413NumRegs 0x39

// Size of registers array for YM2612 - uses 0x21-0xb7, with some gaps, twice (use high 1 for channel 1, high 0 for channel 0)
#define YM2612NumRegs 0x1b7

// Frequency value below which the PSG is silent
#define PSGCutoff 6

// Lengths of pauses in samples
#define LEN60TH 735
#define LEN50TH 882

// Last one is the count because the values are 0-based
// Most are order-dependent for integer-based references, so don't mess with them
enum PSGTypes
{
    PSGTone0, 
    PSGTone1, 
    PSGTone2, 
    PSGNoise, 
    PSGGGst, 
    NumPSGTypes
};

enum YM2413Types
{
    YM2413Tone1, 
    YM2413Tone2, 
    YM2413Tone3, 
    YM2413Tone4, 
    YM2413Tone5, 
    YM2413Tone6, 
    YM2413Tone7, 
    YM2413Tone8, 
    YM2413Tone9, 
    YM2413PercHH, 
    YM2413PercCym, 
    YM2413PercTT, 
    YM2413PercSD, 
    YM2413PercBD, 
    YM2413UserInst, 
    YM2413Invalid, 
    NumYM2413Types
};

enum YM2612Types
{
    YM2612All, 
    NumYM2612Types
};

enum YM2151Types
{
    YM2151All, 
    NumYM2151Types
};

enum ReservedTypes
{
    ReservedAll, 
    NumReservedTypes
};


//----------------------------------------------------------------------------------------------
// Structure defining the state of all available chips at any time, plus whether they're used
// and the currently elapsed time
//----------------------------------------------------------------------------------------------
struct TSystemState
{
    long int samplecount;

    bool UsesPSG, UsesYM2413, UsesYM2612, UsesYM2151, UsesReserved;

    struct PSGState
    {
        int Registers[8];
        int LatchedRegister;
        int GGStereo;
    } PSGState;

    struct YM2413State
    {
        int Registers[YM2413NumRegs];
    } YM2413State;

    struct YM2612State
    {
        int Registers[YM2612NumRegs];
    } YM2612State;

    // TODO: YM2151
};

// Valid/key bitmask arrays

extern const int YM2413ValidBits[YM2413NumRegs];
extern const int YM2413KeyBits[YM2413NumRegs];
extern const int YM2612ValidBits[YM2612NumRegs];

// TODO: YM2151 equivalents


// functions

void write_pause(gzFile out, long int pauselength);

void write_vgm_header(const std::string& filename, VGMHeader VGMHeader, const IVGMToolCallback& callback);

void get_used_chips(gzFile in, bool* UsesPSG, bool* UsesYM2413, bool* UsesYM2612, bool* UsesYM2151, bool* UsesReserved);

void check_lengths(const std::string& filename, bool showResults, const IVGMToolCallback& callback);

int detect_rate(const std::string& filename, const IVGMToolCallback& callback);

bool ReadVGMHeader(gzFile f, VGMHeader* header, const IVGMToolCallback& callback);

void GetWriteCounts(const std::string& filename, unsigned long PSGwrites[NumPSGTypes], unsigned long YM2413writes[NumYM2413Types],
                    unsigned long YM2612writes[NumYM2612Types], unsigned long YM2151writes[NumYM2151Types],
                    unsigned long reservedwrites[NumReservedTypes], const IVGMToolCallback& callback);


void ResetState(TSystemState* State);

void WriteToState(TSystemState* state, int b0, int b1, int b2);

void WriteStateToFile(gzFile out, TSystemState* State, bool WriteKeys);
