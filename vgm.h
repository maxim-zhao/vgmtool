// VGM file format definitions
// and generic VGM-related functions

#ifndef VGM_H
#define VGM_H

#include <windows.h>
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

struct TVGMHeader {
  unsigned long VGMIdent;               // "Vgm "
  unsigned long EoFOffset;              // relative offset (from this point, 0x04) of the end of file
  unsigned long Version;                // 0x00000110 for 1.10
  unsigned long PSGClock;               // typically 3579545, 0 for no PSG
  unsigned long YM2413Clock;            // typically 3579545, 0 for no YM2413
  unsigned long GD3Offset;              // relative offset (from this point, 0x14) of the GD3 tag, 0 if not present
  unsigned long TotalLength;            // in samples
  unsigned long LoopOffset;             // relative again (to 0x1c), 0 if no loop
  unsigned long LoopLength;             // in samples, 0 if no loop
  unsigned long RecordingRate;          // in Hz, for speed-changing, 0 for no changing
  unsigned short PSGWhiteNoiseFeedback; // Feedback pattern for white noise generator; if <=v1.01, substitute default of 0x0009
  unsigned char PSGShiftRegisterWidth;  // Shift register width for noise channel; if <=v1.01, substitute default of 16
  unsigned char Reserved;
  unsigned long YM2612Clock;            // typically 3579545, 0 for no YM2612
  unsigned long YM2151Clock;            // typically 3579545, 0 for no YM2151
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
enum PSGTypes {
	PSGTone0,
	PSGTone1,
	PSGTone2,
	PSGNoise,
	PSGGGst,
	NumPSGTypes
};
enum YM2413Types {
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
enum YM2612Types {
	YM2612All,
	NumYM2612Types
};
enum YM2151Types {
	YM2151All,
	NumYM2151Types
};
enum ReservedTypes {
	ReservedAll,
	NumReservedTypes
};


//----------------------------------------------------------------------------------------------
// Structure defining the state of all available chips at any time, plus whether they're used
// and the currently elapsed time
//----------------------------------------------------------------------------------------------
struct TSystemState {
  long int samplecount;

  BOOL UsesPSG,UsesYM2413,UsesYM2612,UsesYM2151,UsesReserved;
  struct PSGState {
    int Registers[8];
    int LatchedRegister;
    int GGStereo;
  } PSGState;
  struct YM2413State {
    int Registers[YM2413NumRegs];
  } YM2413State;
  struct YM2612State {
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

void WritePause(gzFile out,long int pauselength);

void WriteVGMHeader(char *filename,struct TVGMHeader VGMHeader);

void GetUsedChips(gzFile in,BOOL *UsesPSG,BOOL *UsesYM2413,BOOL *UsesYM2612,BOOL *UsesYM2151,BOOL *UsesReserved);

void CheckLengths(char *filename,BOOL ShowResults);

int DetectRate(char *filename);

BOOL ReadVGMHeader(gzFile f,struct TVGMHeader *header,BOOL quiet);

void GetWriteCounts(char *filename,int PSGwrites[NumPSGTypes],int YM2413writes[NumYM2413Types],int YM2612writes[NumYM2612Types],int YM2151writes[NumYM2151Types],int reservedwrites[NumReservedTypes]);


void ResetState(struct TSystemState *State);

void WriteToState(struct TSystemState *state,int b0,int b1,int b2);

void WriteStateToFile(gzFile out,struct TSystemState *State,BOOL WriteKeys);

#endif