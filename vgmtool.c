// Dialogue-based UI
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <commctrl.h>
#include <zlib.h>

#include "resource.h"

#include "vgm.h"
#include "gd3.h"
#include "writetotext.h"
#include "utils.h"
#include "gui.h"
#include "optimise.h"
#include "trim.h"
#include "convert.h"


// GD3 versions I can accept
#define MINGD3VERSION 0x100
#define REQUIREDGD3MAJORVER 0x100

typedef unsigned short wchar;  // For Unicode strings

HWND hWndMain=NULL;
HINSTANCE HInst;

char Currentfilename[10240]="You didn't load a file yet!";
struct TVGMHeader CurrentFileVGMHeader;

const char* ProgName="VGMTool 2 release 5";

const int GD3EditControls[NumGD3Strings]={edtGD3TitleEn,edtGD3TitleJp,edtGD3GameEn,edtGD3GameJp,cbGD3SystemEn,edtGD3SystemJp,edtGD3AuthorEn,edtGD3AuthorJp,edtGD3Date,edtGD3Creator,edtGD3Notes};

#define NumPSGTypes 5
#define NumYM2413Types 16
#define NumYM2612Types 1
#define NumYM2151Types 1
#define NumReservedTypes 1

unsigned long int PSGWrites     [NumPSGTypes]     ={0};  // 0-2 = tone, 3 = noise, 4 = GG st
unsigned long int YM2413Writes  [NumYM2413Types]  ={0};  // 0-8 = tone, 9-13 = perc, 14 = user inst, 15 = invalid reg
unsigned long int YM2612Writes  [NumYM2612Types]  ={0};  // Unsupported
unsigned long int YM2151Writes  [NumYM2151Types]  ={0};  // Unsupported
unsigned long int ReservedWrites[NumReservedTypes]={0};  // reserved chips

const int  PSGCheckBoxes     [NumPSGTypes]     ={cbPSG0,cbPSG1,cbPSG2,cbPSGNoise,cbPSGGGSt};
const int  YM2413CheckBoxes  [NumYM2413Types]  ={cbYM24130,cbYM24131,cbYM24132,cbYM24133,cbYM24134,cbYM24135,cbYM24136,cbYM24137,cbYM24138,cbYM2413HiHat,cbYM2413Cymbal,cbYM2413TomTom,cbYM2413SnareDrum,cbYM2413BassDrum,cbYM2413UserInst,cbYM2413InvalidRegs};
const int  YM2612CheckBoxes  [NumYM2612Types]  ={cbYM2612};
const int  YM2151CheckBoxes  [NumYM2151Types]  ={cbYM2151};
const int  ReservedCheckboxes[NumReservedTypes]={cbReserved};


wchar *GD3Strings=NULL;

#define NumTabChildWnds 6
HWND TabChildWnds[NumTabChildWnds];  // Holds child windows' HWnds
// Defines to make it easier to place stuff where I want
#define HeaderWnd   TabChildWnds[0]
#define TrimWnd     TabChildWnds[1]
#define StripWnd    TabChildWnds[2]
#define GD3Wnd      TabChildWnds[3]
#define ConvertWnd  TabChildWnds[4]
#define MiscWnd     TabChildWnds[5]

// Forward declarations added here if necessary
int RemoveOffset(char *filename);

#define BUFFER_SIZE 1024

struct TPSGState {
  unsigned char GGStereo;  // GG stereo byte
  unsigned short ToneFreqs[3];
  unsigned char NoiseByte;
  unsigned char Volumes[4];
  unsigned char PSGFrequencyLowBits,Channel;
  BOOL NoiseUpdated;
};

const int YM2413StateRegWriteFlags[YM2413NumRegs]={
//  Chooses bits when writing states (start/loop)
//  Regs:
//  0    1    2    3    4    5    6    7    8    9    a    b    c    d    e    f
//  00-07: user-definable tone channel - left at 0xff for now
//  0E:    rhythm mode control - only bit 5 since rest are unused/keys
//         Was 0x20, now 0x00 (bugfix?)
  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
//  10-18: tone F-number low bits - want all bits
  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
//  20-28: tone F-number high bit, octave set, "key" & sustain
//  0x3f = all
//  0x2f = all but key
//  0x1f = all but sustain
//  0x0f = all but key and sustain
  0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
//  30-38: instrument number/volume - want all bits
  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff
};

const int YM2413RegWriteFlags[YM2413NumRegs]={
//  Chooses bits when copying data -> want everything
//  Regs:
//  0    1    2    3    4    5    6    7    8    9    a    b    c    d    e    f
//  00-07: user-definable tone channel - left at 0xff for now
//  0E:    rhythm mode control - all (bits 0-5)
  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x00,0x00,0x00,0x3f,0x00,
//  10-18: tone F-number low bits - want all bits
  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
//  20-28: tone F-number high bit, octave set, "key" & sustain
//  0x3f = all (bits 0-5)
  0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
//  30-38: instrument number/volume - want all bits
  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff
};

struct TPSGState LastWrittenPSGState = {
  (char)0xff,  // GG stereo - all on
  {0,0,0},  // Tone channels - off
  (char)0xe5,  // Noise byte - white, medium
  {15,15,15,15},  // Volumes - all off
  0,4      // PSG low bits, channel - 4 so no values needed
};

unsigned char LastWrittenYM2413Regs[YM2413NumRegs]="";  // zeroes it

// The game could have moved a key from on to off to on within 1
// frame/pause... so I need to keep a check on that >:(
// Bits
// 0-4 = percussion bits
// 5-13 = tone channels
int
  KeysLifted=0,
  KeysPressed=0;
  NoiseChanged=0;

void WriteVGMInfo(gzFile out,long *pauselength, struct TPSGState *PSGState, unsigned char YM2413Regs[YM2413NumRegs]) {
  int i;
  if (!*pauselength) return;  // If pause=zero do nothing so only the last write to each register before a pause is written
  // Write PSG stuff
  if (PSGState->GGStereo!=LastWrittenPSGState.GGStereo) {  // GG stereo
    gzputc(out,VGM_GGST);
    gzputc(out,PSGState->GGStereo);
  }
  for (i=0;i<3;++i) {  // Tone channel frequencies
    if (PSGState->ToneFreqs[i]!=LastWrittenPSGState.ToneFreqs[i]) {
      gzputc(out,VGM_PSG);
      gzputc(out,
        0x80 |    // bit 7 = 1, 4 = 0 -> PSG tone byte 1
        (i<<5) |  // bits 5 and 6 -> select channel
        (PSGState->ToneFreqs[i] & 0xf)  // bits 0 to 3 -> low 4 bits of freq
      );
      gzputc(out,VGM_PSG);
      gzputc(out,
        // bits 6 and 7 = 0 -> PSG tone byte 2
        (PSGState->ToneFreqs[i] >> 4)  // bits 0 to 5 -> high 6 bits of freq
      );
    }
  }
  if (NoiseChanged) {
    // Writing to the noise register resets the LFSR so I have to always repeat writes
    gzputc(out,VGM_PSG);  // 1-stage write
    gzputc(out,PSGState->NoiseByte);
    NoiseChanged=0;
  }
  for (i=0;i<4;++i) {  // All 4 channels volumes
    if (PSGState->Volumes[i]!=LastWrittenPSGState.Volumes[i]) {
      gzputc(out,VGM_PSG);
      gzputc(out,
        0x90 |    // bits 4 and 7 = 1 -> volume
        (i<<5) |  // bits 5 and 6 -> select channel
        PSGState->Volumes[i]  // bits 0 to 4 -> volume
      );
    }
  }
  // Write YM2413 stuff
  for (i=0;i<YM2413NumRegs;++i) if (YM2413RegWriteFlags[i]) {
    if (
      ((YM2413Regs[i]&YM2413RegWriteFlags[i])!=LastWrittenYM2413Regs[i]) &&
      (i!=0x0e)  // not percussion
    ){
      gzputc(out,VGM_YM2413);  // YM2413
      gzputc(out,i);    // Register
      gzputc(out,(YM2413Regs[i] & YM2413RegWriteFlags[i]));  // Value
      LastWrittenYM2413Regs[i]=YM2413Regs[i] & YM2413RegWriteFlags[i];
    }
  }
  // Percussion after tone
  if (LastWrittenYM2413Regs[0x0e]!=(YM2413Regs[0x0e] & YM2413RegWriteFlags[0x0e])) {
    gzputc(out,VGM_YM2413);  // YM2413
    gzputc(out,0x0e);    // Register
    gzputc(out,(YM2413Regs[0x0e] & YM2413RegWriteFlags[0x0e]));  // Value
    LastWrittenYM2413Regs[0x0e]=YM2413Regs[0x0e] & YM2413RegWriteFlags[0x0e];
  }
  // Now we have to handle keys which have gone from lifted to
  // pressed since the last write - if they were pressed at the last
  // write, that means they've gone Down-Up-Down, and we need to tell
  // the YM2413 this since it would otherwise be missed by the
  // optimiser
  if (KeysLifted & KeysPressed) {  // At least one key was lifted and pressed
    // First the percussion - find which keys have done DUD
    char PercussionKeysDUD=(char)(
      (LastWrittenYM2413Regs[0x0e] & 0x1f)  // 1 for each inst currently set to on
      & KeysLifted & KeysPressed
      );  // 1 for each inst on-off-on
    int ToneKeysUD=KeysLifted & KeysPressed & 0x1fe0;
    if (ToneKeysUD) {
      // At least one tone key went from up to down. So look to
      // see if any of them were D before because if so, they've
      // gone DUD
      for (i=0;i<9;++i) {  // for each tone channel
        if (
          (ToneKeysUD & (1<<(5+i))) &&  // if it's gone UD
          (LastWrittenYM2413Regs[0x20+i] & 0x10)  // and was D before
        ) {
          // debug marker
//          gzputc(out,0x00);gzputc(out,0x00);gzputc(out,0x00);gzputc(out,0x00);gzputc(out,0x00);gzputc(out,0x00);gzputc(out,0x00);gzputc(out,0x00);gzputc(out,0x00);
          gzputc(out,VGM_YM2413);  // YM2413
          gzputc(out,0x20+i);    // Register
          gzputc(out,
            (LastWrittenYM2413Regs[0x20+i] & 0x2f)  // Turn off the key
          );  // Value
          gzputc(out,VGM_YM2413);  // YM2413
          gzputc(out,0x20+i);    // Register
          gzputc(out,LastWrittenYM2413Regs[0x20+i]);
        }
      }
    }
    // Do percussion last in case any ch6-8 writes changed it
    if (
      (PercussionKeysDUD) &&  // At least one key has gone UDU
      (LastWrittenYM2413Regs[0x0e]&0x20)  // Percussion is on
    ) {
      gzputc(out,VGM_YM2413);  // YM2413
      gzputc(out,0x0e);    // Register
      gzputc(out,
        (LastWrittenYM2413Regs[0x0e] ^ PercussionKeysDUD)  // Turn off the ones I want
        | 0x20
      );  // Value
      gzputc(out,VGM_YM2413);  // YM2413
      gzputc(out,0x0e);    // Register
      gzputc(out,LastWrittenYM2413Regs[0x0e]);
    }
  }
  KeysLifted=0;
  KeysPressed=0;
  // then pause
  WritePause(out,*pauselength);
  *pauselength=0; // maybe not needed
  // and record what we've done
  LastWrittenPSGState=*PSGState;
  // Did YM2413 as I went :)
  return;
}

