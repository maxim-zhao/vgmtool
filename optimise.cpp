#include "optimise.h"
#include <cstdio>
#include <zlib.h>
#include "vgm.h"
#include "trim.h"
#include "gd3.h"
#include "utils.h"
#include "gui.h"

//----------------------------------------------------------------------------------------------
// Pause optimiser
//----------------------------------------------------------------------------------------------
BOOL OptimiseVGMPauses(char* filename)
{
    struct VGMHeader VGMHeader;
    char b0, b1, b2;
    int PauseLength = 0;

    if (!FileExists(filename)) return FALSE;

    // Open input file
    gzFile in = gzopen(filename, "rb");

    // Read its VGM header
    if (!ReadVGMHeader(in, &VGMHeader,FALSE))
    {
        gzclose(in);
        return FALSE;
    }

    long int OldLoopOffset = VGMHeader.LoopOffset + LOOPDELTA;

    // Make the output filename...
    char* outfilename = make_temp_filename(filename);

    // ...open it...
    gzFile out = gzopen(outfilename, "wb0");

    // ...skip to the data section...
    gzseek(in,VGM_DATA_OFFSET,SEEK_SET);
    gzseek(out,VGM_DATA_OFFSET,SEEK_SET);

    // ...and parse the input file
    // Go through the file; if it's a pause, add it to the total; if it's data, write the current total and zero it
    do
    {
        // Update loop point
        // Write any remaining pauyse being buffered first, though
        if (gztell(in) == OldLoopOffset)
        {
            write_pause(out, PauseLength);
            PauseLength = 0;
            VGMHeader.LoopOffset = gztell(out) - LOOPDELTA;
        }

        b0 = gzgetc(in);
        switch (b0)
        {
        case VGM_GGST: // GG stereo
        case VGM_PSG: // PSG write
            write_pause(out, PauseLength);
            PauseLength = 0;
            b1 = gzgetc(in);
            gzputc(out, b0);
            gzputc(out, b1);
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
            write_pause(out, PauseLength);
            PauseLength = 0;
            b1 = gzgetc(in);
            b2 = gzgetc(in);
            gzputc(out, b0);
            gzputc(out, b1);
            gzputc(out, b2);
            break;
        case VGM_PAUSE_WORD: // Wait n samples
            b1 = gzgetc(in);
            b2 = gzgetc(in);
            PauseLength += MAKEWORD(b1, b2);
            break;
        case VGM_PAUSE_60TH: // Wait 1/60 s
            PauseLength += LEN60TH;
            break;
        case VGM_PAUSE_50TH: // Wait 1/50 s
            PauseLength += LEN50TH;
            break;
        //    case VGM_PAUSE_BYTE:  // Wait n samples
        //      b1=gzgetc(in);
        //      PauseLength+=b1;
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
            PauseLength += (b0 & 0xf) + 1;
            break;
        case VGM_END: // End of sound data
            write_pause(out, PauseLength);
            PauseLength = 0;
            b0 = EOF; // break out of loop
            break;
        default:
            break;
        } // end switch
    }
    while (b0 != EOF);

    // At end:
    // 1. Write EOF mrker
    gzputc(out,VGM_END);
    // 2. Copy GD3 tag
    if (VGMHeader.GD3Offset)
    {
        struct TGD3Header GD3Header;
        int NewGD3Offset = gztell(out) - GD3DELTA;
        gzseek(in, VGMHeader.GD3Offset + GD3DELTA,SEEK_SET);
        gzread(in, &GD3Header, sizeof(GD3Header));
        gzwrite(out, &GD3Header, sizeof(GD3Header));
        for (int i = 0; i < GD3Header.length; ++i) gzputc(out,gzgetc(in));
        VGMHeader.GD3Offset = NewGD3Offset;
    }
    // 3. Fill in VGM header
    VGMHeader.EoFOffset = gztell(out) - EOFDELTA;
    // LoopOffset updated while optimising

    gzclose(out);

    write_vgm_header(outfilename, VGMHeader);

    // Clean up
    gzclose(in);

    MyReplaceFile(filename, outfilename);

    free(outfilename);

    return TRUE;
}


