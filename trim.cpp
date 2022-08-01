#include <cstdlib>
#include <cstring>
#include <cstdio>
#include "trim.h"
#include "gui.h"
#include <zlib.h>
#include "vgm.h"
#include "gd3.h"
#include "utils.h"

//----------------------------------------------------------------------------------------------
// Creates a log of the trim for future reference
// in a file called "editpoints.txt" in the VGM's directory
//----------------------------------------------------------------------------------------------
void LogTrim(char* VGMFile, int start, int loop, int end)
{
    if (!strrchr(VGMFile, '\\')) return;

    // make filename to log to
    auto fn = static_cast<char*>(malloc(strlen(VGMFile) + 20));
    strcpy(fn, VGMFile);
    strcpy(strrchr(fn, '\\') + 1, "editpoints.txt");

    FILE* f = fopen(fn, "a"); // open file for append

    if (!f) ShowError("Error opening editpoints.txt");
    else
        fprintf(f, "Filename: %s\n"
            "Start:    %d\n"
            "Loop:     %d\n"
            "End:      %d\n\n",
            strrchr(VGMFile, '\\') + 1,
            start,
            loop,
            end);

    if (f) fclose(f);

    free(fn);
}

//----------------------------------------------------------------------------------------------
// Rewritten Trim with:
// - Loop point initialisation at end of file
// - No optimisation
// - Hopefully some clarity
//----------------------------------------------------------------------------------------------
BOOL NewTrim(char* filename, const long int start, const long int loop, const long int end)
{
    gzFile in, out;
    struct TVGMHeader VGMHeader;
    char* outfilename;
    int b0, b1, b2;
    struct TSystemState CurrentState, LoopPointState;

    BOOL PastStart = FALSE, PastLoop = (loop < 0);
    // If loop<0 then PastLoop=TRUE so it won't bother to record a loop state ever
    // Thus loop=-1 means no loop

    if (!FileExists(filename)) return FALSE;

    // Check edit points are sensible
    if ((start < 0) ||
        ((loop != -1) && (loop < start)) ||
        (end <= loop))
    {
        // Invalid edit points
        ShowError("Invalid edit points!\nFailed the condition start <= loop < end");
        return FALSE;
    }

    // Open input file
    in = gzopen(filename, "rb");

    // Read its VGM header
    gzread(in, &VGMHeader, sizeof(VGMHeader));

    // See if the edit points are within the file
    if (end > VGMHeader.TotalLength)
    {
        gzclose(in);
        ShowError("End point beyond end of file");
        return FALSE;
    }

    // Parse input file to see what chips are used
    GetUsedChips(in, &CurrentState.UsesPSG, &CurrentState.UsesYM2413, &CurrentState.UsesYM2612,
        &CurrentState.UsesYM2151, &CurrentState.UsesReserved);

    // Let's make the output filename...
    outfilename = MakeSuffixedFilename(filename, "trimmed");

    // ...open it...
    out = gzopen(outfilename, "wb0");

    // ...skip to the data section...
    gzseek(in,VGM_DATA_OFFSET,SEEK_SET);
    gzseek(out,VGM_DATA_OFFSET,SEEK_SET);

    // ...initialise state to default values...
    ResetState(&CurrentState);

    // ...and parse/copy the input file
    // Record the current state at all times, and if PastStart, pass data through
    do
    {
        b0 = gzgetc(in);
        switch (b0)
        {
        case VGM_GGST: // GG stereo
        case VGM_PSG: // PSG write
            // both have 1 byte data
            b1 = gzgetc(in);
            WriteToState(&CurrentState, b0, b1, 0);
            if (PastStart)
            {
                gzputc(out, b0);
                gzputc(out, b1);
            }
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
        case 0x5f: // reserved types
            // All have 2 bytes of data
            b1 = gzgetc(in);
            b2 = gzgetc(in);
            WriteToState(&CurrentState, b0, b1, b2);
            if (PastStart)
            {
                gzputc(out, b0);
                gzputc(out, b1);
                gzputc(out, b2);
            }
            break;
        case VGM_PAUSE_WORD: // Wait n samples
            b1 = gzgetc(in);
            b2 = gzgetc(in);
            WriteToState(&CurrentState, b0, b1, b2);
            if ( // Write waits if:
                PastStart && // Past start (so in data-writing mode) AND
                (
                    PastLoop || // past loop point
                    (CurrentState.samplecount < loop) // OR before it
                ) // (so it won't be written as I go past it, since I need to divide that wait into two)
                && // AND
                (CurrentState.samplecount < end) // before the end (final pause not written either)
            )
            {
                gzputc(out, b0);
                gzputc(out, b1);
                gzputc(out, b2);
            }
            break;
        //    case VGM_PAUSE_BYTE:  // Wait n samples
        //      b1=gzgetc(in);
        //      WriteToState(&CurrentState,b0,b1,0);
        // Same conditions for writing all pauses
        //      if (PastStart && (PastLoop || (CurrentState.samplecount<loop)) && (CurrentState.samplecount<end) ) {
        //        gzputc(out,b0);
        //        gzputc(out,b1);
        //      }
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
        case VGM_PAUSE_60TH: // Wait 1/60 s
        case VGM_PAUSE_50TH: // Wait 1/50 s
            // All are 1-byte pause commands
            WriteToState(&CurrentState, b0, 0, 0);
            if (PastStart && (PastLoop || (CurrentState.samplecount < loop)) && (CurrentState.samplecount < end))
            {
                gzputc(out, b0);
            }
            break;
        case VGM_END: // End of sound data
            b0 = EOF; // break out of loop
            break;
        default:
            break;
        } // end switch


        // Start point state write

        if ((!PastStart) && (CurrentState.samplecount >= start))
        {
            // We've gone past (or equalled) the start point, so:
            // 1. Write state
            WriteStateToFile(out, &CurrentState,TRUE);
            // 2. Write any remaining pause left
            WritePause(out, CurrentState.samplecount - start);
            // 3. Record that it's time to start copying data
            PastStart = TRUE;
        }

        // Loop point state saving and pause splitting (if needed)

        if ((!PastLoop) && (CurrentState.samplecount >= loop))
        {
            // We've gone past (or equalled) the loop point so:
            int LastPauseLength;
            // 1. Record current state
            LoopPointState = CurrentState;
            // 2. Write any remaining pause up to the loop point
            // 2.1 Find how long the last pause was
            switch (b0)
            {
            case VGM_PAUSE_WORD: LastPauseLength = b1 | (b2 << 8);
                break;
            //      case VGM_PAUSE_BYTE: LastPauseLength=b1;          break;
            case VGM_PAUSE_60TH: LastPauseLength = LEN60TH;
                break;
            case VGM_PAUSE_50TH: LastPauseLength = LEN50TH;
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
                LastPauseLength = (b0 & 0xf) + 1;
                break;
            }
            // 2.2 Handle cases where the loop and start points are close together
            // This is complicated, so it's super-comment time...
            /*
               Case 1: start was ages ago, loop has been passed
               Start of data      Last data             Next data
                                      X                           last pause starts here
                                      <--------------------->     LastPauseLength
                 <------------------------------------------>     CurrentState.samplecount
                 <------------------------------->                loop
                                      <---------->                Write this pause    = LastPauseLength - (CurrentState.samplecount - loop)
                                                  <--------->     Then this pause     = CurrentState.samplecount - loop
               Looping will point to here --------^
               Loop state will be written before EOF
               TODO: make sure pause optimiser doesn't merge these two consecutive pauses!
      
               Case 2: start was ages ago, loop has just been reached
               Start of data      Last data             Next data
                                      X                           last pause starts here
                                      <--------------------->     LastPauseLength
                 <------------------------------------------>     CurrentState.samplecount
                 <------------------------------------------>     loop
                                      <--------------------->     Write this pause    = LastPauseLength
                                                                  Then NO pause
               Looping will point to here --------------------^
               Loop state will be written before EOF
               This is OK, because (CurrentState.samplecount - loop) = 0 so the above expressions are OK
      
               Case 3: start and loop both happened since the last data
               Start of data      Last data             Next data
                                      X                           last pause starts here
                                      <--------------------->     LastPauseLength
                 <------------------------------------------>     CurrentState.samplecount
                 <------------------------>                       start
                 <------------------------------->                loop
                                           <---------------->     CurrentState.samplecount - start
                                           <----->                Write this pause    = (CurrentState.samplecount - start) - (CurrentState.samplecount - loop)
                                                  <--------->     Then this pause     = CurrentState.samplecount - loop
               Looping will point to here --------^
               Loop state will be written before EOF
               This can be achieved using the same expressions by setting LastPauseLength to (CurrentState.samplecount - start)
               when LastPauseLength > CurrentState.samplecount-start
      
               I am not handling the case where loop is very close to end, or all three are within one pause command.
               No real music has that and it'd be a lot more work to get right.
      
            */
            if (LastPauseLength > (CurrentState.samplecount - start)) LastPauseLength = loop - start;

            WritePause(out, LastPauseLength - (CurrentState.samplecount - loop));
            // 3. Record loop offset
            VGMHeader.LoopOffset = gztell(out) - LOOPDELTA;
            // 4. Write any remaining pause left
            WritePause(out, CurrentState.samplecount - loop);
            // 5. Record that I've done it
            PastLoop = TRUE;
        }

        if (CurrentState.samplecount >= end)
        {
            // We've got to the end
            // 1. Write any remaining pause
            int LastPauseLength;
            switch (b0)
            {
            case VGM_PAUSE_WORD: LastPauseLength = b1 | (b2 << 8);
                break;
            //      case VGM_PAUSE_BYTE: LastPauseLength=b1;          break;
            case VGM_PAUSE_60TH: LastPauseLength = LEN60TH;
                break;
            case VGM_PAUSE_50TH: LastPauseLength = LEN50TH;
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
                LastPauseLength = (b0 & 0xf) + 1;
                break;
            }
            // Start   Last data   end   Next data
            //             X                      Last pause starts here
            //             <----------------->    LastPauseLength
            //   <--------------------------->    SampleCount
            //   <------------------>             end
            //             <-------->             end-(SampleCount-LastPauseLength)
            WritePause(out, end - (CurrentState.samplecount - LastPauseLength));
            // 2. Quit (do rest outside loop)
            b0 = EOF;
        }
    }
    while (b0 != EOF);

    // At end:
    // 1. If we're doing looping, write the loop state
    if (loop != -1) WriteStateToFile(out, &CurrentState,FALSE);
    // 2. Write EOF mrker
    gzputc(out,VGM_END);
    // 3. Copy GD3 tag
    if (VGMHeader.GD3Offset)
    {
        struct TGD3Header GD3Header;
        int i;
        int NewGD3Offset = gztell(out) - GD3DELTA;
        ShowStatus("Copying GD3 tag...");
        gzseek(in, VGMHeader.GD3Offset + GD3DELTA,SEEK_SET);
        gzread(in, &GD3Header, sizeof(GD3Header));
        gzwrite(out, &GD3Header, sizeof(GD3Header));
        for (i = 0; i < GD3Header.Length; ++i) gzputc(out,gzgetc(in));
        VGMHeader.GD3Offset = NewGD3Offset;
    }
    gzclose(in);
    // 4. Fill in VGM header
    VGMHeader.EoFOffset = gztell(out) - EOFDELTA;
    VGMHeader.TotalLength = end - start;
    if (loop > -1) VGMHeader.LoopLength = end - loop;
    else
    {
        VGMHeader.LoopLength = 0;
        VGMHeader.LoopOffset = 0;
    }

    gzclose(out);
    WriteVGMHeader(outfilename, VGMHeader);

    free(outfilename);
    return TRUE;
}