void WriteYM2413State(gzFile out, char YM2413Regs[YM2413NumRegs], int IsStart) {
  int i;

  for (i=0;i<YM2413NumRegs;++i) if (YM2413StateRegWriteFlags[i]) {
    gzputc(out,VGM_YM2413);  // YM2413
    gzputc(out,i);    // Register
    gzputc(out,(YM2413Regs[i] & YM2413StateRegWriteFlags[i]));  // Value
    LastWrittenYM2413Regs[i]=YM2413Regs[i] & YM2413StateRegWriteFlags[i];
  }

  if (IsStart) {
    // Write percussion
    gzputc(out,VGM_YM2413);
    gzputc(out,0x0e);
    gzputc(out,YM2413Regs[0x0e]&YM2413RegWriteFlags[0x0e]);
    LastWrittenYM2413Regs[0x0e]=YM2413Regs[0x0e] & YM2413RegWriteFlags[0x0e];
  }
/*
  // Bugfix - turn off rhythm mode before writing state (?)
  gzputc(out,VGM_YM2413);
  gzputc(out,0x0e);
  gzputc(out,0x00);

  for (i=0;i<YM2413NumRegs;++i) if (YM2413StateRegWriteFlags[i]) {
    gzputc(out,VGM_YM2413);  // YM2413
    gzputc(out,i);    // Register
    gzputc(out,(YM2413Regs[i] & YM2413StateRegWriteFlags[i]));  // Value
    LastWrittenYM2413Regs[i]=YM2413Regs[i] & YM2413StateRegWriteFlags[i];
  }

  // Put percussion last
  gzputc(out,VGM_YM2413);
  gzputc(out,0x0e);
  gzputc(out,YM2413Regs[0x0e]&0x20);
  LastWrittenYM2413Regs[0x0e]=YM2413Regs[0x0e] & YM2413RegWriteFlags[0x0e];
*/
}

void WritePSGState(gzFile out, struct TPSGState PSGState) {
  int i;
  // GG stereo
  gzputc(out,VGM_GGST);
  gzputc(out,PSGState.GGStereo);
  // Tone channel frequencies
  for (i=0;i<3;++i) {
    gzputc(out,VGM_PSG);
    gzputc(out,
      0x80 |    // bit 7 = 1, 4 = 0 -> PSG tone byte 1
      (i<<5) |  // bits 5 and 6 -> select channel
      (PSGState.ToneFreqs[i] & 0xf)  // bits 0 to 3 -> low 4 bits of freq
    );
    gzputc(out,VGM_PSG);
    gzputc(out,
      // bits 6 and 7 = 0 -> PSG tone byte 2
      (PSGState.ToneFreqs[i] >> 4)  // bits 0 to 5 -> high 6 bits of freq
    );
  }
  // Noise
  gzputc(out,VGM_PSG);  // 1-stage write
  gzputc(out,PSGState.NoiseByte);
  NoiseChanged=0;

  // All 4 channels volumes
  for (i=0;i<4;++i) {
    gzputc(out,VGM_PSG);
    gzputc(out,
      0x90 |    // bits 4 and 7 = 1 -> volume
      (i<<5) |  // bits 5 and 6 -> select channel
      PSGState.Volumes[i]  // bits 0 to 4 -> volume
    );
  }
  LastWrittenPSGState=PSGState;
}

void CheckWriteCounts(char *filename);

// Merges data by storing writes to a buffer
// When it gets to a pause, it writes every value that has changed since
// the last write (at the last pause, or state write), and stores the
// current state for comparison next time.
void Trim(char *filename,int start,int loop,int end,BOOL OverWrite,BOOL PromptToPlay) {
  gzFile in,*out;
  struct TVGMHeader VGMHeader;
  char *Outfilename;
  char *p;
  int b0,b1,b2;
  long SampleCount=0;
  long PauseLength=0;
  long FileSizeBefore,FileSizeAfter;

  char LastFirstByteWritten=0;
  char WrittenStart=0,WrittenLoop=0;

  struct TPSGState CurrentPSGState = {
    (char)0xff,  // GG stereo - all on
    {0,0,0},  // Tone channels - off
    (char)0xe5,  // Noise byte - white, medium
    {15,15,15,15},  // Volumes - all off
    0,4      // PSG low bits, channel - 4 so no values needed
  };

  unsigned char YM2413Regs[YM2413NumRegs];

  if (!FileExists(filename)) return;

  if(IsDlgButtonChecked(TrimWnd,cbLogTrims)) LogTrim(Currentfilename,start,loop,end);

  CheckWriteCounts(filename);

  memset(YM2413Regs,0,sizeof(YM2413Regs));

  if ((start>end)||(loop>end)||((loop>-1) && (loop<start))) {
    ShowError("Impossible edit points!");
    return;
  }

  in=gzopen(filename,"rb");

  // Read header
  if(!ReadVGMHeader(in,&VGMHeader,FALSE)) {
    gzclose(in);
	return;
  }

  FileSizeBefore=VGMHeader.EoFOffset+EOFDELTA;
  gzseek(in,VGM_DATA_OFFSET,SEEK_SET);

  ShowStatus("Trimming VGM data...");

  Outfilename=malloc(strlen(filename)+15);
  strcpy(Outfilename,filename);
  for (p=Outfilename+strlen(Outfilename)-1;p>=Outfilename;--p) if (*p=='.') {
    strcpy(p," (trimmed).vgz");
    break;
  }

  out=gzopen(Outfilename,"wb0");  // No compression, since I'll recompress it later

  // Write header (update it later)
  gzwrite(out,&VGMHeader,sizeof(VGMHeader));
  gzseek(out,VGM_DATA_OFFSET,SEEK_SET);

  if (end>VGMHeader.TotalLength) {
    ShowMessage("End point (%d samples) beyond end of file!\nUsing maximum value of %d samples instead",end,VGMHeader.TotalLength);
    end=VGMHeader.TotalLength;
  }

  do {
    b0=gzgetc(in);
    switch (b0) {
    case VGM_GGST:  // GG stereo (1 byte data)
      b1=gzgetc(in);
      if (b1!=CurrentPSGState.GGStereo) {  // Do not copy it if the current stereo state is the same
        CurrentPSGState.GGStereo=b1;
        if ((WrittenStart) && (VGMHeader.PSGClock))
          WriteVGMInfo(out,&PauseLength,&CurrentPSGState,YM2413Regs);
      }
      break;
    case VGM_PSG:  // PSG write (1 byte data)
      b1=gzgetc(in);
      switch (b1 & 0x90) {
      case 0x00:  // fall through
      case 0x10:  // second frequency byte
        if (CurrentPSGState.Channel>3) break;
        if (CurrentPSGState.Channel==3) {
          // 2nd noise byte (Micro Machines title screen)
          // Always write
          NoiseChanged=1;
          CurrentPSGState.NoiseByte=(b1&0xf)|0xe0;
          if ((WrittenStart) && (VGMHeader.PSGClock))
            WriteVGMInfo(out,&PauseLength,&CurrentPSGState,YM2413Regs);
        } else {
          int freq=(b1&0x3F)<<4|CurrentPSGState.PSGFrequencyLowBits;
          if (freq<PSGCutoff) freq=0;
          if (CurrentPSGState.ToneFreqs[CurrentPSGState.Channel]!=freq) {  // Changes the freq
            char FirstByte=0x80 | (CurrentPSGState.Channel<<5) | CurrentPSGState.PSGFrequencyLowBits;  // 1st byte needed for this freq
            if (FirstByte!=LastFirstByteWritten) {  // If necessary, write 1st byte
              if ((WrittenStart) && (VGMHeader.PSGClock))
                WriteVGMInfo(out,&PauseLength,&CurrentPSGState,YM2413Regs);
              LastFirstByteWritten=FirstByte;
            }
            // Don't write if volume is off
            if ((WrittenStart) && (VGMHeader.PSGClock) /*&& (CurrentPSGState.Volumes[CurrentPSGState.Channel])*/)
              WriteVGMInfo(out,&PauseLength,&CurrentPSGState,YM2413Regs);
            CurrentPSGState.ToneFreqs[CurrentPSGState.Channel]=freq;  // Write 2nd byte
          }
        }
        break;
      case 0x80:
        if ((b1&0x60)==0x60) {  // noise
          // No "does it change" because writing resets the LFSR (ie. has an effect)
          NoiseChanged=1;
          CurrentPSGState.NoiseByte=b1;
          CurrentPSGState.Channel=3;
          if ((WrittenStart) && (VGMHeader.PSGClock))
            WriteVGMInfo(out,&PauseLength,&CurrentPSGState,YM2413Regs);
        } else {  // First frequency byte
          CurrentPSGState.Channel=(b1&0x60)>>5;
          CurrentPSGState.PSGFrequencyLowBits=b1&0xF;
        }
        break;
      case 0x90:  // set volume
        {
          char chan=(b1&0x60)>>5;
          char vol=b1&0xF;
          if (CurrentPSGState.Volumes[chan]!=vol) {
            CurrentPSGState.Volumes[chan]=vol;
            CurrentPSGState.Channel=4;
            // Only write volume change if we've got to the start and PSG is turned on
            if ((WrittenStart) && (VGMHeader.PSGClock))
              WriteVGMInfo(out,&PauseLength,&CurrentPSGState,YM2413Regs);
          }
        }
        break;
      } // end case
      break;
    case VGM_YM2413:  // YM2413
      b1=gzgetc(in);
      b2=gzgetc(in);
      if ((b1>=YM2413NumRegs) || !(YM2413RegWriteFlags[b1])) break;  // Discard invalid register numbers
      YM2413Regs[b1]=b2;

      // Check for percussion keys lifted or pressed
      if (b1==0x0e) {
        KeysLifted|=(b2 ^ 0x1f) & 0x1f;
        // OR the percussion keys with the inverse of the perc.
        // keys to get a 1 stored there if the key has been lifted
        KeysPressed|=(b2 & 0x1f);
        // Do similar with the non-inverse to get the keys pressed
      }
      // Check for tone keys lifted or pressed
      if ((b1>=0x20) && (b1<=0x28)) {
        if (b2 & 0x10)  // Key was pressed
          KeysPressed|=1<<(b1-0x20+5);
        else
          KeysLifted|=1<<(b1-0x20+5);
        if (
          (KeysLifted & KeysPressed & (1<<(b1-0x20+5)))  // the key has gone UD
          &&
          (LastWrittenYM2413Regs[b1] & 0x10)  // 0x10 == 00010000 == YM2413 tone key bit
                            // ie. the key was D before UD
        ) {
          KeysLifted=KeysLifted;
        }
      }

      if ((WrittenStart) && (VGMHeader.YM2413Clock))
        WriteVGMInfo(out,&PauseLength,&CurrentPSGState,YM2413Regs);
      break;
    case VGM_YM2612_0:  // YM2612 port 0
    case VGM_YM2612_1:  // YM2612 port 1
    case VGM_YM2151:  // YM2151
      b1=gzgetc(in);
      b2=gzgetc(in);
      gzputc(out,b0);
      gzputc(out,b1);
      gzputc(out,b2);
      break;
    case 0x55:  // Reserved up to 0x5f
    case 0x56:  // All have 2 bytes of data
    case 0x57:  // which I discard :)
    case 0x58:
    case 0x59:
    case 0x5a:
    case 0x5b:
    case 0x5c:
    case 0x5d:
    case 0x5e:
    case 0x5f:
      b1=gzgetc(in);
      b2=gzgetc(in);
      break;
    case VGM_PAUSE_WORD:  // Wait n samples
      b1=gzgetc(in);
      b2=gzgetc(in);
      SampleCount+=b1|(b2<<8);
      PauseLength+=b1|(b2<<8);
      if ((WrittenStart) && ((SampleCount<=loop) || (loop==-1) || (WrittenLoop)) && (SampleCount<=end) ) WriteVGMInfo(out,&PauseLength,&CurrentPSGState,YM2413Regs);
      break;
    case VGM_PAUSE_60TH:  // Wait 1/60 s
      SampleCount+=LEN60TH;
      PauseLength+=LEN60TH;
      if ((WrittenStart) && ((SampleCount<=loop) || (loop==-1) || (WrittenLoop)) && (SampleCount<=end) ) WriteVGMInfo(out,&PauseLength,&CurrentPSGState,YM2413Regs);
      break;
    case VGM_PAUSE_50TH:  // Wait 1/50 s
      SampleCount+=LEN50TH;
      PauseLength+=LEN50TH;
      if ((WrittenStart) && ((SampleCount<=loop) || (loop==-1) || (WrittenLoop)) && (SampleCount<=end) ) WriteVGMInfo(out,&PauseLength,&CurrentPSGState,YM2413Regs);
      break;
//    case VGM_PAUSE_BYTE:  // Wait n samples
//      b1=gzgetc(in);
//      SampleCount+=b1;
//      PauseLength+=b1;
//      if ((WrittenStart) && ((SampleCount<=loop) || (loop==-1) || (WrittenLoop)) && (SampleCount<=end) ) WriteVGMInfo(out,&PauseLength,&CurrentPSGState,YM2413Regs);
//      break;
    case 0x70: case 0x71: case 0x72: case 0x73: case 0x74: case 0x75: case 0x76: case 0x77:
    case 0x78: case 0x79: case 0x7a: case 0x7b: case 0x7c: case 0x7d: case 0x7e: case 0x7f: // Wait 1-16 samples
      b1=(b0 & 0xf)+1;
      SampleCount+=b1;
      PauseLength+=b1;
      if ((WrittenStart) && ((SampleCount<=loop) || (loop==-1) || (WrittenLoop)) && (SampleCount<=end) ) WriteVGMInfo(out,&PauseLength,&CurrentPSGState,YM2413Regs);
      break;
    case VGM_END:  // End of sound data
      gzclose(in);
      gzclose(out);
      free(Outfilename);
      ShowError("Reached end of VGM data! There must be something wrong - try fixing the lengths for this file");
      return;
    default:
      break;
    }

    // Loop point
    if ((!WrittenLoop) && (loop!=-1) && (SampleCount>=loop)) {
      if (WrittenStart) {
        PauseLength=loop-(SampleCount-PauseLength);  // Write any remaining pause up to the edit point
        WriteVGMInfo(out,&PauseLength,&CurrentPSGState,YM2413Regs);
      }
      PauseLength=SampleCount-loop;  // and remember any left over
      // Remember offset
      VGMHeader.LoopOffset=gztell(out)-LOOPDELTA;
      // Write loop point initialisation... unless start = loop
      // because then the start initialisation will work
      if (loop!=start) {
        if (IsWindowEnabled(GetDlgItem(StripWnd,gbPSG))) {
          CurrentPSGState.NoiseByte&=0xf7;
          WritePSGState(out,CurrentPSGState);
        }
        if (IsWindowEnabled(GetDlgItem(StripWnd,gbYM2413))) WriteYM2413State(out,YM2413Regs,FALSE);
      }
      WrittenLoop=1;
    }
    // Start point
    if ((!WrittenStart) && (SampleCount>start)) {
      if (IsWindowEnabled(GetDlgItem(StripWnd,gbPSG))) {
        CurrentPSGState.NoiseByte&=0xf7;
        WritePSGState(out,CurrentPSGState);
      }
      if (IsWindowEnabled(GetDlgItem(StripWnd,gbYM2413))) WriteYM2413State(out,YM2413Regs,TRUE);
      // Remember any needed delay
      PauseLength=SampleCount-start;
      WrittenStart=1;
    }

    // End point
    if (SampleCount>=end) {
      // Write remaining delay
      PauseLength-=SampleCount-end;
      WritePause(out,PauseLength);
      PauseLength=0; // maybe not needed
      // End of VGM data
      gzputc(out,VGM_END);
      break;
    }
  } while (b0!=EOF);

  // Copy GD3 tag
  if (VGMHeader.GD3Offset) {
    struct TGD3Header GD3Header;
    int i;
    int NewGD3Offset=gztell(out)-GD3DELTA;
    ShowStatus("Copying GD3 tag...");
    gzseek(in,VGMHeader.GD3Offset+GD3DELTA,SEEK_SET);
    gzread(in,&GD3Header,sizeof(GD3Header));
    gzwrite(out,&GD3Header,sizeof(GD3Header));
    for (i=0; i<GD3Header.Length; ++i) {  // Copy strings
      gzputc(out,gzgetc(in));
    }
    VGMHeader.GD3Offset=NewGD3Offset;
  }
  VGMHeader.EoFOffset=gztell(out)-EOFDELTA;
  gzclose(in);
  gzclose(out);

  // Update header
  VGMHeader.TotalLength=end-start;
  if (loop>-1) {  // looped
    VGMHeader.LoopLength=end-loop;
    // Already rememebered offset
  } else {  // not looped
    VGMHeader.LoopLength=0;
    VGMHeader.LoopOffset=0;
  }

  // Amend it with the updated header
  WriteVGMHeader(Outfilename,VGMHeader);

  OptimiseVGMPauses(Outfilename);

  Compress(Outfilename);

  if (OverWrite==TRUE) {
    DeleteFile(filename);
    MoveFile(Outfilename,filename);
    strcpy(Outfilename,filename);
  }

  // Get output file size
  in=gzopen(Outfilename,"rb");
  gzread(in,&VGMHeader,sizeof(VGMHeader));
  FileSizeAfter=VGMHeader.EoFOffset+EOFDELTA;
  gzclose(in);

  ShowStatus("Trimming complete");

  if (PromptToPlay) {
    if (ShowQuestion(
      "File %s to\n%s\nUncompressed file size %d -> %d bytes (%+.2f%%)\nDo you want to open it in the associated program?",
      (OverWrite==TRUE?"optimised":"trimmed"),
      Outfilename,
      FileSizeBefore,
      FileSizeAfter,
      (((double)FileSizeAfter)-FileSizeBefore)*100/FileSizeBefore
    )==IDYES) ShellExecute(hWndMain,"Play",Outfilename,NULL,NULL,SW_NORMAL);
  }

  free(Outfilename);
}