//----------------------------------------------------------------------------------------------
// PSG offset (small freq value, volume on) remover
// Returns number of offsets removed
//----------------------------------------------------------------------------------------------
int RemoveOffset(char* filename)
{
    struct VGMHeader VGMHeader;
    signed int b0, b1, b2;
    BOOL SilencedChannels[3] = {FALSE,FALSE,FALSE};
    unsigned short int PSGRegisters[8] = {0, 0xf, 0, 0xf, 0, 0xf, 0, 0xf};
    int PSGLatchedRegister = 0;

    long int NewLoopOffset = 0;
    int NumOffsetsRemoved = 0;
    int NoiseCh2 = 0;

    if (!FileExists(filename)) return 0;

    gzFile in = gzopen(filename, "rb");

    // Read header
    if (!ReadVGMHeader(in, &VGMHeader,FALSE))
    {
        gzclose(in);
        return FALSE;
    }

    gzseek(in,VGM_DATA_OFFSET,SEEK_SET);

    char* outfilename = make_temp_filename(filename);

    gzFile out = gzopen(outfilename, "wb0"); // No compression, since I'll recompress it later

    // copy header... update it later
    gzwrite(out, &VGMHeader, sizeof(VGMHeader));
    gzseek(out,VGM_DATA_OFFSET,SEEK_SET);

    // Process file
    do
    {
        if ((VGMHeader.LoopOffset) && (gztell(in) == VGMHeader.LoopOffset + LOOPDELTA))
            NewLoopOffset = gztell(out) -
                LOOPDELTA;
        b0 = gzgetc(in);
        switch (b0)
        {
        case VGM_GGST: // GG stereo (1 byte data)
            // Just pass it through
            b1 = gzgetc(in);
            gzputc(out,VGM_GGST);
            gzputc(out, b1);
            break;
        case VGM_PSG: // PSG write (1 byte data)
            b1 = gzgetc(in);

            if (b1 & 0x80)
            {
                // Latch/data byte   %1 cc t dddd
                PSGLatchedRegister = ((b1 >> 4) & 0x07);
                PSGRegisters[PSGLatchedRegister] =
                    (PSGRegisters[PSGLatchedRegister] & 0x3f0) // zero low 4 bits
                    | (b1 & 0xf); // and replace with data
            }
            else
            {
                // Data byte
                if (!(PSGLatchedRegister % 2) && (PSGLatchedRegister < 5))
                    // Tone register
                    PSGRegisters[PSGLatchedRegister] =
                        (PSGRegisters[PSGLatchedRegister] & 0x00f) // zero high 6 bits
                        | ((b1 & 0x3f) << 4); // and replace with data
                else
                    // Other register
                    PSGRegisters[PSGLatchedRegister] = b1 & 0x0f; // Replace with data
            }
        // Analyse:
            switch (PSGLatchedRegister)
            {
            case 0:
            case 2:
            case 4: // Tone registers
                if ((PSGRegisters[PSGLatchedRegister] < PSGCutoff) && // If freq is too low
                    !((PSGLatchedRegister == 4) && NoiseCh2) // and it's not tone2 controlling noise
                )
                {
                    // then silence that channel
                    if (!SilencedChannels[PSGLatchedRegister / 2])
                    {
                        // If channel hasn't already been silenced
                        ++NumOffsetsRemoved;
                        gzputc(out,VGM_PSG);
                        gzputc(out, static_cast<char>(0x9f | // %10011111
                                (PSGLatchedRegister & 0x6) << 4) // %0cc00000
                        );
                        // Remember I've done it
                        SilencedChannels[PSGLatchedRegister / 2] = TRUE;
                        // Output zero frequency
                        gzputc(out,VGM_PSG);
                        gzputc(out, static_cast<char>(0x80 | (PSGLatchedRegister << 4)));
                        gzputc(out,VGM_PSG);
                        gzputc(out, 0);
                    }
                }
                else
                {
                    // Channel shouldn't be silent
                    if (SilencedChannels[PSGLatchedRegister / 2])
                    {
                        // If I've previously silenced this channel
                        // then restore the volume
                        gzputc(out,VGM_PSG);
                        gzputc(out, static_cast<char>(0x90 | // %10010000
                                (PSGLatchedRegister & 0x6) << 4) | // %0cc00000
                            PSGRegisters[PSGLatchedRegister + 1] // %0000vvvv
                        );
                        SilencedChannels[PSGLatchedRegister / 2] = FALSE;
                    }
                    // Write the frequency bytes
                    gzputc(out,VGM_PSG);
                    gzputc(out,
                        static_cast<char>(0x80 | (PSGLatchedRegister << 4) | PSGRegisters[PSGLatchedRegister] & 0xf));
                    gzputc(out,VGM_PSG);
                    gzputc(out, static_cast<char>(PSGRegisters[PSGLatchedRegister] >> 4));
                }
                break;
            case 6: // Noise
                // Detect if it's ch2 noise
                NoiseCh2 = ((PSGRegisters[6] & 0x3) == 0x3);
            // Pass through
                gzputc(out,VGM_PSG);
                gzputc(out, b1);
                break;
            default: // Volume
                if ((PSGLatchedRegister / 2 < 3) && // Tone channel
                    SilencedChannels[PSGLatchedRegister / 2]) // Silenced
                    break; // Don't write
            // Pass through
                gzputc(out,VGM_PSG);
                gzputc(out, b1);
                break;
            } // end switch
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
            b1 = gzgetc(in);
            b2 = gzgetc(in);
            gzputc(out, b0);
            gzputc(out, b1);
            gzputc(out, b2);
            break;
        case VGM_PAUSE_WORD: // Wait n samples
            b1 = gzgetc(in);
            b2 = gzgetc(in);
            gzputc(out,VGM_PAUSE_WORD);
            gzputc(out, b1);
            gzputc(out, b2);
            break;
        case VGM_PAUSE_60TH: // Wait 1/60 s
            gzputc(out,VGM_PAUSE_60TH);
            break;
        case VGM_PAUSE_50TH: // Wait 1/50 s
            gzputc(out,VGM_PAUSE_50TH);
            break;
        //    case VGM_PAUSE_BYTE:  // Wait n samples
        //      b1=gzgetc(in);
        //      gzputc(out,VGM_PAUSE_BYTE);
        //      gzputc(out,b1);
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
            gzputc(out, b0);
            break;
        case VGM_END: // End of sound data
            gzputc(out,VGM_END);
            break;
        default:
            break;
        }
    }
    while ((b0 != EOF) && (b0 != VGM_END));

    // Then copy the GD3 over
    if (VGMHeader.GD3Offset)
    {
        struct TGD3Header GD3Header;
        int NewGD3Offset = gztell(out) - GD3DELTA;
        gzseek(in, VGMHeader.GD3Offset + GD3DELTA,SEEK_SET);
        gzread(in, &GD3Header, sizeof(GD3Header));
        gzwrite(out, &GD3Header, sizeof(GD3Header));
        for (int i = 0; i < GD3Header.length; ++i)
        {
            // Copy strings
            gzputc(out,gzgetc(in));
        }
        VGMHeader.GD3Offset = NewGD3Offset;
    }
    VGMHeader.LoopOffset = NewLoopOffset;
    VGMHeader.EoFOffset = gztell(out) - EOFDELTA;

    gzclose(in);
    gzclose(out);

    // Amend it with the updated header
    write_vgm_header(outfilename, VGMHeader);

    // Overwrite original with the new one
    MyReplaceFile(filename, outfilename);

    free(outfilename);

    return NumOffsetsRemoved;
}

