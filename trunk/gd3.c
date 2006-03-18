#include "gd3.h"
#include <zlib.h>
#include "vgm.h"
#include "gui.h"
#include "utils.h"

//----------------------------------------------------------------------------------------------
// Remove GD3 from file
//----------------------------------------------------------------------------------------------
void RemoveGD3(char *filename) {
  gzFile *in,*out;
  struct TVGMHeader VGMHeader;
  char *outfilename;
  long int i;
  if (!FileExists(filename)) return;

  in=gzopen(filename,"rb");

  if(!ReadVGMHeader(in,&VGMHeader,FALSE) || !VGMHeader.GD3Offset) {
    // do nothing if not a VGM file, or file already has no GD3
    gzclose(in);
	return;
  }

  ShowStatus("Removing GD3 tag...");

  gzrewind(in);

  outfilename=MakeTempFilename(filename);

  out=gzopen(outfilename,"wb0");

  // Copy everything up to the GD3 tag
  for (i=0;i<VGMHeader.GD3Offset+GD3DELTA;++i) gzputc(out,gzgetc(in));

  VGMHeader.GD3Offset=0;
  VGMHeader.EoFOffset=gztell(out)-EOFDELTA;  // Update EoF offset in header

  gzclose(in);
  gzclose(out);

  WriteVGMHeader(outfilename,VGMHeader);  // Write changed header

  ReplaceFile(filename,outfilename);

  free(outfilename);

  ShowStatus("GD3 tag removed");
}