void Optimize(char *filename) {
  gzFile in;
  struct TVGMHeader VGMHeader;
  long FileSizeBefore,FileSizeAfter;
  int NumOffsetsRemoved=0;

  in=gzopen(filename,"rb");
  gzread(in,&VGMHeader,sizeof(VGMHeader));
  gzclose(in);
  FileSizeBefore=VGMHeader.EoFOffset+EOFDELTA;

  // Make sure lengths are correct
  CheckLengths(filename,FALSE);

  // Remove PSG offsets if selected
  if ((VGMHeader.PSGClock) && IsDlgButtonChecked(TrimWnd,cbRemoveOffset)) NumOffsetsRemoved=RemoveOffset(filename);

  // Trim (using the existing edit points), also merges pauses
  if (VGMHeader.LoopLength)
    Trim(filename,0,VGMHeader.TotalLength-VGMHeader.LoopLength,VGMHeader.TotalLength,TRUE,FALSE);
  else
    Trim(filename,0,-1,VGMHeader.TotalLength,TRUE,FALSE);

  in=gzopen(filename,"rb");
  gzread(in,&VGMHeader,sizeof(VGMHeader));
  gzclose(in);
  FileSizeAfter=VGMHeader.EoFOffset+EOFDELTA;

  FixExt(filename);

  if (ShowQuestion(
    "File optimised to\n"
    "%s\n"
    "%d offsets/silent PSG writes removed,\n"
    "Uncompressed file size %d -> %d bytes (%+.2f%%)\n"
    "Do you want to open it in the associated program?",
    filename,
    NumOffsetsRemoved,
    FileSizeBefore,
    FileSizeAfter,
    (FileSizeAfter-FileSizeBefore)*100.0/FileSizeBefore
  )==IDYES) ShellExecute(hWndMain,"Play",filename,NULL,NULL,SW_NORMAL);
}

void UpdateWriteCount(const int CheckBoxes[],int Writes[],int count) {
  int i;
  char TempStr[64],*p;
  for (i=0;i<count;++i) {
    GetDlgItemText(StripWnd,CheckBoxes[i],TempStr,64);
    p=TempStr;
    while (p<TempStr+64) {
      if (*p=='(') {
        *(p-1)='\0';
        break;
      }
      ++p;
    }
    if(Writes[i]) sprintf(TempStr,"%s (%d)",TempStr,Writes[i]);
    SetDlgItemText(StripWnd,CheckBoxes[i],TempStr);
    EnableWindow(GetDlgItem(StripWnd,CheckBoxes[i]),(Writes[i]>0));
    if (!Writes[i]) CheckDlgButton(StripWnd,CheckBoxes[i],0);
  }
}

// Check checkboxes and show numbers for how many times each channel/data type is used
void CheckWriteCounts(char *filename) {
  int i,j;
  GetWriteCounts(filename,PSGWrites,YM2413Writes,YM2612Writes,YM2151Writes,ReservedWrites);

  UpdateWriteCount(     PSGCheckBoxes,     PSGWrites,NumPSGTypes);
  UpdateWriteCount(  YM2413CheckBoxes,  YM2413Writes,NumYM2413Types);
  UpdateWriteCount(  YM2612CheckBoxes,  YM2612Writes,NumYM2612Types);
  UpdateWriteCount(  YM2151CheckBoxes,  YM2151Writes,NumYM2151Types);
  UpdateWriteCount(ReservedCheckboxes,ReservedWrites,NumReservedTypes);

  // Sum stuff for group checkboxes and other stuff:
  // PSG tone channels
  for (i=j=0;i<3;++i) j+=PSGWrites[i];               // count writes
  if (!j) CheckDlgButton(StripWnd,cbPSGTone,0);       // if >0, initialise unchecked
  EnableWindow(GetDlgItem(StripWnd,cbPSGTone),(j>0)); // enabled = (>0)
  // YM2413 tone channels
  for (i=j=0;i<9;++i) j+=YM2413Writes[i];
  if (!j) CheckDlgButton(StripWnd,cbYM2413Tone,0);
  EnableWindow(GetDlgItem(StripWnd,cbYM2413Tone),(j>0));
  // YM2413 percussion
  for (i=9,j=0;i<14;++i) j+=YM2413Writes[i];  
  if (!j) CheckDlgButton(StripWnd,cbYM2413Percussion,0); 
  EnableWindow(GetDlgItem(StripWnd,cbYM2413Percussion),(j>0));
  // PSG anything
  for (i=j=0;i<NumPSGTypes;++i) j+=PSGWrites[i];    
  EnableWindow(GetDlgItem(StripWnd,gbPSG),(j!=0));
  // YM2413 anything
  for (i=j=0;i<NumYM2413Types;++i) j+=YM2413Writes[i];
  EnableWindow(GetDlgItem(StripWnd,gbYM2413),(j!=0));

  ShowStatus("Scan for chip data complete");
}


