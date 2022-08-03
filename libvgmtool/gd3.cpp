#include "gd3.h"
#include <zlib.h>

#include "IVGMToolCallback.h"
#include "vgm.h"
#include "utils.h"

//----------------------------------------------------------------------------------------------
// Remove GD3 from file
//----------------------------------------------------------------------------------------------
void remove_gd3(char* filename, const IVGMToolCallback& callback)
{
    VGMHeader VGMHeader;
    if (!FileExists(filename, callback)) return;

    gzFile in = gzopen(filename, "rb");

    if (!ReadVGMHeader(in, &VGMHeader, FALSE, callback) || !VGMHeader.GD3Offset)
    {
        // do nothing if not a VGM file, or file already has no GD3
        gzclose(in);
        return;
    }

    // ShowStatus("Removing GD3 tag...");

    gzrewind(in);

    char* outfilename = make_temp_filename(filename);

    gzFile out = gzopen(outfilename, "wb0");

    // Copy everything up to the GD3 tag
    for (long int i = 0; i < VGMHeader.GD3Offset + GD3DELTA; ++i) gzputc(out,gzgetc(in));

    VGMHeader.GD3Offset = 0;
    VGMHeader.EoFOffset = gztell(out) - EOFDELTA; // Update EoF offset in header

    gzclose(in);
    gzclose(out);

    write_vgm_header(outfilename, VGMHeader, callback); // Write changed header

    MyReplaceFile(filename, outfilename, callback);

    free(outfilename);

    callback.show_status("GD3 tag removed");
}