/*
  Note to self:

  New optimisation technique:

  Store current, and last-written, state information in structs.
  
  Need to store edge-case information like PSG noise dirty flag, FM key UDU/DUD

  Have a function writedata(struct &last, struct &current) which will
  (1) write only the data needed to change from last to current
  (2) copy current to last

  Maybe further segment it with a function like

  AddPSGWrite(struct &current,char data)
  AddYM2413Write(etc)

  The struct will contain all the state information the chip needs, eg. latched register

  Thus the details are moved out of the main loop, amking it easier to read and the optimiser
  can deal with inlining it since it's only called once.

  Also do this when writing the loop initialisation at the end, so it does not write any unneeded state information.

  The start of the file needs full initialisation, though. Let *last==NULL signal that.
*/


/*
//----------------------------------------------------------------------------------------------
// Data optimiser
//----------------------------------------------------------------------------------------------
// TODO: fix!
// TODO: optimise 1-byte PSG frequency changes (where the change is only in the high 4 bits)
// Test: Golvellius - Dina near the start
BOOL OptimiseVGMData(char *filename) {
  gzFile in,out;
  struct VGMHeader VGMHeader;
  struct TSystemState CurrentState;
  struct TSystemState LastWrittenState;
  char *Outfilename;
  int b0,b1,b2;
  long int OldLoopOffset;
  int i,PauseLength=0;
  BOOL NoiseUpdated=FALSE;

  if (!FileExists(filename)) return FALSE;

  // Open input file
  in=gzopen(filename,"rb");

  // Read its VGM header
  gzread(in,&VGMHeader,sizeof(VGMHeader));
  OldLoopOffset=VGMHeader.LoopOffset+LOOPDELTA;

  // Parse input file to see what chips are used
  GetUsedChips(in,&CurrentState.UsesPSG,&CurrentState.UsesYM2413,&CurrentState.UsesYM2612,&CurrentState.UsesYM2151,&CurrentState.UsesReserved);

  // It's OK then. Let's make the output filename...
  Outfilename=MakeSuffixedFilename(filename,"optimised");
  
  // ...open it...
  out=gzopen(Outfilename,"wb0");

  // ...skip to the data section...
  gzseek(in,VGM_DATA_OFFSET,SEEK_SET);
  gzseek(out,VGM_DATA_OFFSET,SEEK_SET);

  // ...initialise state to default values...
  ResetState(&CurrentState);
  ResetState(&LastWrittenState);

  // ...and parse the input file
  do {
    // Update loop point
    if (gztell(in)==OldLoopOffset) VGMHeader.LoopOffset=gztell(out)-LOOPDELTA;

    b0=gzgetc(in);
    switch (b0) {
    case VGM_GGST:  // GG stereo
      CurrentState.PSGState.GGStereo=gzgetc(in);
      break;
    case VGM_PSG:  // PSG write
      b1=gzgetc(in);
      if (b1&0x80) {
        CurrentState.PSGState.LatchedRegister=((b1>>4)&0x07);
        CurrentState.PSGState.Registers[CurrentState.PSGState.LatchedRegister]=(CurrentState.PSGState.Registers[CurrentState.PSGState.LatchedRegister]&0x3f0)|(b1&0xf);
      } else {
        if (!(CurrentState.PSGState.LatchedRegister%2)&&(CurrentState.PSGState.LatchedRegister<5)) CurrentState.PSGState.Registers[CurrentState.PSGState.LatchedRegister]=(CurrentState.PSGState.Registers[CurrentState.PSGState.LatchedRegister]&0x00f)|((b1&0x3f)<<4);
        else CurrentState.PSGState.Registers[CurrentState.PSGState.LatchedRegister]=b1&0x0f;
      }
      if (CurrentState.PSGState.LatchedRegister==6) NoiseUpdated=TRUE;
      break;
    case VGM_YM2413:  // YM2413
      b1=gzgetc(in);
      b2=gzgetc(in);
      if (b1<=0x38) CurrentState.YM2413State.Registers[b1]=b2&YM2413ValidBits[b1];
      break;
    case VGM_YM2612_0:  // YM2612 port 0
    case VGM_YM2612_1:  // YM2612 port 1
    case VGM_YM2151:  // YM2151
      b1=gzgetc(in);
      b2=gzgetc(in);
      // TODO: keep track of states for these chips
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
      PauseLength=MAKEWORD(b1,b2);
      break;
    case VGM_PAUSE_60TH:  // Wait 1/60 s
      PauseLength=LEN60TH;
      break;
    case VGM_PAUSE_50TH:  // Wait 1/50 s
      PauseLength=LEN50TH;
      break;
    case VGM_PAUSE_BYTE:
      b1=gzgetc(in);
      PauseLength=b1;
      break;
    case 0x70: case 0x71: case 0x72: case 0x73: case 0x74: case 0x75: case 0x76: case 0x77:
    case 0x78: case 0x79: case 0x7a: case 0x7b: case 0x7c: case 0x7d: case 0x7e: case 0x7f: // Wait 1-16 samples
      PauseLength=(b0 & 0xf)+1;
      break;
    case VGM_END:  // End of sound data
      b0=EOF;  // break out of loop
      break;
    default:
      break;
    }  // end switch

    if ((PauseLength)||(b0==EOF)) {
      // We have got to a pause, or the end of the file, so it's time to write the changes since the last write
      // See WriteState above for simpler, commented version

      // First PSG
      if (CurrentState.UsesPSG) {
        // Write in groups, not register order
        // 1. Tone channel frequencies
        for (i=0;i<5;i+=2) if ((CurrentState.PSGState.Registers[i]!=LastWrittenState.PSGState.Registers[i])||FirstState) {
          gzputc(out,VGM_PSG);
          gzputc(out,
            0x80|                    // Marker    %1-------
            (i<<4)|                    // Channel/type  %-cct---- (t=0)
            (CurrentState.PSGState.Registers[i]&0x0f)  // Data      %----dddd
          );
          gzputc(out,VGM_PSG);
          gzputc(out,
            (CurrentState.PSGState.Registers[i]>>4)&0x3f  // Data    %0-dddddd
          );
        }
        // 2. Noise
        if (NoiseUpdated||FirstState) {
          gzputc(out,VGM_PSG);
          gzputc(out,0xe0|(CurrentState.PSGState.Registers[6]&0x07));
        }
        // 3. Volumes (regs 1,3,5,7)
        for (i=1;i<8;i+=2) if ((CurrentState.PSGState.Registers[i]!=LastWrittenState.PSGState.Registers[i])||FirstState) {
          gzputc(out,VGM_PSG);
          gzputc(out,0x80|(i<<4)|(CurrentState.PSGState.Registers[i]&0x0f));
        }
        // 4. GG stereo
        if ((CurrentState.PSGState.GGStereo!=LastWrittenState.PSGState.GGStereo)||FirstState) {
          gzputc(out,VGM_GGST);
          gzputc(out,CurrentState.PSGState.GGStereo);
        }
      }

      // Then YM2413
      if (CurrentState.UsesYM2413) {
        for (i=0;i<YM2413NumRegs;++i)
        if (
          (
            (CurrentState.YM2413Regs[i]!=LastWrittenState.YM2413Regs[i])
            ||
            (FirstState)
          )&&
          (YM2413ValidBits[i])
        ) {
          gzputc(out,VGM_YM2413);
          gzputc(out,i);
          gzputc(out,CurrentState.YM2413Regs[i]);
        }
      }
      // Write pause in a very unoptimised way
      gzputc(out,VGM_PAUSE_WORD);
      gzputc(out,LOBYTE(PauseLength));
      gzputc(out,HIBYTE(PauseLength));
      // Reset PauseLength
      PauseLength=0;
      // Switch off noise-update bool
      NoiseUpdated=FALSE;
      // Remember what I wrote
      LastWrittenState=CurrentState;
      // Stop it doing any more full state writes
      FirstState=FALSE;
    }
  } while (b0!=EOF);

  // At end:
  // 1. Write EOF mrker
  gzputc(out,VGM_END);
  // 2. Copy GD3 tag
  if (VGMHeader.GD3Offset) {
    struct TGD3Header GD3Header;
    int NewGD3Offset=gztell(out)-GD3DELTA;
    gzseek(in,VGMHeader.GD3Offset+GD3DELTA,SEEK_SET);
    gzread(in,&GD3Header,sizeof(GD3Header));
    gzwrite(out,&GD3Header,sizeof(GD3Header));
    for (i=0; i<GD3Header.Length; ++i) gzputc(out,gzgetc(in));
    VGMHeader.GD3Offset=NewGD3Offset;
  }
  // 3. Fill in VGM header
  VGMHeader.EoFOffset=gztell(out)-EOFDELTA;
  // LoopOffset updated while optimising

  gzclose(out);

  WriteVGMHeader(Outfilename,VGMHeader);

  // Clean up
  gzclose(in);
  free(Outfilename);

  return TRUE;
}
*/