// Load a file - check it's valid, load displayed info
void LoadFile(char *filename) {
  gzFile in;
  char buffer[64];
  int Mins,Secs;
  struct TGD3Header GD3Header;
  int i,FileHasGD3=0;

  if (!FileExists(filename)) return;

  ShowStatus("Loading file...");

  in=gzopen(filename,"rb");
  gzread(in,&CurrentFileVGMHeader,sizeof(CurrentFileVGMHeader));

  if (CurrentFileVGMHeader.VGMIdent!=VGMIDENT) {  // no VGM marker
    ShowError("File is not a VGM file!\nIt will not be opened.\n\nMaybe you want to convert GYM, CYM and SSL files to VGM?\nClick on the \"Conversion\" tab.");
    strcpy(Currentfilename,"");
    SetDlgItemText(hWndMain,edtFileName,"Drop a file onto the window to load");
    ShowStatus("");
    return;
  }
  
  strcpy(Currentfilename,filename);  // Remember it
  SetDlgItemText(hWndMain,edtFileName,filename);  // Put it in the box

  if (CurrentFileVGMHeader.GD3Offset) {  // GD3 tag exists
    gzseek(in,CurrentFileVGMHeader.GD3Offset+GD3DELTA,SEEK_SET);
    gzread(in,&GD3Header,sizeof(GD3Header));
    if (
      (strncmp(GD3Header.IDString,"Gd3 ",4)==0) &&  // Has valid marker
      (GD3Header.Version>=MINGD3VERSION) &&  // and is an acceptable version
      ((GD3Header.Version & REQUIREDGD3MAJORVER)==REQUIREDGD3MAJORVER)
    ) {
      if (GD3Strings) free(GD3Strings);
      GD3Strings=malloc(GD3Header.Length);
      gzread(in,GD3Strings,GD3Header.Length);
      FileHasGD3=1;
    }
  }
  gzclose(in);

  // Rate
  SetDlgItemInt(HeaderWnd,edtPlaybackRate,CurrentFileVGMHeader.RecordingRate,FALSE);

  // Lengths
  Mins=ROUND(CurrentFileVGMHeader.TotalLength/44100.0)/60;
  Secs=ROUND(CurrentFileVGMHeader.TotalLength/44100.0-Mins*60);
  sprintf(buffer,"%d:%02d",Mins,Secs);
  SetDlgItemText(HeaderWnd,edtLengthTotal,buffer);
  if (CurrentFileVGMHeader.LoopLength) {
    Mins=ROUND(CurrentFileVGMHeader.LoopLength/44100.0)/60;
    Secs=ROUND(CurrentFileVGMHeader.LoopLength/44100.0-Mins*60);
    sprintf(buffer,"%d:%02d",Mins,Secs);
  } else strcpy(buffer,"-");
  SetDlgItemText(HeaderWnd,edtLengthLoop,buffer);

  // Version
  sprintf(buffer,"%x.%02x",CurrentFileVGMHeader.Version>>8,CurrentFileVGMHeader.Version&0xff);
  SetDlgItemText(HeaderWnd,edtVersion,buffer);

  // Clock speeds
  SetDlgItemInt(HeaderWnd,edtPSGClock,CurrentFileVGMHeader.PSGClock,FALSE);
  SetDlgItemInt(HeaderWnd,edtYM2413Clock,CurrentFileVGMHeader.YM2413Clock,FALSE);
  SetDlgItemInt(HeaderWnd,edtYM2612Clock,CurrentFileVGMHeader.YM2612Clock,FALSE);
  SetDlgItemInt(HeaderWnd,edtYM2151Clock,CurrentFileVGMHeader.YM2151Clock,FALSE);

  // PSG settings
  sprintf(buffer,"0x%04x",CurrentFileVGMHeader.PSGWhiteNoiseFeedback);
  SetDlgItemText(HeaderWnd,edtPSGFeedback,buffer);
  SetDlgItemInt(HeaderWnd,edtPSGSRWidth,CurrentFileVGMHeader.PSGShiftRegisterWidth,FALSE);

  // GD3 tag
  if (GD3Strings) {
    wchar *GD3string=GD3Strings;  // pointer to parse GD3Strings
    char MBCSstring[1024*2];  // Allow 2KB to hold each string
    wchar ModifiedString[1024*2],*wp;

    for (i=0;i<NumGD3Strings;++i) {
      wcscpy(ModifiedString,GD3string);

      switch (i) {  // special handling for any strings?
      case 10:  // Notes: change \n to \r\n so Windows shows it properly
        {
          wp=ModifiedString+wcslen(ModifiedString);
          while (wp>=ModifiedString) {
            if (*wp==L'\n') {
              memmove(wp+1,wp,(wcslen(wp)+1)*2);
              *wp=L'\r';                    
            }
            wp--;
          }
        }
        break;
      }

      if (!SetDlgItemTextW(GD3Wnd,GD3EditControls[i],ModifiedString)) {
        // Widechar text setting failed, try WC2MB
        BOOL ConversionFailure;
        WideCharToMultiByte(CP_ACP,0,ModifiedString,-1,MBCSstring,1024,NULL,&ConversionFailure);
        if (ConversionFailure) {
          // Alternate method: transcribe to HTML escaped data
          wchar *p=ModifiedString;
          unsigned int j;
          char tempstr[10];
          *MBCSstring='\0';
          for (j=0;j<wcslen(ModifiedString);j++) {
            int val=*p;
            if (val<128) sprintf(tempstr,"%c",*p);
            else sprintf(tempstr,"&#x%04x;",val);
            strcat(MBCSstring,tempstr);
            p++;
          }
        }
        SetDlgItemText(GD3Wnd,GD3EditControls[i],MBCSstring);  // and put it in the edit control
      }
    
      // Move the pointer to the next string
      GD3string+=wcslen(GD3string)+1;
    }
  }

  CheckWriteCounts(NULL); // reset counts

  if ((!FileHasGD3) && GD3Strings) 
    ShowStatus("File loaded - file has no GD3 tag, previous tag kept");
  else
    ShowStatus("File loaded");
}

void UpdateHeader() {
  char buffer[64];
  BOOL b=FALSE;
  int i,j;
  gzFile in;
  struct TVGMHeader VGMHeader;

  if (!FileExists(Currentfilename)) return;

  in=gzopen(Currentfilename,"rb");
  gzread(in,&VGMHeader,sizeof(VGMHeader));
  gzclose(in);

  if(GetInt(HeaderWnd,edtPlaybackRate,&i)) VGMHeader.RecordingRate=i;

  GetDlgItemText(HeaderWnd,edtVersion,buffer,64);
  if (sscanf(buffer,"%d.%d",&i,&j)==2)  // valid data
    VGMHeader.Version = 
      (i/10) << 12 | // major tens
      (i%10) << 8  | // major units
      (j/10) << 4  | // minor tens
      (j%10);        // minor units

  if(GetInt(HeaderWnd,edtPSGClock,&i)) VGMHeader.PSGClock=i;
  if(GetInt(HeaderWnd,edtYM2413Clock,&i)) VGMHeader.YM2413Clock=i;
  if(GetInt(HeaderWnd,edtYM2612Clock,&i)) VGMHeader.YM2612Clock=i;
  if(GetInt(HeaderWnd,edtYM2151Clock,&i)) VGMHeader.YM2151Clock=i;

  GetDlgItemText(HeaderWnd,edtPSGFeedback,buffer,64);
  if (sscanf(buffer,"0x%x",&i)==1)  // valid data
    VGMHeader.PSGWhiteNoiseFeedback=i;
  if(GetInt(HeaderWnd,edtPSGSRWidth,&i)) VGMHeader.PSGShiftRegisterWidth=i;

  WriteVGMHeader(Currentfilename,VGMHeader);
}

void ClearGD3Strings(){
  int i;
  for (i=0;i<NumGD3Strings;++i) SetDlgItemText(GD3Wnd,GD3EditControls[i],"");
}

void UpdateGD3() {
  wchar *GD3string=GD3Strings;  // pointer to parse GD3Strings
  gzFile in,*out;
  struct TVGMHeader VGMHeader;
  struct TGD3Header GD3Header;
  char *Outfilename;
  char *p;
  long int i;
  wchar AllGD3Strings[1024*NumGD3Strings]=L"";  // zeroes the whole buffer
  wchar *AllGD3End=AllGD3Strings;
  int ConversionErrors=0;

  if (!FileExists(Currentfilename)) return;

  ShowStatus("Updating GD3 tag...");

  in=gzopen(Currentfilename,"rb");
  if(!ReadVGMHeader(in,&VGMHeader,FALSE)) {
    gzclose(in);
	return;
  }
  
  gzrewind(in);

  Outfilename=malloc(strlen(Currentfilename)+10);
  strcpy(Outfilename,Currentfilename);
  for (p=Outfilename+strlen(Outfilename); p>=Outfilename; --p) {
    if (*p=='.') {
      strcpy(p," (tagged).vgz");  // must be different from temp exts used elsewhere
      break;
    }
  }
  
  out=gzopen(Outfilename,"wb0");

  // Copy everything up to the GD3 tag
  for (i=0;i<(VGMHeader.GD3Offset?VGMHeader.GD3Offset+GD3DELTA:VGMHeader.EoFOffset+EOFDELTA);++i) gzputc(out,gzgetc(in));

  VGMHeader.GD3Offset=gztell(out)-GD3DELTA;  // record GD3 position

  for (i=0;i<NumGD3Strings;++i) {
    wchar widestr[1024]=L"\0";
    if (!GetDlgItemTextW(GD3Wnd,GD3EditControls[i],widestr,1023)) {
      // GetW failed, try to do MB2WC
      char s[1024]="\0";
  
      GetDlgItemText(GD3Wnd,GD3EditControls[i],s,1023);  // get string

      if (MultiByteToWideChar(CP_ACP,MB_PRECOMPOSED+MB_ERR_INVALID_CHARS,s,-1,widestr,1024)==0)  // convert to Unicode
        ConversionErrors++;
    }
    // parse widestring for HTML-encoded characters
    {
      wchar tempstr[5],*wp;
      int value=0;
      do {
        wp=wcsstr(widestr,L"&#x");
        if (wp) {  // string found
          // copy chars
          wcsncpy(tempstr,wp+3,4);
          tempstr[4]=L'\0';
          // get value
          if (swscanf(tempstr,L"%x",&value)==1) {
            // Put it in that position
            *wp=(wchar)value;
            // Move the rest of the string down by 7 ("&#xnnnn;"=8)
            // "&#xnnnn;...\0" len=11
            //  ^- wp, Unicode value
            //          ^- where to copy from, = wp+8
            //          ^--^ how many, = len-7
            memmove(wp+1,wp+8,(wcslen(wp)-8+1)*2);
            // tricky to work out...
            // the from and to parameters handle the 2-byte nature
            // the length parameter doesn't
          }
        }
      } while (wp);
    }

    {  // Special handling for any strings
      wchar *p;
      switch (i) {  
      case 10:    // Notes - change \r\n to \n
        p=widestr+wcslen(widestr);  // point to end of string
        while (p>=widestr) {
          if (*p==L'\r') memmove(p,p+1,wcslen(p)*2);
          p--;
        }
        break;
      }
    }

    wcscpy(AllGD3End,widestr);  // Add to the big string
    AllGD3End+=wcslen(widestr)+1;  // and move pointer to after the \0
  }
  AllGD3End++;  // Final null wchar

  strncpy(GD3Header.IDString,"Gd3 ",4);
  GD3Header.Version=0x0100;
  GD3Header.Length=(AllGD3End-AllGD3Strings)*2;

  gzwrite(out,&GD3Header,sizeof(GD3Header));  // write GD3 header
  gzwrite(out,AllGD3Strings,GD3Header.Length);  // write GD3 strings

  VGMHeader.EoFOffset=gztell(out)-EOFDELTA;  // Update EoF offset in header

  gzclose(in);
  gzclose(out);

  WriteVGMHeader(Outfilename,VGMHeader);  // Write changed header

  DeleteFile(Currentfilename);
  MoveFile(Outfilename,Currentfilename);

  free(Outfilename);

  if (ConversionErrors) {
    ShowError("There were %d error(s) when converting the GD3 tag to Unicode. Some fields may be truncated or incorrect.",ConversionErrors);
  }
  ShowStatus("GD3 tag updated");
}

