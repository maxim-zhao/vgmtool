#pragma once

class IStatusCallback;
class VgmFile;

void trim_vgm_file(VgmFile& vgmFile, int start, int loop, int end, const IStatusCallback& callback);