BOOL RoundToFrameAccurate(char* filename)
{
    struct VGMHeader VGMHeader;
    char b0, b1, b2;
    int i, PauseLength = 0;
    int bucketsize = 1; // how many samples per bucket for counting
    int framelength;
    char buffer[1024 * 10], buffer2[64];

    if (!FileExists(filename)) return FALSE;

    // Open input file
    gzFile in = gzopen(filename, "rb");

    // Read its VGM header
    if (!ReadVGMHeader(in, &VGMHeader,FALSE))
    {
        gzclose(in);
    }

    long int OldLoopOffset = VGMHeader.LoopOffset + LOOPDELTA;
    switch (VGMHeader.RecordingRate)
    {
    case 50:
        framelength = LEN50TH;
        break;
    case 60:
        framelength = LEN60TH;
        break;
    default:
        ShowError("Can't round this file because it's not defined as 50 or 60Hz.");
        gzclose(in);
        return FALSE;
        break;
    }

    int numbuckets = framelength / bucketsize + 1;

    auto PausePositions = static_cast<int*>(malloc(sizeof(int) * numbuckets));
    ZeroMemory(PausePositions, sizeof(int)*numbuckets);

    // Make the output filename...
    char* outfilename = make_temp_filename(filename);

    // ...open it...
    //  out=gzopen(outfilename,"wb0");

    // ...skip to the data section...
    gzseek(in,VGM_DATA_OFFSET,SEEK_SET);
    //  gzseek(out,VGM_DATA_OFFSET,SEEK_SET);

    // ...parse the input file to find pause alignment
    // Go through the file; if it's a pause, add it to the total; if it's data, write the current total and zero it
    do
    {
        b0 = gzgetc(in);
        switch (b0)
        {
        case VGM_GGST: // GG stereo
        case VGM_PSG: // PSG write
            if (PauseLength > 0)PausePositions[(PauseLength % framelength) / bucketsize]++;
        // increment the corresponding counter
            PauseLength %= framelength;
            b1 = gzgetc(in);
        //      gzputc(out,b0);
        //      gzputc(out,b1);
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
            if (PauseLength > 0)PausePositions[(PauseLength % framelength) / bucketsize]++;
        // increment the corresponding counter
            PauseLength %= framelength;
            b1 = gzgetc(in);
            b2 = gzgetc(in);
        //      gzputc(out,b0);
        //      gzputc(out,b1);
        //      gzputc(out,b2);
            break;
        case VGM_PAUSE_WORD: // Wait n samples
            b1 = gzgetc(in);
            b2 = gzgetc(in);
            PauseLength += MAKEWORD(b1, b2);
            break;
        case VGM_PAUSE_60TH: // Wait 1/60 s
            PauseLength += LEN60TH;
            break;
        case VGM_PAUSE_50TH: // Wait 1/50 s
            PauseLength += LEN50TH;
            break;
        //    case VGM_PAUSE_BYTE:  // Wait n samples
        //      b1=gzgetc(in);
        //      PauseLength+=b1;
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
            PauseLength += (b0 & 0xf) + 1;
            break;
        case VGM_END: // End of sound data
            b0 = EOF; // break out of loop
            break;
        default:
            break;
        } // end switch
    }
    while (b0 != EOF);

    // display the array

    int maxcount = 0;
    for (i = 0; i < numbuckets; ++i)
    {
        if (PausePositions[i] > maxcount)
            maxcount = PausePositions[i];
    }

    sprintf(buffer, "Slots (max %d):\n", maxcount);

    maxcount /= 9;

    for (i = 0; i < numbuckets; ++i)
    {
        sprintf(buffer2, "%d ", PausePositions[i]);
        strcat(buffer, buffer2);

        if (i % 32 == 31)
            strcat(buffer, "\n");
    }

    ShowMessage(buffer);

    /*
    // At end:
    // 1. Write EOF mrker
    gzputc(out,VGM_END);
    // 2. Copy GD3 tag
    if (VGMHeader.GD3Offset) {
      struct TGD3Header GD3Header;
      int NewGD3Offset=gztell(out)-GD3DELTA;
      gzseek(in,VGMHeader.GD3Offset+GD3DELTA,SEEK_SET);
      gzread(in,&GD3Header,sizeof(GD3Header));
      gzwrite(out,&GD3Header,sizeof(GD3Header));
      for (i=0; i<GD3Header.Length; ++i) gzputc(out,gzgetc(in));
      VGMHeader.GD3Offset=NewGD3Offset;
    }
    // 3. Fill in VGM header
    VGMHeader.EoFOffset=gztell(out)-EOFDELTA;
    // LoopOffset updated while optimising
  */
    //  gzclose(out);

    //  WriteVGMHeader(outfilename,VGMHeader);

    // Clean up
    gzclose(in);

    //  ReplaceFile(filename,outfilename);

    free(outfilename);

    return TRUE;
}