// Remove data for checked boxes
void Strip(char *filename,char *Outfilename) {
  gzFile in,*out;
  struct TVGMHeader VGMHeader;
  signed int b0,b1,b2;
  int i;
  char LatchedChannel=0;
  unsigned char PSGMask,YM2612Mask,YM2151Mask;
  unsigned long YM2413Mask;
  long int NewLoopOffset=0;

  // Set up masks
     PSGMask=(1<<NumPSGTypes   )-1; for (i=0;i<NumPSGTypes   ;i++) if ((IsDlgButtonChecked(StripWnd,PSGCheckBoxes[i]   )) || (!IsWindowEnabled(GetDlgItem(StripWnd,PSGCheckBoxes[i]   ))))    PSGMask^=(1<<i);
  YM2413Mask=(1<<NumYM2413Types)-1; for (i=0;i<NumYM2413Types;i++) if ((IsDlgButtonChecked(StripWnd,YM2413CheckBoxes[i])) || (!IsWindowEnabled(GetDlgItem(StripWnd,YM2413CheckBoxes[i])))) YM2413Mask^=(1<<i);
  YM2612Mask=(1<<NumYM2612Types)-1; for (i=0;i<NumYM2612Types;i++) if ((IsDlgButtonChecked(StripWnd,YM2612CheckBoxes[i])) || (!IsWindowEnabled(GetDlgItem(StripWnd,YM2612CheckBoxes[i])))) YM2612Mask^=(1<<i);
  YM2151Mask=(1<<NumYM2151Types)-1; for (i=0;i<NumYM2151Types;i++) if ((IsDlgButtonChecked(StripWnd,YM2151CheckBoxes[i])) || (!IsWindowEnabled(GetDlgItem(StripWnd,YM2151CheckBoxes[i])))) YM2151Mask^=(1<<i);

  in=gzopen(filename,"rb");

  // Read header
  if(!ReadVGMHeader(in,&VGMHeader,FALSE)) {
    gzclose(in);
	return;
  }
  gzseek(in,VGM_DATA_OFFSET,SEEK_SET);

  out=gzopen(Outfilename,"wb0");  // No compression, since I'll recompress it later

  // Write header... update it later
  gzwrite(out,&VGMHeader,sizeof(VGMHeader));
  gzseek(out,VGM_DATA_OFFSET,SEEK_SET);

  // process file
  do {
    if ((VGMHeader.LoopOffset) && (gztell(in)==VGMHeader.LoopOffset+LOOPDELTA)) NewLoopOffset=gztell(out)-LOOPDELTA;
    b0=gzgetc(in);
    switch (b0) {
    case VGM_GGST:  // GG stereo
      b1=gzgetc(in);
      if (PSGMask & (1<<4)) {
        gzputc(out,VGM_GGST);
        gzputc(out,b1);
      }
      break;
    case VGM_PSG:  // PSG write (1 byte data)
      b1=gzgetc(in);
      if (b1&0x80) LatchedChannel=((b1>>5)&0x03);
      if (PSGMask&(1<<LatchedChannel)) {
        gzputc(out,VGM_PSG);
        gzputc(out,b1);
      }
      break;
    case VGM_YM2413:  // YM2413
      b1=gzgetc(in);
      b2=gzgetc(in);
      switch (b1>>4) {  // go by 1st digit first
      case 0x0:  // User tone / percussion
        switch (b1) {
        case 0x00:
        case 0x01:
        case 0x02:
        case 0x03:
        case 0x04:
        case 0x05:
        case 0x06:
        case 0x07:
          if (YM2413Mask & (1<<14)) {gzputc(out,VGM_YM2413);gzputc(out,b1);gzputc(out,b2);}
          break;
        case 0x0E:  // Percussion
          //if ((b2&0x20) && !(YM2413Mask&0x3e00)) break;  // break if no percussion is wanted and it's rhythm mode
          // Original data  --RBSTCH
          //          00111100
          // Mask        00110101 = (YM2413Mask>>9)&0x1f|0x20
          // Result      00110100
          b2&=(YM2413Mask>>9)&0x1f|0x20;
          gzputc(out,VGM_YM2413);gzputc(out,b1);gzputc(out,b2);
          break;
        default:
          if (YM2413Mask & (1<<15)) {gzputc(out,VGM_YM2413);gzputc(out,b1);gzputc(out,b2);}
          break;
        }
        break;
      case 0x1:  // Tone F-number low 8 bits
        if (b1>0x18) {
          if (YM2413Mask & (1<<15)) {gzputc(out,VGM_YM2413);gzputc(out,b1);gzputc(out,b2);}
        } else {
          if ((YM2413Mask & (1<<(b1&0xf))) ||      // if channel is on
            ((b1>=0x16) && (YM2413Mask & 0x3e00)))  // or last 3 channels AND percussion is wanted
          {gzputc(out,VGM_YM2413);gzputc(out,b1);gzputc(out,b2);}
        }
        break;
      case 0x2:  // Tone more stuff including key
        if (b1>0x28) {
          if (YM2413Mask & (1<<15)) {gzputc(out,VGM_YM2413);gzputc(out,b1);gzputc(out,b2);}
        } else {
          if ((YM2413Mask & (1<<(b1&0xf))) ||              // if channel is on
            ((b1>=0x26) && (YM2413Mask & 0x3e00) /*&& !(b1&0x10)*/))  // or last 3 channels AND percussion is wanted AND key is off
          {gzputc(out,VGM_YM2413);gzputc(out,b1);gzputc(out,b2);}
        }
        break;
      case 0x3:  // Tone instruments and volume
        if (b1>YM2413NumRegs) {
          if (YM2413Mask & (1<<15)) {gzputc(out,VGM_YM2413);gzputc(out,b1);gzputc(out,b2);}
        } else {
          if ((YM2413Mask & (1<<(b1&0xf))) ||      // if channel is on
            ((b1>=0x36) && (YM2413Mask & 0x3e00)))  // or last 3 channels AND percussion is on
          {gzputc(out,VGM_YM2413);gzputc(out,b1);gzputc(out,b2);}
        }
        break;
      default:
        if (YM2413Mask & (1<<15)) {gzputc(out,VGM_YM2413);gzputc(out,b1);gzputc(out,b2);}
        break;
      }
      break;
    case VGM_YM2612_0:  // YM2612 port 0
    case VGM_YM2612_1:  // YM2612 port 1
      b1=gzgetc(in);
      b2=gzgetc(in);
      if (YM2612Mask & (1<<0)) {gzputc(out,b0);gzputc(out,b1);gzputc(out,b2);}
      break;
    case VGM_YM2151:  // YM2151
      b1=gzgetc(in);
      b2=gzgetc(in);
      if (YM2151Mask & (1<<0)) {gzputc(out,b0);gzputc(out,b1);gzputc(out,b2);}
      break;
    case 0x55:  // Reserved up to 0x5f
    case 0x56:  // All have 2 bytes of data
    case 0x57:  // which I discard :)
    case 0x58:
    case 0x59:
    case 0x5a:
    case 0x5b:
    case 0x5c:
    case 0x5d:
    case 0x5e:
    case 0x5f:
      gzseek(in,2,SEEK_CUR);
      break;
    case VGM_PAUSE_WORD:  // Wait n samples
      b1=gzgetc(in);
      b2=gzgetc(in);
      gzputc(out,b0);gzputc(out,b1);gzputc(out,b2);
      break;
    case VGM_PAUSE_60TH:  // Wait 1/60 s
    case VGM_PAUSE_50TH:  // Wait 1/50 s
      gzputc(out,b0);
      break;
//    case VGM_PAUSE_BYTE:  // Wait n samples
//      b1=gzgetc(in);
//      gzputc(out,b0);gzputc(out,b1);
//      break;
    case 0x70: case 0x71: case 0x72: case 0x73: case 0x74: case 0x75: case 0x76: case 0x77:
    case 0x78: case 0x79: case 0x7a: case 0x7b: case 0x7c: case 0x7d: case 0x7e: case 0x7f: // Wait 1-16 samples
      gzputc(out,b0);
      break;
    case VGM_END:  // End of sound data
      gzputc(out,b0);
      b0=EOF;  // make it break out
      break;
    default:
      break;
    }
  } while (b0!=EOF);

  // If removed PSG or FM then set the clock appropriately
  if (!PSGMask) VGMHeader.PSGClock=0;
  if (!YM2413Mask) VGMHeader.YM2413Clock=0;
  if (!YM2612Mask) VGMHeader.YM2612Clock=0;
  if (!YM2151Mask) VGMHeader.YM2151Clock=0;

  // Then copy the GD3 over
  if (VGMHeader.GD3Offset) {
    struct TGD3Header GD3Header;
    int i;
    int NewGD3Offset=gztell(out)-GD3DELTA;
    gzseek(in,VGMHeader.GD3Offset+GD3DELTA,SEEK_SET);
    gzread(in,&GD3Header,sizeof(GD3Header));
    gzwrite(out,&GD3Header,sizeof(GD3Header));
    for (i=0; i<GD3Header.Length; ++i) {  // Copy strings
      gzputc(out,gzgetc(in));
    }
    VGMHeader.GD3Offset=NewGD3Offset;
  }
  VGMHeader.LoopOffset=NewLoopOffset;
  VGMHeader.EoFOffset=gztell(out)-EOFDELTA;

  gzclose(in);
  gzclose(out);
  
  // Amend it with the updated header
  WriteVGMHeader(Outfilename,VGMHeader);
}

