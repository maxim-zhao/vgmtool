#include "gd3.h"
#include <zlib.h>

#include "IVGMToolCallback.h"
#include "vgm.h"
#include "utils.h"

//----------------------------------------------------------------------------------------------
// Remove GD3 from file
//----------------------------------------------------------------------------------------------
void remove_gd3(const std::string& filename, const IVGMToolCallback& callback)
{
    OldVGMHeader VGMHeader;
    if (!Utils::file_exists(filename))
    {
        return;
    }

    gzFile in = gzopen(filename.c_str(), "rb");

    if (!ReadVGMHeader(in, &VGMHeader, callback) || !VGMHeader.GD3Offset)
    {
        // do nothing if not a VGM file, or file already has no GD3
        gzclose(in);
        return;
    }

    // ShowStatus("Removing GD3 tag...");

    gzrewind(in);

    const std::string outFilename = Utils::make_temp_filename(filename);

    gzFile out = gzopen(outFilename.c_str(), "wb0");

    // Copy everything up to the GD3 tag
    for (auto i = 0; i < static_cast<int>(VGMHeader.GD3Offset + GD3DELTA); ++i)
    {
        gzputc(out, gzgetc(in));
    }

    VGMHeader.GD3Offset = 0;
    VGMHeader.EoFOffset = gztell(out) - EOFDELTA; // Update EoF offset in header

    gzclose(in);
    gzclose(out);

    write_vgm_header(outFilename, VGMHeader, callback); // Write changed header

    Utils::replace_file(filename, outFilename);

    callback.show_status("GD3 tag removed");
}
