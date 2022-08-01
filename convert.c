#include "convert.h"
#include <zlib.h>
#include <stdio.h>
#include "vgm.h"
#include "utils.h"
#include "gui.h"

struct TGYMXHeader {
    char gym_id[4];
    char song_title[32];
    char game_title[32];
    char game_publisher[32];
    char dumper_emu[32];
    char dumper_person[32];
    char comments[256];
    unsigned int looped;
    unsigned int compressed;
};

void SpreadDAC(gzFile in,gzFile out) {
  // Spread remaining DAC data up until next pause evenly throughout frame

  // When this function is called, in and out are open and the last byte read was
  // the first 0x2a DAC data address byte.

  int addr,data;
  int i,NumDACValues=1,InFileDataStart=gztell(in);
  long WaitLength;

  // 1. Count how many DAC values there are
  do {
    gzgetc(in);        // Skip DAC data
    data=gzgetc(in);    // Read next byte (00 for wait or 01 for port 0 for DAC/timer)
    if ((data==0)||(data==EOF)) break;    // Exit when I find a pause or EOF
    switch (data) {
    case 01:
      addr=gzgetc(in);    // Read next byte if not a wait (2a for DAC, 27 for timer)
      if (addr==0x2a) NumDACValues++;  // counter starts at 1 since if I'm here there's at least one
      break;
    case 02:
      gzgetc(in);
      break;
    case 03:
      break;
    }
  } while (1);

  // 2. Seek back to data start (01 of first 01 2a)
  gzseek(in,InFileDataStart-2,SEEK_SET);

  // 3. Read data again, this time outputting it with wait commands between
  i=0;
  do {
    data=gzgetc(in);  // GYM data type byte
    if ((data==0)||(data==EOF)) break;

    switch (data) {
    case 01:
      gzputc(out,VGM_YM2612_0);
      addr=gzgetc(in);  // 2a for DAC, etc
      data=gzgetc(in);  // data
      gzputc(out,addr);
      gzputc(out,data);
      if (addr==0x2a) {
        // Got to DAC data so let's pause
        // Calculate pause length
        WaitLength=(LEN60TH*(i+1)/NumDACValues)-(LEN60TH*i/NumDACValues);
        // Write pause
        WritePause(out,WaitLength);
        // Increment counter
        i++;
      }
      break;
    case 02:
      gzputc(out,VGM_YM2612_1);
      addr=gzgetc(in);
      data=gzgetc(in);
      gzputc(out,addr);
      gzputc(out,data);
      break;
    case 03:
      gzputc(out,VGM_PSG);
      data=gzgetc(in);
      gzputc(out,data);
      break;
    }

  } while (1);

  i=gztell(in);
  if (i>0xce00) {
    data=data;
  }

  // 4. Finished!
}