void StripChecked (char *filename) {
  char Tmpfilename[MAX_PATH+10],Outfilename[MAX_PATH+10],*p;
  struct TVGMHeader VGMHeader;
  gzFile in;

  if (!FileExists(filename)) return;

  ShowStatus("Stripping chip data...");

  in=gzopen(filename,"rb");
  if(!ReadVGMHeader(in,&VGMHeader,FALSE)) {
    gzclose(in);
	return;
  }
  gzclose(in);

  // Set up filenames
  strcpy(Tmpfilename,filename);
  p=Tmpfilename+strlen(Tmpfilename); while (p>=Tmpfilename) { if (*p=='.') { strcpy(p,".tmp5"); break; } p--; }
  strcpy(Outfilename,filename);
  p=Outfilename+strlen(Outfilename); while (p>=Outfilename) { if (*p=='.') { strcpy(p," (stripped).vgz"); break; } p--; }

  // Strip data
  Strip(filename,Outfilename);
/*
  // Optimise output file
  if (VGMHeader.LoopLength) Trim(Tmpfilename,0,VGMHeader.TotalLength-VGMHeader.LoopLength,VGMHeader.TotalLength,TRUE,FALSE);
  else Trim(Tmpfilename,0,-1,VGMHeader.TotalLength,TRUE,FALSE);

  // Strip data again because optimiser added it back
  Strip(Tmpfilename,Outfilename);
  
  // Clean up
  DeleteFile(Tmpfilename);
*/

  ShowStatus("Data stripping complete");

  if (ShowQuestion("Stripped VGM data written to\n%s\nDo you want to open it in the associated program?",Outfilename)==IDYES)
    ShellExecute(hWndMain,"open",Outfilename,NULL,NULL,SW_NORMAL);
}

void ccb(const int CheckBoxes[],const int WriteCount[],int count,int mode) {
  int i,Cutoff=0;
  char tempstr[64];

  for (i=0;i<count;++i) if (Cutoff<WriteCount[i]) Cutoff=WriteCount[i];
  Cutoff=Cutoff/50;  // 2% of largest for that chip

  for (i=0;i<count;++i) {
    if (IsWindowEnabled(GetDlgItem(StripWnd,CheckBoxes[i]))) switch (mode) {
    case 1:  // check all
      CheckDlgButton(StripWnd,CheckBoxes[i],1);
      break;
    case 2:  // check none
      CheckDlgButton(StripWnd,CheckBoxes[i],0);
      break;
    case 3: // invert selection
      CheckDlgButton(StripWnd,CheckBoxes[i],!IsDlgButtonChecked(StripWnd,CheckBoxes[i]));
      break;
    case 4:  // guess
      GetDlgItemText(StripWnd,CheckBoxes[i],tempstr,64);
      CheckDlgButton(StripWnd,CheckBoxes[i],((WriteCount[i]<Cutoff) || (strstr(tempstr,"Invalid"))));
      break;
    }
  }
}

void ChangeCheckBoxes(int mode) {
  ccb(   PSGCheckBoxes,   PSGWrites,NumPSGTypes   ,mode);
  ccb(YM2413CheckBoxes,YM2413Writes,NumYM2413Types,mode);
  ccb(YM2612CheckBoxes,YM2612Writes,NumYM2612Types,mode);
  ccb(YM2151CheckBoxes,YM2151Writes,NumYM2151Types,mode);
}

/*
// Go through file, convert 50<->60Hz
void ConvertRate(char *filename) {
  gzFile in,*out;
  struct TVGMHeader VGMHeader;
  char *Outfilename;
  char *p;
  int b0,b1,b2;
  long PauseLength=0;
  int BytesAdded=0,BytesAddedBeforeLoop=0;  // for when I have to expand wait nnnn commands

  if (!FileExists(filename)) return;

  in=gzopen(filename,"rb");

  // Read header
  gzread(in,&VGMHeader,sizeof(VGMHeader));
  gzseek(in,VGM_DATA_OFFSET,SEEK_SET);

  if (strncmp(VGMHeader.VGMIdent,"Vgm ",4)!=0) {  // no VGM marker
    sprintf(MessageBuffer,"File is not a VGM file! (no \"Vgm \")");
    ShowMessage();
    gzclose(in);
    return;
  }

  // See what I'm converting from
  switch (VGMHeader.RecordingRate) {
    case 50:
      ShowStatus("Converting rate from 50 to 60Hz...");
      break;
    case 60:
      ShowStatus("Converting rate from 50 to 60Hz...");
      break;
    default:
      sprintf(MessageBuffer,"Cannot convert this file because its recording rate is not set");
      ShowMessage();
      gzclose(in);
      return;
  }

  Outfilename=malloc(strlen(filename)+40);
  strcpy(Outfilename,filename);
  for (p=Outfilename+strlen(Outfilename); p>=Outfilename; --p) {
    if (*p=='.') {
      sprintf(MessageBuffer," (converted to %dHz).vgz",(VGMHeader.RecordingRate==50?60:50));
      strcpy(p,MessageBuffer);
      break;
    }
  }

  out=gzopen(Outfilename,"wb0");  // No compression, since I'll recompress it later

  // Write header... update it later
  gzwrite(out,&VGMHeader,sizeof(VGMHeader));
  gzseek(out,VGM_DATA_OFFSET,SEEK_SET);

  do {
    if (gztell(in)==VGMHeader.LoopOffset+LOOPDELTA) {  // Check to see if it's the loop point
      BytesAddedBeforeLoop=BytesAdded;
    }

    b0=gzgetc(in);
    switch (b0) {
    case VGM_GGST:  // GG stereo
    case VGM_PSG:  // PSG write
      b1=gzgetc(in);
      gzputc(out,b0);
      gzputc(out,b1);
      break;
    case VGM_YM2413:  // YM2413
    case VGM_YM2612_0:  // YM2612 port 0
    case VGM_YM2612_1:  // YM2612 port 1
    case VGM_YM2151:  // YM2151
      b1=gzgetc(in);
      b2=gzgetc(in);
      gzputc(out,b0);
      gzputc(out,b1);
      gzputc(out,b2);
      break;
    case 0x55:  // Reserved up to 0x5f
    case 0x56:  // All have 2 bytes of data
    case 0x57:  // which I discard :)
    case 0x58:
    case 0x59:
    case 0x5a:
    case 0x5b:
    case 0x5c:
    case 0x5d:
    case 0x5e:
    case 0x5f:
      b1=gzgetc(in);
      b2=gzgetc(in);
      break;
    
    
    // Not updated for VGM_PAUSE_BYTE, 0x7n
    case VGM_PAUSE_WORD:  // Wait n samples
      b1=gzgetc(in);
      b2=gzgetc(in);
      PauseLength=b1|(b2<<8);

      // Convert
      if (VGMHeader.RecordingRate==50) PauseLength=PauseLength*60/50;
      else PauseLength=PauseLength*50/60;

      // Write
      while (PauseLength>65535) {
        gzputc(out,VGM_PAUSE_WORD);
        gzputc(out,0xff);
        gzputc(out,0xff);
        BytesAdded+=3;
        PauseLength-=65535;
      }
      gzputc(out,VGM_PAUSE_WORD);
      gzputc(out,(PauseLength & 0xff));
      gzputc(out,(PauseLength >> 8));
      break;

    
    case VGM_PAUSE_60TH:  // Wait 1/60 s
      if (VGMHeader.RecordingRate==60) gzputc(out,VGM_PAUSE_50TH);
      else {
        sprintf(MessageBuffer,"Wait 1/60th command found in 50Hz file!");
        ShowMessage();
      }
      break;
    case VGM_PAUSE_50TH:  // Wait 1/50 s
      if (VGMHeader.RecordingRate==50) gzputc(out,VGM_PAUSE_60TH);
      else {
        sprintf(MessageBuffer,"Wait 1/50th command found in 60Hz file!");
        ShowMessage();
      }
      break;
    case VGM_END:  // End of sound data
      b0=EOF;  // break out of loop
      break;
    default:
      break;
    }
  } while (b0!=EOF);
  gzputc(out,VGM_END);

  // Copy GD3 tag
  if (VGMHeader.GD3Offset) {
    struct TGD3Header GD3Header;
    int i;
    int NewGD3Offset=gztell(out)-GD3DELTA;
    ShowStatus("Copying GD3 tag...");
    gzseek(in,VGMHeader.GD3Offset+GD3DELTA,SEEK_SET);
    gzread(in,&GD3Header,sizeof(GD3Header));
    gzwrite(out,&GD3Header,sizeof(GD3Header));
    for (i=0; i<GD3Header.Length; ++i) {  // Copy strings
      gzputc(out,gzgetc(in));
    }
    VGMHeader.GD3Offset=NewGD3Offset;
  }
  VGMHeader.EoFOffset=gztell(out)-EOFDELTA;
  gzclose(in);
  gzclose(out);

  VGMHeader.LoopOffset+=BytesAddedBeforeLoop;
  if (VGMHeader.GD3Offset) VGMHeader.GD3Offset+=BytesAdded;
  VGMHeader.EoFOffset+=BytesAdded;
  if (VGMHeader.RecordingRate==50) {
    VGMHeader.RecordingRate=60;
    VGMHeader.PSGClock=3546893;
  } else {
    VGMHeader.RecordingRate=50;
    VGMHeader.PSGClock=3579545;
  }

  // Amend it with the updated header
  WriteVGMHeader(Outfilename,VGMHeader);

  CountLength(Outfilename,0);  // fix lengths

  Compress(Currentfilename);

  ShowStatus("Rate conversion complete");

  sprintf(MessageBuffer,"File converted to\n%s",Outfilename);
  ShowMessage();

  free(Outfilename);
}

*/

void PasteUnicode() {
  HANDLE DataHandle;
  char textbuffer[2048]="";

  if (OpenClipboard(hWndMain)) {  // Open clipboard
    DataHandle=GetClipboardData(CF_UNICODETEXT);  // See if there's suitable data there
    if (DataHandle) {  // If there is...
      wchar *wp=GlobalLock(DataHandle);
      int i=0;
      while ((int)(*wp)) {
        if ((int)(*wp)<128) {
          strcat(textbuffer,(char*)*wp); // Nasty hack!
        } else {
          sprintf(textbuffer,"%s&#x%04x;",textbuffer,(int)(*wp));
        }
        wp++;
      }

      GlobalUnlock(DataHandle);
    } else { // No unicode on clipboard
      strcat(textbuffer,"No Unicode on clipboard");
    }

    CloseClipboard();
  }
  SetDlgItemText(GD3Wnd,edtPasteUnicode,textbuffer);
}

void CopyLengthsToClipboard() {
  HANDLE CopyForClipboard;
  char String[64]="",*p;
  int strlength;
  char MessageBuffer[1024];

  GetDlgItemText(GD3Wnd,edtGD3TitleEn,MessageBuffer,35);
  sprintf(String,"%-35s",MessageBuffer);
  GetDlgItemText(HeaderWnd,edtLengthTotal,MessageBuffer,5);
  if (!strlen(MessageBuffer)) return;  // quit if no length in box
  sprintf(String,"%s %s",String,MessageBuffer);
  GetDlgItemText(HeaderWnd,edtLengthLoop,MessageBuffer,5);
  sprintf(String,"%s   %s\r\n",String,MessageBuffer);

  strlength=(strlen(String)+1)*sizeof(char);
  if (!OpenClipboard(hWndMain)) return;
  EmptyClipboard();
    CopyForClipboard=GlobalAlloc(GMEM_DDESHARE,strlength);
  if (!CopyForClipboard) { CloseClipboard(); return; }
  p=GlobalLock(CopyForClipboard); 
  memcpy(p,String,strlength);
  GlobalUnlock(CopyForClipboard); 
  SetClipboardData(CF_TEXT,CopyForClipboard); 
  CloseClipboard();

  String[strlen(String)-2]='\0';
  sprintf(MessageBuffer,"Copied: \"%s\"",String);
  ShowStatus(MessageBuffer);
}


void ConvertDroppedFiles(HDROP HDrop) {
  int i,NumFiles,FilenameLength,NumConverted=0,Timer=GetTickCount();
  char *DroppedFilename,*p;

  // Get number of files dropped
  NumFiles=DragQueryFile(HDrop,0xFFFFFFFF,NULL,0);

  // Go through files
  for (i=0;i<NumFiles;++i) {
    // Get filename length
    FilenameLength=DragQueryFile(HDrop,i,NULL,0)+1;
    DroppedFilename=malloc(FilenameLength);  // Allocate memory for the filename
    DragQueryFile(HDrop,i,DroppedFilename,FilenameLength);  // Get filename of file

    // Get extension
    p=strrchr(DroppedFilename,'.');

	if(p) {
      if      ((_strcmpi(p,".gym")==0)&&ConverttoVGM(DroppedFilename,ftGYM)) NumConverted++;
      else if ((_strcmpi(p,".ssl")==0)&&ConverttoVGM(DroppedFilename,ftSSL)) NumConverted++;
      else if ((_strcmpi(p,".cym")==0)&&ConverttoVGM(DroppedFilename,ftCYM)) NumConverted++;
      else {
        p=strrchr(DroppedFilename,'\\')+1;
        AddConvertText("Unknown extension: \"%s\"\r\n",p);
	  }
	}
    free(DroppedFilename);  // deallocate buffer
  }

  AddConvertText("%d of %d file(s) successfully converted in %dms\r\n",NumConverted,NumFiles,GetTickCount()-Timer);

  DragFinish(HDrop);
}

LRESULT CALLBACK DialogProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

void MakeTabbedDialogue() {
  // Variables
  TC_ITEM NewTab;
  HWND TabCtrlWnd=GetDlgItem(hWndMain,tcMain);
  RECT TabDisplayRect,TabRect;
  int i;
  HIMAGELIST il;
  // Load images for tabs
  InitCommonControls(); // required before doing imagelist stuff
  il=ImageList_LoadImage(HInst,(LPCSTR)tabimages,16,0,RGB(255,0,255),IMAGE_BITMAP,LR_CREATEDIBSECTION);
  TabCtrl_SetImageList(TabCtrlWnd,il);
  // Add tabs
  NewTab.mask=TCIF_TEXT | TCIF_IMAGE;
  NewTab.pszText="Header";
  NewTab.iImage=0;
  TabCtrl_InsertItem(TabCtrlWnd,0,&NewTab);
  NewTab.pszText="Trim/optimise";
  NewTab.iImage=1;
  TabCtrl_InsertItem(TabCtrlWnd,1,&NewTab);
  NewTab.pszText="Data usage/strip";
  NewTab.iImage=2;
  TabCtrl_InsertItem(TabCtrlWnd,2,&NewTab);
  NewTab.pszText="GD3 tag";
  NewTab.iImage=3;
  TabCtrl_InsertItem(TabCtrlWnd,3,&NewTab);
  NewTab.pszText="Conversion";
  NewTab.iImage=4;
  TabCtrl_InsertItem(TabCtrlWnd,4,&NewTab);
  NewTab.pszText="More functions";
  NewTab.iImage=5;
  TabCtrl_InsertItem(TabCtrlWnd,5,&NewTab);
  // Get display rect
  GetWindowRect(TabCtrlWnd,&TabDisplayRect);
  GetWindowRect(hWndMain,&TabRect);
  OffsetRect(&TabDisplayRect,-TabRect.left-GetSystemMetrics(SM_CXDLGFRAME),-TabRect.top-GetSystemMetrics(SM_CYDLGFRAME)-GetSystemMetrics(SM_CYCAPTION));
  TabCtrl_AdjustRect(TabCtrlWnd,FALSE,&TabDisplayRect);
  
  // Create child windows (resource hog, I don't care)
  TabChildWnds[0]=CreateDialog(HInst,(LPCTSTR) DlgVGMHeader,hWndMain,DialogProc);
  TabChildWnds[1]=CreateDialog(HInst,(LPCTSTR) DlgTrimming, hWndMain,DialogProc);
  TabChildWnds[2]=CreateDialog(HInst,(LPCTSTR) DlgStripping,hWndMain,DialogProc);
  TabChildWnds[3]=CreateDialog(HInst,(LPCTSTR) DlgGD3,      hWndMain,DialogProc);
  TabChildWnds[4]=CreateDialog(HInst,(LPCTSTR) DlgConvert,  hWndMain,DialogProc);
  TabChildWnds[5]=CreateDialog(HInst,(LPCTSTR) DlgMisc,     hWndMain,DialogProc);
  // Enable WinXP styles
  {
    HINSTANCE dllinst=LoadLibrary("uxtheme.dll");
    if (dllinst) {
      FARPROC EnableThemeDialogTexture =GetProcAddress(dllinst,"EnableThemeDialogTexture"),
            IsThemeDialogTextureEnabled=GetProcAddress(dllinst,"IsThemeDialogTextureEnabled");
      if (
        (IsThemeDialogTextureEnabled)&&(EnableThemeDialogTexture)&& // All functions found
        (IsThemeDialogTextureEnabled(hWndMain))) { // and app is themed
        for (i=0;i<NumTabChildWnds;++i) EnableThemeDialogTexture(TabChildWnds[i],6); // then draw pages with theme texture
      }
      FreeLibrary(dllinst);
    }
  }
  // Put them in the right place, and hide them
  for (i=0;i<NumTabChildWnds;++i) 
    SetWindowPos(TabChildWnds[i],HWND_TOP,TabDisplayRect.left,TabDisplayRect.top,TabDisplayRect.right-TabDisplayRect.left,TabDisplayRect.bottom-TabDisplayRect.top,SWP_HIDEWINDOW);
  // Show the first one, though
  SetWindowPos(TabChildWnds[0],HWND_TOP,0,0,0,0,SWP_NOSIZE|SWP_NOMOVE|SWP_SHOWWINDOW);
}


// parses items, splitting at separators
// fills combobox id in parent with strings
void FillComboBox(HWND parent,int id,char *items,char *separators) {
  char *buf,*p;
  buf=malloc(strlen(items)+1);
  strcpy(buf,items); // I need a writeable local copy
  p=strtok(buf,separators);

  while(p){
    SendDlgItemMessage(parent,id,CB_ADDSTRING,0,(LPARAM)p);
    p=strtok(NULL,separators);
  }

  free(buf);
}