BOOL ConverttoVGM(char *filename,int FileType) {
  gzFile in,out;
  char *Outfilename;
  struct TVGMHeader VGMHeader;
  char b1,YM2413Address=(char)0xff,*p1,*p2;
  long int Length=0,LoopLength=0,LoopOffset=0;

  if (!FileExists(filename)) return FALSE;

  // Make output filename filename.gym.vgz
  Outfilename=malloc(strlen(filename)+10);
  strcpy(Outfilename,filename);
  strcat(Outfilename,".vgz");

  p1=strrchr(filename,'\\')+1;
  p2=strrchr(Outfilename,'\\')+1;
  ShowStatus("Converting \"%s\" to VGM format...",p1);

  // Open files
  in=gzopen(filename,"rb");
  out=gzopen(Outfilename,"wb0");

  gzseek(out,VGM_DATA_OFFSET,SEEK_SET);

  // Fill in VGM header
  VGMHeader.VGMIdent = VGMIDENT;
  VGMHeader.YM2413Clock=0;
  VGMHeader.YM2612Clock=0;
  VGMHeader.YM2151Clock=0;
  VGMHeader.PSGClock=0;
  VGMHeader.TotalLength=0;
  VGMHeader.LoopLength=0;
  VGMHeader.LoopOffset=0;
  VGMHeader.GD3Offset=0;
  VGMHeader.RecordingRate=60;
  VGMHeader.Version=0x110;

  switch (FileType) {
  case ftGYM: {
    // GYM format:
    // 00    wait
    // 01 aa dd  YM2612 port 0 address aa data dd
    // 02 aa dd  YM2612 port 1 address aa data dd
    // 03 dd  PSG data dd

    struct TGYMXHeader GYMXHeader;
//    struct TGD3Header GD3Header;

    // Check for GYMX header
    gzread(in,&GYMXHeader,sizeof(GYMXHeader));

    if (strncmp(GYMXHeader.gym_id,"GYMX",4)==0) {
      // File has a GYM header

      // Is the file compressed?
      if (GYMXHeader.compressed) {
        // Can't handle that
        AddConvertText("Cannot convert compressed GYM \"%s\" - see vgmtool.txt\r\n",p1);
        gzclose(out);
        gzclose(in);
        DeleteFile(Outfilename);
        free(Outfilename);
        return FALSE;
      }

      // To do: put the tag information in a GD3, handle looping

      if (GYMXHeader.looped) {
        // Figure out the loop point in samples
        // Store it temporarily in LoopLength
        VGMHeader.LoopLength=(GYMXHeader.looped-1)*LEN60TH;
      }
      
      // Seek to the GYM data
      gzseek(in,428,SEEK_SET);
    }

    // To do:
    // GYMX handling
    // - GD3 from header
    // - looping (check it works)
    do {
      if (GYMXHeader.looped && (VGMHeader.TotalLength==VGMHeader.LoopLength)) {
        VGMHeader.LoopOffset=gztell(out)-LOOPDELTA;
      }
      b1=gzgetc(in);
      switch (b1) {
      case 0:  // Wait 1/60s
        gzputc(out,VGM_PAUSE_60TH);
        VGMHeader.TotalLength+=LEN60TH;
        break;
      case 1: // YM2612 port 0
        b1=gzgetc(in);
        if (b1==0x2a) {
          // DAC...
          SpreadDAC(in,out);
          VGMHeader.TotalLength+=LEN60TH;
          // SpreadDAC absorbs the next wait command
        } else {
          gzputc(out,VGM_YM2612_0);
          gzputc(out,b1);
          b1=gzgetc(in);
          gzputc(out,b1);
        }
        VGMHeader.YM2612Clock=7670454;  // 3579545*15/7
        break;
      case 2: // YM2612 port 1
        gzputc(out,VGM_YM2612_1);
        b1=gzgetc(in);
        gzputc(out,b1);
        b1=gzgetc(in);
        gzputc(out,b1);
        VGMHeader.YM2612Clock=7670454;
        break;
      case 3: // PSG
        gzputc(out,VGM_PSG);
        b1=gzgetc(in);
        gzputc(out,b1);
        VGMHeader.PSGClock=3579545;
        VGMHeader.PSGShiftRegisterWidth = 16;
        VGMHeader.PSGWhiteNoiseFeedback = 0x0009;
        break;
      default:  // Ignore unwanted bytes & EOF
        break;
      }
    } while (!gzeof(in));
    if (GYMXHeader.looped) {
      // Convert LoopLength from AB to BC
      // A                     B                     C
      // start --------------- loop point -------- end
      VGMHeader.LoopLength=VGMHeader.TotalLength-VGMHeader.LoopLength;
    }
    }
    break;
  case ftSSL:
    // SSL format:
    // 00    wait
    // 03 dd  PSG data dd
    // 04 dd  GG stereo dd
    // 05 aa  YM2413 address aa
    // 06 dd  YM2413 data dd
    do {
      b1=gzgetc(in);
      switch (b1) {
      case 0:  // Wait 1/60s
        gzputc(out,VGM_PAUSE_60TH);
        VGMHeader.TotalLength+=LEN60TH;
        break;
      case 3: // PSG
        gzputc(out,VGM_PSG);
        b1=gzgetc(in);
        gzputc(out,b1);
        VGMHeader.PSGClock=3579545;
        VGMHeader.PSGShiftRegisterWidth = 16;
        VGMHeader.PSGWhiteNoiseFeedback = 0x0009;
        break;
      case 4: // GG stereo
        gzputc(out,VGM_GGST);
        b1=gzgetc(in);
        gzputc(out,b1);
        break;
      case 5: // YM2413 address
        YM2413Address=gzgetc(in);
        break;
      case 6: // YM2413 data
        gzputc(out,VGM_YM2413);
        gzputc(out,YM2413Address);
        b1=gzgetc(in);
        gzputc(out,b1);
        VGMHeader.YM2413Clock=3579545;
        break;
      default:  // Ignore unwanted bytes & EOF
        break;
      }
    } while (!gzeof(in));
    break;
  case ftCYM:
    // CYM format
    // 00    wait
    // aa dd  YM2151 address aa data dd
    do {
      b1=gzgetc(in);
      switch (b1) {
      case 0:    // Wait 1/60s
        gzputc(out,VGM_PAUSE_60TH);
        VGMHeader.TotalLength+=LEN60TH;
        break;
      case EOF:  // Needs specific handling this time
        break;
      default:  // Other data
        gzputc(out,VGM_YM2151);
        gzputc(out,b1);
        b1=gzgetc(in);
        gzputc(out,b1);
        VGMHeader.YM2151Clock=7670454;
        break;
      }
    } while (!gzeof(in));
    break;
  }

  gzputc(out,VGM_END);

  // Fill in more of the VGM header
  VGMHeader.EoFOffset=gztell(out)-EOFDELTA;

  // Close files
  gzclose(out);
  gzclose(in);

  // Update header
  WriteVGMHeader(Outfilename,VGMHeader);
  Compress(Outfilename);

  // Report
  AddConvertText("Converted \"%s\" to \"%s\"\r\n",p1,p2);

  free(Outfilename);
  return TRUE;
}