LRESULT CALLBACK DialogProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
  switch (message) {  // process messages
  case WM_INITDIALOG:  // Initialise dialogue
    if (hWndMain!=NULL) return FALSE;  // nothing for child windows
    hWndMain=hWnd;  // remember our window handle
    //SetClassLong(hWndMain,GCL_HICON,(LONG)LoadIcon(HInst,MAKEINTRESOURCE(MAINICON)));  // Give it the icon
    SetDlgItemText(hWndMain,edtFileName,"Drop a file onto the window to load");
    SetWindowText(hWndMain,ProgName);
    MakeTabbedDialogue();
    // Fill combo box - see below for Japanese translations
    FillComboBox(GD3Wnd,cbGD3SystemEn,"Sega Master System,Sega Game Gear,Sega Master System / Game Gear,Sega Mega Drive / Genesis,Sega Game 1000,Sega Computer 3000,Sega System 16,Capcom Play System 1,Colecovision,BBC Model B,BBC Model B+,BBC Master 128,Custom...",",");
    FillComboBox(HeaderWnd,edtPlaybackRate,"0 (unknown),50 (PAL),60 (NTSC)",",");
    FillComboBox(HeaderWnd,edtPSGClock,"0 (disabled),3546893 (PAL),3579545 (NTSC),4000000 (BBC)",",");
    FillComboBox(HeaderWnd,edtYM2413Clock,"0 (disabled),3546893 (PAL),3579545 (NTSC)",",");
    FillComboBox(HeaderWnd,edtYM2612Clock,"0 (disabled),7600489 (PAL),7670453 (NTSC)",",");
    FillComboBox(HeaderWnd,edtYM2151Clock,"0 (disabled),7670453 (NTSC)",",");

    FillComboBox(HeaderWnd,edtPSGFeedback,"0 (disabled),0x0009 (Sega VDP),0x0003 (discrete chip)",",");
    FillComboBox(HeaderWnd,edtPSGSRWidth,"0 (disabled),16 (Sega VDP),15 (discrete chip)",",");

    // Focus 1st control
    SetActiveWindow(hWndMain);
    SetFocus(GetDlgItem(hWndMain,edtFileName));
    return TRUE;
  case WM_CLOSE:  // Window is being asked to close ([x], Alt+F4, etc)
    PostQuitMessage(0);
    return TRUE;
  case WM_DESTROY:  // Window is being destroyed
    PostQuitMessage(0);
    return TRUE;
  case WM_DROPFILES:  // File dropped
    if (hWnd==ConvertWnd) ConvertDroppedFiles((HDROP)wParam);
    else {
      char *DroppedFilename;
      int FilenameLength=DragQueryFile((HDROP)wParam,0,NULL,0)+1;
      DroppedFilename=malloc(FilenameLength);  // Allocate memory for the filename
      DragQueryFile((HDROP)wParam,0,DroppedFilename,FilenameLength);  // Get filename of first file, discard the rest
      DragFinish((HDROP)wParam);  // Tell Windows I've finished
      LoadFile(DroppedFilename);
      free(DroppedFilename);  // deallocate buffer
    }
    return TRUE;
  case WM_COMMAND:
    switch (LOWORD(wParam)) {
    case btnUpdateHeader:
      UpdateHeader();
      Compress(Currentfilename);
      FixExt(Currentfilename);  // in case I've VGM-VGZed
      LoadFile(Currentfilename);
      break;
    case btnCheckLengths:
      CheckLengths(Currentfilename,1);
      LoadFile(Currentfilename);
      break;
    case btnTrim: 
      {
        int Start,Loop=-1,End;
        BOOL b1,b2,b3=TRUE;
        Start=GetDlgItemInt(TrimWnd,edtTrimStart,&b1,FALSE);
        End=GetDlgItemInt(TrimWnd,edtTrimEnd,&b2,FALSE);
        if (IsDlgButtonChecked(TrimWnd,cbLoop)) // want looping
          Loop=GetDlgItemInt(TrimWnd,edtTrimLoop,&b3,FALSE);

        if (!b1 || !b2 || !b3) {  // failed to get values
          ShowError("Invalid edit points!");
          break;
        }
        Trim(Currentfilename,Start,Loop,End,FALSE,TRUE);
      }
      break;
    case btnWriteToText:
      WriteToText(Currentfilename);
      break;
    case btnOptimise:
      Optimize(Currentfilename);
      Compress(Currentfilename);
      FixExt(Currentfilename);
      LoadFile(Currentfilename);
      break;
    case btnDecompress:
      if(Decompress(Currentfilename) && FixExt(Currentfilename)) LoadFile(Currentfilename);
      break;
    case btnRoundTimes:
      {
        int Time,i,FrameLength;
        BOOL b;
        const int Edits[3]={edtTrimStart,edtTrimLoop,edtTrimEnd};
        if (!CurrentFileVGMHeader.RecordingRate) break;  // stop if rate = 0
        FrameLength=44100/CurrentFileVGMHeader.RecordingRate;
        for (i=0;i<3;++i) {
          Time=GetDlgItemInt(TrimWnd,Edits[i],&b,FALSE);
          if (b) SetDlgItemInt(TrimWnd,Edits[i],ROUND((double)Time/FrameLength)*FrameLength,FALSE);
        }
      }
      break;
    case btnPlayFile:
      ShellExecute(hWndMain,"Play",Currentfilename,NULL,NULL,SW_NORMAL);
      break;
    case btnUpdateGD3:
      UpdateGD3();
      Compress(Currentfilename);
      FixExt(Currentfilename);  // in case I've VGM-VGZed
      LoadFile(Currentfilename);
      break;
    case btnGD3Clear:
      ClearGD3Strings();
      break;
    case btnSelectAll:
      ChangeCheckBoxes(1);
      break;
    case btnSelectNone:
      ChangeCheckBoxes(2);
      break;
    case btnSelectInvert:
      ChangeCheckBoxes(3);
      break;
    case btnSelectGuess:
      ChangeCheckBoxes(4);
      break;
    case btnStrip:
      StripChecked(Currentfilename);
      break;
    case cbPSGTone:
      {
        int i;
        const int Checked=IsDlgButtonChecked(StripWnd,cbPSGTone);
        for (i=0;i<3;++i) if (IsWindowEnabled(GetDlgItem(StripWnd,PSGCheckBoxes[i]))) CheckDlgButton(StripWnd,PSGCheckBoxes[i],Checked);
      }
      break;
    case cbYM2413Tone:
      {
        int i;
        const int Checked=IsDlgButtonChecked(StripWnd,cbYM2413Tone);
        for (i=0;i<9;++i) if (IsWindowEnabled(GetDlgItem(StripWnd,YM2413CheckBoxes[i]))) CheckDlgButton(StripWnd,YM2413CheckBoxes[i],Checked);
      }
      break;
    case cbYM2413Percussion:
      {
        int i;
        const int Checked=IsDlgButtonChecked(StripWnd,cbYM2413Percussion);
        for (i=9;i<14;++i) if (IsWindowEnabled(GetDlgItem(StripWnd,YM2413CheckBoxes[i]))) CheckDlgButton(StripWnd,YM2413CheckBoxes[i],Checked);
      }
      break;
    case btnRateDetect:
		{
		  int i=DetectRate(Currentfilename);
		  if(i) SetDlgItemInt(HeaderWnd,edtPlaybackRate,i,FALSE); // TODO: make sure VGM version is set high enough when updating header
		}
      break;
    case btnPasteUnicode:
      PasteUnicode();
      break;
    case cbGD3SystemEn:
      if (HIWORD(wParam)!=CBN_SELENDOK) break;
      switch (SendDlgItemMessage(GD3Wnd,cbGD3SystemEn,CB_GETCURSEL,0,0)) {
      case 0: SetDlgItemText(GD3Wnd,edtGD3SystemJp,"&#x30bb;&#x30ac;&#x30de;&#x30b9;&#x30bf;&#x30fc;&#x30b7;&#x30b9;&#x30c6;&#x30e0;");  break; // Sega Master System
      case 1: SetDlgItemText(GD3Wnd,edtGD3SystemJp,"&#x30bb;&#x30ac;&#x30b2;&#x30fc;&#x30e0;&#x30ae;&#x30a2;");  break; // Sega Game Gear
      case 2: SetDlgItemText(GD3Wnd,edtGD3SystemJp,"&#x30bb;&#x30ac;&#x30de;&#x30b9;&#x30bf;&#x30fc;&#x30b7;&#x30b9;&#x30c6;&#x30e0; / &#x30b2;&#x30fc;&#x30e0;&#x30ae;&#x30a2;");  break; // Sega Master System / Game Gear
      case 3: SetDlgItemText(GD3Wnd,edtGD3SystemJp,"&#x30bb;&#x30ac;&#x30e1;&#x30ac;&#x30c9;&#x30e9;&#x30a4;&#x30d6;");  break; // Sega Megadrive (Japanese name)
      default:SetDlgItemText(GD3Wnd,edtGD3SystemJp,"");  break;
      }
      break;
    case btnCopyLengths:
      CopyLengthsToClipboard();
      break;
    case btnRemoveGD3:
      RemoveGD3(Currentfilename);
      LoadFile(Currentfilename);
      break;
    case btnRemoveOffsets:
      RemoveOffset(Currentfilename);
      LoadFile(Currentfilename);
      break;
    case btnOptimiseVGMData:
//      OptimiseVGMData(Currentfilename);
      ShowError("TODO: OptimiseVGMData() fixing");
      LoadFile(Currentfilename);
      break;
    case btnOptimisePauses:
      OptimiseVGMPauses(Currentfilename);
      LoadFile(Currentfilename);
      break;
    case btnTrimOnly:
      {
        int Start,Loop=-1,End;
        BOOL b1,b2,b3=TRUE;
        Start=GetDlgItemInt(TrimWnd,edtTrimStart,&b1,FALSE);
        End=GetDlgItemInt(TrimWnd,edtTrimEnd,&b2,FALSE);
        if (IsDlgButtonChecked(TrimWnd,cbLoop)) // want looping
          Loop=GetDlgItemInt(TrimWnd,edtTrimLoop,&b3,FALSE);

        if (!b1 || !b2 || !b3) {  // failed to get values
          ShowError("Invalid edit points!");
          break;
        }

        if(IsDlgButtonChecked(TrimWnd,cbLogTrims)) LogTrim(Currentfilename,Start,Loop,End);

        NewTrim(Currentfilename,Start,Loop,End);
      }
      break;
    case btnCompress:
      Compress(Currentfilename);
      FixExt(Currentfilename);
      LoadFile(Currentfilename);
      break;
    case btnNewTrim:
      {
        int Start,Loop=-1,End;
        BOOL b1,b2,b3=TRUE;
        Start=GetDlgItemInt(TrimWnd,edtTrimStart,&b1,FALSE);
        End=GetDlgItemInt(TrimWnd,edtTrimEnd,&b2,FALSE);
        if (IsDlgButtonChecked(TrimWnd,cbLoop)) // want looping
          Loop=GetDlgItemInt(TrimWnd,edtTrimLoop,&b3,FALSE);

        if (!b1 || !b2 || !b3) {  // failed to get values
          ShowError("Invalid edit points!");
          break;
        }
        NewTrim(Currentfilename,Start,Loop,End);
        RemoveOffset(Currentfilename);
//        OptimiseVGMData(Currentfilename);
        OptimiseVGMPauses(Currentfilename);
          
      }
      break;
    case btnGetCounts:
      CheckWriteCounts(Currentfilename);
      break;
    case btnRoundToFrames:
      RoundToFrameAccurate(Currentfilename);
      break;
    } // end switch(LOWORD(wParam))
    {
      // Stuff to happen after every message/button press (?!): update "all" checkboxes for stripping
      int i,Checked,Total;
      Checked=0; Total=0; for (i=0;i<3; ++i) {Checked+=IsDlgButtonChecked(StripWnd,   PSGCheckBoxes[i]); Total+=IsWindowEnabled(GetDlgItem(StripWnd,   PSGCheckBoxes[i]));} CheckDlgButton(StripWnd,cbPSGTone     ,(Checked==Total)&&Total);
      Checked=0; Total=0; for (i=0;i<9; ++i) {Checked+=IsDlgButtonChecked(StripWnd,YM2413CheckBoxes[i]); Total+=IsWindowEnabled(GetDlgItem(StripWnd,YM2413CheckBoxes[i]));} CheckDlgButton(StripWnd,cbYM2413Tone     ,(Checked==Total)&&Total);
      Checked=0; Total=0; for (i=9;i<14;++i) {Checked+=IsDlgButtonChecked(StripWnd,YM2413CheckBoxes[i]); Total+=IsWindowEnabled(GetDlgItem(StripWnd,YM2413CheckBoxes[i]));} CheckDlgButton(StripWnd,cbYM2413Percussion,(Checked==Total)&&Total);
    }
    return TRUE; // WM_COMMAND handled
  case WM_NOTIFY:
    switch (LOWORD(wParam)) {
    case tcMain:
      switch (((LPNMHDR)lParam)->code) {
      case TCN_SELCHANGING:  // hide current window
        SetWindowPos(TabChildWnds[TabCtrl_GetCurSel(GetDlgItem(hWndMain,tcMain))],HWND_TOP,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_HIDEWINDOW);
        break;
      case TCN_SELCHANGE:  // show current window
        {
          int i=TabCtrl_GetCurSel(GetDlgItem(hWndMain,tcMain));
          SetWindowPos(TabChildWnds[i],HWND_TOP,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_SHOWWINDOW);
          SetFocus(TabChildWnds[i]);
        }
        break;
      }
      break;
    } // end switch (LOWORD(wParam))
    return TRUE; // WM_NOTIFY handled
  }
  return FALSE;    // return FALSE to signify message not processed
}

/* // Old method, using Windows' built-in message handlers
int APIENTRY WinMain(HINSTANCE hInstance,HINSTANCE hPrevInstance,LPSTR lpCmdLine,int nCmdShow) {
  HInst=hInstance;
  DialogBox(hInstance,(LPCTSTR) MAINDIALOGUE,NULL,DialogProc);  // Open dialog
  if (GD3Strings) free(GD3Strings);
  return 0;
}*/


// I only handle commandlines that look like
// "\"path to file\"blahetcwhatever"
// ie. the filename must be quote-wrapped and I don't handle anything more than that
void HandleCommandline(char *commandline) {
  char *start,*end; // pointers to start and end of filename section of commandline

  if(!commandline || !*commandline) return; // do nothing for null pointer or empty string

  start=strchr(commandline,'"')+1;
  if(start){
    end=strchr(start,'"');
	if(end){
      *end=0;
      if(FileExistsQuiet(start)) LoadFile(start);
	}
  }
}

void DoCtrlTab(void) {
  HWND TabCtrlWnd=GetDlgItem(hWndMain,tcMain);
  int currenttab=TabCtrl_GetCurFocus(TabCtrlWnd);

  if(GetKeyState(VK_SHIFT)<0) currenttab--;
  else currenttab++;

  if(currenttab==-1) currenttab=NumTabChildWnds-1;
  else if(currenttab==NumTabChildWnds) currenttab=0;

  TabCtrl_SetCurFocus(TabCtrlWnd,currenttab);
}

int APIENTRY WinMain(HINSTANCE hInstance,HINSTANCE hPrevInstance,LPSTR lpCmdLine,int nCmdShow) {
  MSG msg;

  HInst=hInstance;
  CreateDialog(hInstance,(LPCTSTR) MAINDIALOGUE,NULL,DialogProc);  // Open dialog
  HandleCommandline(lpCmdLine);
  ShowWindow(hWndMain,nCmdShow);
  while (GetMessage(&msg, NULL, 0, 0)) {
    // you can use TranslateAccelerator if you prefer
    if (IsChild(hWndMain,msg.hwnd) && msg.message == WM_KEYDOWN && msg.wParam == VK_TAB && GetKeyState(VK_CONTROL) < 0) {
      DoCtrlTab();
	} else if (IsDialogMessage(hWndMain, &msg)) {
     /* IsDialogMessage handled the message */
	} else {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
	}
  }

  if (GD3Strings) free(GD3Strings);
  return 0;
}

