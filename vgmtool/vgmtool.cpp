// Dialogue-based UI
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>

#include <Windows.h>
#include <CommCtrl.h>
#include <Uxtheme.h>
#include <zlib.h>

#include <convert.h>
#include <gd3.h>
#include "gui.h"
#include <optimise.h>
#include <sstream>

#include "resource.h"
#include <trim.h>
#include "utils.h"
#include <vgm.h>
#include <writetotext.h>

#include "IVGMToolCallback.h"

// This hooks our GUI handlers to the callback interface
class Callback : public IVGMToolCallback
{
public:
    void show_message(const std::string& message) const override
    {
        show_message_box("%s", message.c_str());
    }

    void show_status(const std::string& message) const override
    {
        set_status_text("%s", message.c_str());
    }

    void show_conversion_progress(const std::string& message) const override
    {
        add_convert_text("%s\r\n", message.c_str());
    }

    void show_error(const std::string& message) const override
    {
        show_error_message_box("%s", message.c_str());
    }
} callback;

// This allows us to have modern style widgets
#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

// GD3 versions I can accept
#define MINGD3VERSION 0x100
#define REQUIREDGD3MAJORVER 0x100

HWND hWndMain = nullptr;
HINSTANCE HInst;

std::string g_current_filename("You didn't load a file yet!");
VGMHeader g_current_file_vgm_header;

std::string g_program_name = "VGMTool 2 release 6 BETA";

const int GD3EditControls[NumGD3Strings] = {
    edtGD3TitleEn, edtGD3TitleJp, edtGD3GameEn, edtGD3GameJp, cbGD3SystemEn, edtGD3SystemJp, edtGD3AuthorEn,
    edtGD3AuthorJp,
    edtGD3Date, edtGD3Creator, edtGD3Notes
};

#define NumPSGTypes 5
#define NumYM2413Types 16
#define NumYM2612Types 1
#define NumYM2151Types 1
#define NumReservedTypes 1

unsigned long int PSGWrites[NumPSGTypes] = {0}; // 0-2 = tone, 3 = noise, 4 = GG st
unsigned long int YM2413Writes[NumYM2413Types] = {0}; // 0-8 = tone, 9-13 = perc, 14 = user inst, 15 = invalid reg
unsigned long int YM2612Writes[NumYM2612Types] = {0}; // Unsupported
unsigned long int YM2151Writes[NumYM2151Types] = {0}; // Unsupported
unsigned long int ReservedWrites[NumReservedTypes] = {0}; // reserved chips

const int PSGCheckBoxes[NumPSGTypes] = {cbPSG0, cbPSG1, cbPSG2, cbPSGNoise, cbPSGGGSt};
const int YM2413CheckBoxes[NumYM2413Types] = {
    cbYM24130, cbYM24131, cbYM24132, cbYM24133, cbYM24134, cbYM24135, cbYM24136, cbYM24137, cbYM24138, cbYM2413HiHat,
    cbYM2413Cymbal, cbYM2413TomTom, cbYM2413SnareDrum, cbYM2413BassDrum, cbYM2413UserInst, cbYM2413InvalidRegs
};
const int YM2612CheckBoxes[NumYM2612Types] = {cbYM2612};
const int YM2151CheckBoxes[NumYM2151Types] = {cbYM2151};
const int ReservedCheckboxes[NumReservedTypes] = {cbReserved};


wchar_t* GD3Strings = nullptr;

#define NumTabChildWnds 6
HWND TabChildWnds[NumTabChildWnds]; // Holds child windows' HWnds
// Defines to make it easier to place stuff where I want
#define HeaderWnd   TabChildWnds[0]
#define TrimWnd     TabChildWnds[1]
#define StripWnd    TabChildWnds[2]
#define GD3Wnd      TabChildWnds[3]
#define ConvertWnd  TabChildWnds[4]
#define MiscWnd     TabChildWnds[5]

#define BUFFER_SIZE 1024

// TODO move this to optimise.cpp?
void Optimize(const std::string& filename)
{
    VGMHeader VGMHeader;
    int NumOffsetsRemoved = 0;

    gzFile in = gzopen(filename.c_str(), "rb");
    gzread(in, &VGMHeader, sizeof(VGMHeader));
    gzclose(in);
    long FileSizeBefore = VGMHeader.EoFOffset + EOFDELTA;

    // Make sure lengths are correct
    check_lengths(filename, FALSE, callback);

    // Remove PSG offsets if selected
    if ((VGMHeader.PSGClock) && IsDlgButtonChecked(TrimWnd, cbRemoveOffset))
    {
        NumOffsetsRemoved = remove_offset(filename, callback);
    }

    // Trim (using the existing edit points), also merges pauses
    if (VGMHeader.LoopLength)
    {
        trim(filename, 0, static_cast<int>(VGMHeader.TotalLength - VGMHeader.LoopLength),
            static_cast<int>(VGMHeader.TotalLength), true, false, callback);
    }
    else
    {
        trim(filename, 0, -1, static_cast<int>(VGMHeader.TotalLength), true, false, callback);
    }

    in = gzopen(filename.c_str(), "rb");
    gzread(in, &VGMHeader, sizeof(VGMHeader));
    gzclose(in);
    long FileSizeAfter = VGMHeader.EoFOffset + EOFDELTA;

    if (show_question_message_box(
        "File optimised to\n"
        "%s\n"
        "%d offsets/silent PSG writes removed, \n"
        "Uncompressed file size %d -> %d bytes (%+.2f%%)\n"
        "Do you want to open it in the associated program?",
        filename.c_str(),
        NumOffsetsRemoved,
        FileSizeBefore,
        FileSizeAfter,
        (FileSizeAfter - FileSizeBefore) * 100.0 / FileSizeBefore
    ) == IDYES)
    {
        ShellExecute(hWndMain, "Play", filename.c_str(), nullptr, nullptr, SW_NORMAL);
    }
}

void UpdateWriteCount(const int CheckBoxes[], unsigned long Writes[], int count)
{
    for (int i = 0; i < count; ++i)
    {
        auto s = get_string(StripWnd, CheckBoxes[i]);
        // Remove existing count
        if (const auto index = s.find(" ("); index != std::string::npos)
        {
            s = s.substr(0, index);
        }
        // And add the new one
        if (Writes[i] > 0)
        {
            s = Utils::format("%s (%d)", s.c_str(), Writes[i]);
        }
        SetDlgItemText(StripWnd, CheckBoxes[i], s.c_str());
        EnableWindow(GetDlgItem(StripWnd, CheckBoxes[i]), Writes[i] > 0);
        if (Writes[i] == 0)
        {
            CheckDlgButton(StripWnd, CheckBoxes[i], 0);
        }
    }
}

// Check checkboxes and show numbers for how many times each channel/data type is used
void CheckWriteCounts(const char* filename)
{
    int i, j;
    if (filename != nullptr)
    {
        GetWriteCounts(filename, PSGWrites, YM2413Writes, YM2612Writes, YM2151Writes, ReservedWrites, callback);
    }
    else
    {
        memset(PSGWrites, 0, sizeof(PSGWrites));
        memset(YM2413Writes, 0, sizeof(YM2413Writes));
        memset(YM2612Writes, 0, sizeof(YM2612Writes));
        memset(YM2151Writes, 0, sizeof(YM2151Writes));
        memset(ReservedWrites, 0, sizeof(ReservedWrites));
    }

    UpdateWriteCount(PSGCheckBoxes, PSGWrites, NumPSGTypes);
    UpdateWriteCount(YM2413CheckBoxes, YM2413Writes, NumYM2413Types);
    UpdateWriteCount(YM2612CheckBoxes, YM2612Writes, NumYM2612Types);
    UpdateWriteCount(YM2151CheckBoxes, YM2151Writes, NumYM2151Types);
    UpdateWriteCount(ReservedCheckboxes, ReservedWrites, NumReservedTypes);

    // Sum stuff for group checkboxes and other stuff:
    // PSG tone channels
    for (i = j = 0; i < 3; ++i)
    {
        j += PSGWrites[i]; // count writes
    }
    if (!j)
    {
        CheckDlgButton(StripWnd, cbPSGTone, 0); // if >0, initialise unchecked
    }
    EnableWindow(GetDlgItem(StripWnd, cbPSGTone), (j > 0)); // enabled = (>0)
    // YM2413 tone channels
    for (i = j = 0; i < 9; ++i)
    {
        j += YM2413Writes[i];
    }
    if (!j)
    {
        CheckDlgButton(StripWnd, cbYM2413Tone, 0);
    }
    EnableWindow(GetDlgItem(StripWnd, cbYM2413Tone), (j > 0));
    // YM2413 percussion
    for (i = 9, j = 0; i < 14; ++i)
    {
        j += YM2413Writes[i];
    }
    if (!j)
    {
        CheckDlgButton(StripWnd, cbYM2413Percussion, 0);
    }
    EnableWindow(GetDlgItem(StripWnd, cbYM2413Percussion), (j > 0));
    // PSG anything
    for (i = j = 0; i < NumPSGTypes; ++i)
    {
        j += PSGWrites[i];
    }
    EnableWindow(GetDlgItem(StripWnd, gbPSG), (j != 0));
    // YM2413 anything
    for (i = j = 0; i < NumYM2413Types; ++i)
    {
        j += YM2413Writes[i];
    }
    EnableWindow(GetDlgItem(StripWnd, gbYM2413), (j != 0));

    set_status_text("Scan for chip data complete");
}


// Load a file - check it's valid, load displayed info
void LoadFile(const char* filename)
{
    char buffer[64];
    TGD3Header GD3Header;
    int FileHasGD3 = 0;

    if (!Utils::file_exists(filename))
    {
        return;
    }

    set_status_text("Loading file...");

    gzFile in = gzopen(filename, "rb");
    gzread(in, &g_current_file_vgm_header, sizeof(g_current_file_vgm_header));

    if (!g_current_file_vgm_header.is_valid())
    {
        // no VGM marker
        show_error_message_box(
            "File is not a VGM file!\nIt will not be opened.\n\nMaybe you want to convert GYM, CYM and SSL files to VGM?\nClick on the \"Conversion\" tab.");
        g_current_filename.clear();
        SetDlgItemText(hWndMain, edtFileName, "Drop a file onto the window to load");
        set_status_text("");
        return;
    }

    g_current_filename = filename; // Remember it
    SetDlgItemText(hWndMain, edtFileName, filename); // Put it in the box

    if (g_current_file_vgm_header.GD3Offset)
    {
        // GD3 tag exists
        gzseek(in, g_current_file_vgm_header.GD3Offset + GD3DELTA, SEEK_SET);
        gzread(in, &GD3Header, sizeof(GD3Header));
        if (
            (strncmp(GD3Header.id_string, "Gd3 ", 4) == 0) && // Has valid marker
            (GD3Header.version >= MINGD3VERSION) && // and is an acceptable version
            ((GD3Header.version & REQUIREDGD3MAJORVER) == REQUIREDGD3MAJORVER)
        )
        {
            if (GD3Strings)
            {
                free(GD3Strings);
            }
            GD3Strings = static_cast<wchar_t*>(malloc(GD3Header.length));
            gzread(in, GD3Strings, GD3Header.length);
            FileHasGD3 = 1;
        }
    }
    gzclose(in);

    // Rate
    SetDlgItemInt(HeaderWnd, edtPlaybackRate, g_current_file_vgm_header.RecordingRate, FALSE);

    // Lengths
    int Mins = static_cast<int>(g_current_file_vgm_header.TotalLength) / 44100 / 60;
    int Secs = static_cast<int>(g_current_file_vgm_header.TotalLength) / 44100 - Mins * 60;
    sprintf(buffer, "%d:%02d", Mins, Secs);
    SetDlgItemText(HeaderWnd, edtLengthTotal, buffer);
    if (g_current_file_vgm_header.LoopLength > 0)
    {
        Mins = static_cast<int>(g_current_file_vgm_header.LoopLength) / 44100 / 60;
        Secs = static_cast<int>(g_current_file_vgm_header.LoopLength) / 44100 - Mins * 60;
        sprintf(buffer, "%d:%02d", Mins, Secs);
    }
    else
    {
        strcpy(buffer, "-");
    }
    SetDlgItemText(HeaderWnd, edtLengthLoop, buffer);

    // Version
    sprintf(buffer, "%x.%02x", g_current_file_vgm_header.Version >> 8, g_current_file_vgm_header.Version & 0xff);
    SetDlgItemText(HeaderWnd, edtVersion, buffer);

    // Clock speeds
    SetDlgItemInt(HeaderWnd, edtPSGClock, g_current_file_vgm_header.PSGClock, FALSE);
    SetDlgItemInt(HeaderWnd, edtYM2413Clock, g_current_file_vgm_header.YM2413Clock, FALSE);
    SetDlgItemInt(HeaderWnd, edtYM2612Clock, g_current_file_vgm_header.YM2612Clock, FALSE);
    SetDlgItemInt(HeaderWnd, edtYM2151Clock, g_current_file_vgm_header.YM2151Clock, FALSE);

    // PSG settings
    sprintf(buffer, "0x%04x", g_current_file_vgm_header.PSGWhiteNoiseFeedback);
    SetDlgItemText(HeaderWnd, edtPSGFeedback, buffer);
    SetDlgItemInt(HeaderWnd, edtPSGSRWidth, g_current_file_vgm_header.PSGShiftRegisterWidth, FALSE);

    // GD3 tag
    if (GD3Strings)
    {
        wchar_t* GD3string = GD3Strings; // pointer to parse GD3Strings
        char MBCSstring[1024 * 2]; // Allow 2KB to hold each string
        wchar_t ModifiedString[1024 * 2];

        for (int i = 0; i < NumGD3Strings; ++i)
        {
            wcscpy(ModifiedString, GD3string);

            switch (i)
            {
            // special handling for any strings?
            case 10: // Notes: change \n to \r\n so Windows shows it properly
                {
                    wchar_t* wp = ModifiedString + wcslen(ModifiedString);
                    while (wp >= ModifiedString)
                    {
                        if (*wp == L'\n')
                        {
                            memmove(wp + 1, wp, (wcslen(wp) + 1) * 2);
                            *wp = L'\r';
                        }
                        wp--;
                    }
                }
                break;
            }

            if (!SetDlgItemTextW(GD3Wnd, GD3EditControls[i], ModifiedString))
            {
                // Widechar text setting failed, try WC2MB
                BOOL ConversionFailure;
                WideCharToMultiByte(CP_ACP, 0, ModifiedString, -1, MBCSstring, 1024, nullptr, &ConversionFailure);
                if (ConversionFailure)
                {
                    // Alternate method: transcribe to HTML escaped data
                    wchar_t* p = ModifiedString;
                    char tempstr[10];
                    *MBCSstring = '\0';
                    for (unsigned int j = 0; j < wcslen(ModifiedString); j++)
                    {
                        int val = *p;
                        if (val < 128)
                        {
                            sprintf(tempstr, "%c", *p);
                        }
                        else
                        {
                            sprintf(tempstr, "&#x%04x;", val);
                        }
                        strcat(MBCSstring, tempstr);
                        p++;
                    }
                }
                SetDlgItemText(GD3Wnd, GD3EditControls[i], MBCSstring); // and put it in the edit control
            }

            // Move the pointer to the next string
            GD3string += wcslen(GD3string) + 1;
        }
    }

    CheckWriteCounts(nullptr); // reset counts

    if ((!FileHasGD3) && GD3Strings)
    {
        set_status_text("File loaded - file has no GD3 tag, previous tag kept");
    }
    else
    {
        set_status_text("File loaded");
    }
}

void UpdateHeader()
{
    char buffer[64];
    int i, j;
    VGMHeader VGMHeader;

    if (!Utils::file_exists(g_current_filename))
    {
        return;
    }

    gzFile in = gzopen(g_current_filename.c_str(), "rb");
    gzread(in, &VGMHeader, sizeof(VGMHeader));
    gzclose(in);

    if (get_int(HeaderWnd, edtPlaybackRate, &i))
    {
        VGMHeader.RecordingRate = i;
    }

    GetDlgItemText(HeaderWnd, edtVersion, buffer, 64);
    if (sscanf(buffer, "%d.%d", &i, &j) == 2)
    {
        // valid data
        VGMHeader.Version =
            (i / 10) << 12 | // major tens
            (i % 10) << 8 | // major units
            (j / 10) << 4 | // minor tens
            (j % 10); // minor units
    }

    if (get_int(HeaderWnd, edtPSGClock, &i))
    {
        VGMHeader.PSGClock = i;
    }
    if (get_int(HeaderWnd, edtYM2413Clock, &i))
    {
        VGMHeader.YM2413Clock = i;
    }
    if (get_int(HeaderWnd, edtYM2612Clock, &i))
    {
        VGMHeader.YM2612Clock = i;
    }
    if (get_int(HeaderWnd, edtYM2151Clock, &i))
    {
        VGMHeader.YM2151Clock = i;
    }

    GetDlgItemText(HeaderWnd, edtPSGFeedback, buffer, 64);
    if (sscanf(buffer, "0x%x", &i) == 1)
    {
        // valid data
        VGMHeader.PSGWhiteNoiseFeedback = static_cast<uint16_t>(i);
    }
    if (get_int(HeaderWnd, edtPSGSRWidth, &i))
    {
        VGMHeader.PSGShiftRegisterWidth = static_cast<uint8_t>(i);
    }

    write_vgm_header(g_current_filename, VGMHeader, callback);
}

void ClearGD3Strings()
{
    for (const int gd3EditControl : GD3EditControls)
    {
        SetDlgItemText(GD3Wnd, gd3EditControl, "");
    }
}

void UpdateGD3()
{
    if (!Utils::file_exists(g_current_filename))
    {
        return;
    }

    set_status_text("Updating GD3 tag...");

    gzFile in = gzopen(g_current_filename.c_str(), "rb");
    VGMHeader VGMHeader;
    if (!ReadVGMHeader(in, &VGMHeader, callback))
    {
        gzclose(in);
        return;
    }

    gzrewind(in);

    auto outFilename = Utils::make_suffixed_filename(g_current_filename, "tagged");
    gzFile out = gzopen(outFilename.c_str(), "wb0");

    // Copy everything up to the GD3 tag
    for (auto i = 0; i < static_cast<int>(VGMHeader.GD3Offset > 0
                                              ? VGMHeader.GD3Offset + GD3DELTA
                                              : VGMHeader.EoFOffset + EOFDELTA); ++i)
    {
        gzputc(out, gzgetc(in));
    }

    VGMHeader.GD3Offset = gztell(out) - GD3DELTA; // record GD3 position

    std::wostringstream AllGD3Strings;
    int ConversionErrors = 0;

    for (auto i = 0; i < NumGD3Strings; ++i)
    {
        // Get string from widget
        const auto length = GetWindowTextLengthW(GetDlgItem(GD3Wnd, GD3EditControls[i])) + 1;
        std::wstring s(length, L'\0');
        if (length > 1)
        {
            // Non-empty string
            if (GetDlgItemTextW(GD3Wnd, GD3EditControls[i], s.data(), static_cast<int>(s.size())) == 0)
            {
                ConversionErrors++;
            }

            // Special handling for any strings
            switch (i)
            {
            case 10: // Notes - change \r\n to \n
                s.replace(s.begin(), s.end(), {L'\r'});
                break;
            }
        }
        AllGD3Strings << s;
    }

    const auto& data = AllGD3Strings.str();

    TGD3Header GD3Header{};

    strncpy(GD3Header.id_string, "Gd3 ", 4);
    GD3Header.version = 0x0100;
    GD3Header.length = static_cast<uint32_t>(data.length() * 2);

    gzwrite(out, &GD3Header, sizeof(GD3Header)); // write GD3 header
    gzwrite(out, data.data(), GD3Header.length); // write GD3 strings

    VGMHeader.EoFOffset = gztell(out) - EOFDELTA; // Update EoF offset in header

    gzclose(in);
    gzclose(out);

    write_vgm_header(outFilename, VGMHeader, callback); // Write changed header

    Utils::replace_file(g_current_filename, outFilename);

    if (ConversionErrors > 0)
    {
        show_error_message_box(
            "There were %d error(s) when converting the GD3 tag to Unicode. Some fields may be truncated or incorrect.",
            ConversionErrors);
    }
    set_status_text("GD3 tag updated");
}

// Remove data for checked boxes
void Strip(const char* filename, const char* Outfilename)
{
    VGMHeader VGMHeader;
    signed int b0, b1, b2;
    char LatchedChannel = 0;
    long int NewLoopOffset = 0;

    // Set up masks
    unsigned char PSGMask = (1 << NumPSGTypes) - 1;
    for (auto i = 0; i < NumPSGTypes; i++)
    {
        if ((IsDlgButtonChecked(StripWnd, PSGCheckBoxes[i])) || (!IsWindowEnabled(
            GetDlgItem(StripWnd, PSGCheckBoxes[i]))))
        {
            PSGMask ^= (1 << i);
        }
    }
    unsigned long YM2413Mask = (1 << NumYM2413Types) - 1;
    for (auto i = 0; i < NumYM2413Types; i++)
    {
        if ((IsDlgButtonChecked(StripWnd, YM2413CheckBoxes[i])) || (!IsWindowEnabled(
            GetDlgItem(StripWnd, YM2413CheckBoxes[i]))))
        {
            YM2413Mask ^= (1 << i);
        }
    }
    unsigned char YM2612Mask = (1 << NumYM2612Types) - 1;
    for (auto i = 0; i < NumYM2612Types; i++)
    {
        if ((IsDlgButtonChecked(StripWnd, YM2612CheckBoxes[i])) || (!IsWindowEnabled(
            GetDlgItem(StripWnd, YM2612CheckBoxes[i]))))
        {
            YM2612Mask ^= (1 << i);
        }
    }
    unsigned char YM2151Mask = (1 << NumYM2151Types) - 1;
    for (auto i = 0; i < NumYM2151Types; i++)
    {
        if ((IsDlgButtonChecked(StripWnd, YM2151CheckBoxes[i])) || (!IsWindowEnabled(
            GetDlgItem(StripWnd, YM2151CheckBoxes[i]))))
        {
            YM2151Mask ^= (1 << i);
        }
    }

    gzFile in = gzopen(filename, "rb");

    // Read header
    if (!ReadVGMHeader(in, &VGMHeader, callback))
    {
        gzclose(in);
        return;
    }
    gzseek(in, VGM_DATA_OFFSET, SEEK_SET);

    gzFile out = gzopen(Outfilename, "wb0"); // No compression, since I'll recompress it later

    // Write header... update it later
    gzwrite(out, &VGMHeader, sizeof(VGMHeader));
    gzseek(out, VGM_DATA_OFFSET, SEEK_SET);

    // process file
    do
    {
        if ((VGMHeader.LoopOffset) && (gztell(in) == static_cast<long>(VGMHeader.LoopOffset) + LOOPDELTA))
        {
            NewLoopOffset = gztell(out) -
                LOOPDELTA;
        }
        b0 = gzgetc(in);
        switch (b0)
        {
        case VGM_GGST: // GG stereo
            b1 = gzgetc(in);
            if (PSGMask & (1 << 4))
            {
                gzputc(out, VGM_GGST);
                gzputc(out, b1);
            }
            break;
        case VGM_PSG: // PSG write (1 byte data)
            b1 = gzgetc(in);
            if (b1 & 0x80)
            {
                LatchedChannel = ((b1 >> 5) & 0x03);
            }
            if (PSGMask & (1 << LatchedChannel))
            {
                gzputc(out, VGM_PSG);
                gzputc(out, b1);
            }
            break;
        case VGM_YM2413: // YM2413
            b1 = gzgetc(in);
            b2 = gzgetc(in);
            switch (b1 >> 4)
            {
            // go by 1st digit first
            case 0x0: // User tone / percussion
                switch (b1)
                {
                case 0x00:
                case 0x01:
                case 0x02:
                case 0x03:
                case 0x04:
                case 0x05:
                case 0x06:
                case 0x07:
                    if (YM2413Mask & (1 << 14))
                    {
                        gzputc(out, VGM_YM2413);
                        gzputc(out, b1);
                        gzputc(out, b2);
                    }
                    break;
                case 0x0E: // Percussion
                    //if ((b2&0x20) && !(YM2413Mask&0x3e00)) break;  // break if no percussion is wanted and it's rhythm mode
                    // Original data  --RBSTCH
                    //          00111100
                    // Mask        00110101 = (YM2413Mask>>9)&0x1f|0x20
                    // Result      00110100
                    b2 &= (YM2413Mask >> 9) & 0x1f | 0x20;
                    gzputc(out, VGM_YM2413);
                    gzputc(out, b1);
                    gzputc(out, b2);
                    break;
                default:
                    if (YM2413Mask & (1 << 15))
                    {
                        gzputc(out, VGM_YM2413);
                        gzputc(out, b1);
                        gzputc(out, b2);
                    }
                    break;
                }
                break;
            case 0x1: // Tone F-number low 8 bits
                if (b1 > 0x18)
                {
                    if (YM2413Mask & (1 << 15))
                    {
                        gzputc(out, VGM_YM2413);
                        gzputc(out, b1);
                        gzputc(out, b2);
                    }
                }
                else
                {
                    if ((YM2413Mask & (1 << (b1 & 0xf))) || // if channel is on
                        ((b1 >= 0x16) && (YM2413Mask & 0x3e00))) // or last 3 channels AND percussion is wanted
                    {
                        gzputc(out, VGM_YM2413);
                        gzputc(out, b1);
                        gzputc(out, b2);
                    }
                }
                break;
            case 0x2: // Tone more stuff including key
                if (b1 > 0x28)
                {
                    if (YM2413Mask & (1 << 15))
                    {
                        gzputc(out, VGM_YM2413);
                        gzputc(out, b1);
                        gzputc(out, b2);
                    }
                }
                else
                {
                    if ((YM2413Mask & (1 << (b1 & 0xf))) || // if channel is on
                        ((b1 >= 0x26) && (YM2413Mask & 0x3e00) /*&& !(b1&0x10)*/))
                    // or last 3 channels AND percussion is wanted AND key is off
                    {
                        gzputc(out, VGM_YM2413);
                        gzputc(out, b1);
                        gzputc(out, b2);
                    }
                }
                break;
            case 0x3: // Tone instruments and volume
                if (b1 > YM2413NumRegs)
                {
                    if (YM2413Mask & (1 << 15))
                    {
                        gzputc(out, VGM_YM2413);
                        gzputc(out, b1);
                        gzputc(out, b2);
                    }
                }
                else
                {
                    if ((YM2413Mask & (1 << (b1 & 0xf))) || // if channel is on
                        ((b1 >= 0x36) && (YM2413Mask & 0x3e00))) // or last 3 channels AND percussion is on
                    {
                        gzputc(out, VGM_YM2413);
                        gzputc(out, b1);
                        gzputc(out, b2);
                    }
                }
                break;
            default:
                if (YM2413Mask & (1 << 15))
                {
                    gzputc(out, VGM_YM2413);
                    gzputc(out, b1);
                    gzputc(out, b2);
                }
                break;
            }
            break;
        case VGM_YM2612_0: // YM2612 port 0
        case VGM_YM2612_1: // YM2612 port 1
            b1 = gzgetc(in);
            b2 = gzgetc(in);
            if (YM2612Mask & (1 << 0))
            {
                gzputc(out, b0);
                gzputc(out, b1);
                gzputc(out, b2);
            }
            break;
        case VGM_YM2151: // YM2151
            b1 = gzgetc(in);
            b2 = gzgetc(in);
            if (YM2151Mask & (1 << 0))
            {
                gzputc(out, b0);
                gzputc(out, b1);
                gzputc(out, b2);
            }
            break;
        case 0x55: // Reserved up to 0x5f
        case 0x56: // All have 2 bytes of data
        case 0x57: // which I discard :)
        case 0x58:
        case 0x59:
        case 0x5a:
        case 0x5b:
        case 0x5c:
        case 0x5d:
        case 0x5e:
        case 0x5f:
            gzseek(in, 2, SEEK_CUR);
            break;
        case VGM_PAUSE_WORD: // Wait n samples
            b1 = gzgetc(in);
            b2 = gzgetc(in);
            gzputc(out, b0);
            gzputc(out, b1);
            gzputc(out, b2);
            break;
        case VGM_PAUSE_60TH: // Wait 1/60 s
        case VGM_PAUSE_50TH: // Wait 1/50 s
            gzputc(out, b0);
            break;
        //    case VGM_PAUSE_BYTE:  // Wait n samples
        //      b1=gzgetc(in);
        //      gzputc(out, b0);gzputc(out, b1);
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
            gzputc(out, b0);
            b0 = EOF; // make it break out
            break;
        default:
            break;
        }
    }
    while (b0 != EOF);

    // If removed PSG or FM then set the clock appropriately
    if (!PSGMask)
    {
        VGMHeader.PSGClock = 0;
    }
    if (!YM2413Mask)
    {
        VGMHeader.YM2413Clock = 0;
    }
    if (!YM2612Mask)
    {
        VGMHeader.YM2612Clock = 0;
    }
    if (!YM2151Mask)
    {
        VGMHeader.YM2151Clock = 0;
    }

    // Then copy the GD3 over
    if (VGMHeader.GD3Offset)
    {
        TGD3Header GD3Header;
        int NewGD3Offset = gztell(out) - GD3DELTA;
        gzseek(in, VGMHeader.GD3Offset + GD3DELTA, SEEK_SET);
        gzread(in, &GD3Header, sizeof(GD3Header));
        gzwrite(out, &GD3Header, sizeof(GD3Header));
        for (auto i = 0u; i < GD3Header.length; ++i)
        {
            // Copy strings
            gzputc(out, gzgetc(in));
        }
        VGMHeader.GD3Offset = NewGD3Offset;
    }
    VGMHeader.LoopOffset = NewLoopOffset;
    VGMHeader.EoFOffset = gztell(out) - EOFDELTA;

    gzclose(in);
    gzclose(out);

    // Amend it with the updated header
    write_vgm_header(Outfilename, VGMHeader, callback);
}

void StripChecked(const char* filename)
{
    char Tmpfilename[MAX_PATH + 10], Outfilename[MAX_PATH + 10];
    VGMHeader VGMHeader;

    if (!Utils::file_exists(filename))
    {
        return;
    }

    set_status_text("Stripping chip data...");

    gzFile in = gzopen(filename, "rb");
    if (!ReadVGMHeader(in, &VGMHeader, callback))
    {
        gzclose(in);
        return;
    }
    gzclose(in);

    // Set up filenames
    strcpy(Tmpfilename, filename);
    char* p = Tmpfilename + strlen(Tmpfilename);
    while (p >= Tmpfilename)
    {
        if (*p == '.')
        {
            strcpy(p, ".tmp5");
            break;
        }
        p--;
    }
    strcpy(Outfilename, filename);
    p = Outfilename + strlen(Outfilename);
    while (p >= Outfilename)
    {
        if (*p == '.')
        {
            strcpy(p, " (stripped).vgm");
            break;
        }
        p--;
    }

    // Strip data
    Strip(filename, Outfilename);
    /*
      // Optimise output file
      if (VGMHeader.LoopLength) Trim(Tmpfilename, 0, VGMHeader.TotalLength-VGMHeader.LoopLength, VGMHeader.TotalLength, TRUE, FALSE);
      else Trim(Tmpfilename, 0, -1, VGMHeader.TotalLength, TRUE, FALSE);
    
      // Strip data again because optimiser added it back
      Strip(Tmpfilename, Outfilename);
      
      // Clean up
      DeleteFile(Tmpfilename);
    */

    set_status_text("Data stripping complete");

    if (show_question_message_box("Stripped VGM data written to\n%s\nDo you want to open it in the associated program?",
        Outfilename) == IDYES)
    {
        ShellExecute(hWndMain, "open", Outfilename, nullptr, nullptr, SW_NORMAL);
    }
}

void ccb(const int CheckBoxes[], const unsigned long WriteCount[], int count, int mode)
{
    unsigned long Cutoff = 0;

    for (auto i = 0; i < count; ++i)
    {
        if (Cutoff < WriteCount[i])
        {
            Cutoff = WriteCount[i];
        }
    }
    Cutoff = Cutoff / 50; // 2% of largest for that chip

    for (auto i = 0; i < count; ++i)
    {
        if (IsWindowEnabled(GetDlgItem(StripWnd, CheckBoxes[i])))
        {
            switch (mode)
            {
            case 1: // check all
                CheckDlgButton(StripWnd, CheckBoxes[i], 1);
                break;
            case 2: // check none
                CheckDlgButton(StripWnd, CheckBoxes[i], 0);
                break;
            case 3: // invert selection
                CheckDlgButton(StripWnd, CheckBoxes[i], !IsDlgButtonChecked(StripWnd, CheckBoxes[i]));
                break;
            case 4: // guess
                {
                    char tempstr[64];
                    GetDlgItemText(StripWnd, CheckBoxes[i], tempstr, 64);
                    CheckDlgButton(StripWnd, CheckBoxes[i], ((WriteCount[i] < Cutoff) || (strstr(tempstr, "Invalid"))));
                    break;
                }
            }
        }
    }
}

void ChangeCheckBoxes(int mode)
{
    ccb(PSGCheckBoxes, PSGWrites, NumPSGTypes, mode);
    ccb(YM2413CheckBoxes, YM2413Writes, NumYM2413Types, mode);
    ccb(YM2612CheckBoxes, YM2612Writes, NumYM2612Types, mode);
    ccb(YM2151CheckBoxes, YM2151Writes, NumYM2151Types, mode);
}

/*
// Go through file, convert 50<->60Hz
void ConvertRate(char *filename) {
  gzFile in, *out;
  struct VGMHeader VGMHeader;
  char *Outfilename;
  char *p;
  int b0, b1, b2;
  long PauseLength=0;
  int BytesAdded=0, BytesAddedBeforeLoop=0;  // for when I have to expand wait nnnn commands

  if (!FileExists(filename)) return;

  in=gzopen(filename, "rb");

  // Read header
  gzread(in, &VGMHeader, sizeof(VGMHeader));
  gzseek(in, VGM_DATA_OFFSET, SEEK_SET);

  if (strncmp(VGMHeader.VGMIdent, "Vgm ", 4)!=0) {  // no VGM marker
    sprintf(MessageBuffer, "File is not a VGM file! (no \"Vgm \")");
    show_message_box();
    gzclose(in);
    return;
  }

  // See what I'm converting from
  switch (VGMHeader.RecordingRate) {
    case 50:
      set_status_text("Converting rate from 50 to 60Hz...");
      break;
    case 60:
      set_status_text("Converting rate from 50 to 60Hz...");
      break;
    default:
      sprintf(MessageBuffer, "Cannot convert this file because its recording rate is not set");
      show_message_box();
      gzclose(in);
      return;
  }

  Outfilename=malloc(strlen(filename)+40);
  strcpy(Outfilename, filename);
  for (p=Outfilename+strlen(Outfilename); p>=Outfilename; --p) {
    if (*p=='.') {
      sprintf(MessageBuffer, " (converted to %dHz).vgm", (VGMHeader.RecordingRate==50?60:50));
      strcpy(p, MessageBuffer);
      break;
    }
  }

  out=gzopen(Outfilename, "wb0");  // No compression, since I'll recompress it later

  // Write header... update it later
  gzwrite(out, &VGMHeader, sizeof(VGMHeader));
  gzseek(out, VGM_DATA_OFFSET, SEEK_SET);

  do {
    if (gztell(in)==VGMHeader.LoopOffset+LOOPDELTA) {  // Check to see if it's the loop point
      BytesAddedBeforeLoop=BytesAdded;
    }

    b0=gzgetc(in);
    switch (b0) {
    case VGM_GGST:  // GG stereo
    case VGM_PSG:  // PSG write
      b1=gzgetc(in);
      gzputc(out, b0);
      gzputc(out, b1);
      break;
    case VGM_YM2413:  // YM2413
    case VGM_YM2612_0:  // YM2612 port 0
    case VGM_YM2612_1:  // YM2612 port 1
    case VGM_YM2151:  // YM2151
      b1=gzgetc(in);
      b2=gzgetc(in);
      gzputc(out, b0);
      gzputc(out, b1);
      gzputc(out, b2);
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
        gzputc(out, VGM_PAUSE_WORD);
        gzputc(out, 0xff);
        gzputc(out, 0xff);
        BytesAdded+=3;
        PauseLength-=65535;
      }
      gzputc(out, VGM_PAUSE_WORD);
      gzputc(out, (PauseLength & 0xff));
      gzputc(out, (PauseLength >> 8));
      break;

    
    case VGM_PAUSE_60TH:  // Wait 1/60 s
      if (VGMHeader.RecordingRate==60) gzputc(out, VGM_PAUSE_50TH);
      else {
        sprintf(MessageBuffer, "Wait 1/60th command found in 50Hz file!");
        show_message_box();
      }
      break;
    case VGM_PAUSE_50TH:  // Wait 1/50 s
      if (VGMHeader.RecordingRate==50) gzputc(out, VGM_PAUSE_60TH);
      else {
        sprintf(MessageBuffer, "Wait 1/50th command found in 60Hz file!");
        show_message_box();
      }
      break;
    case VGM_END:  // End of sound data
      b0=EOF;  // break out of loop
      break;
    default:
      break;
    }
  } while (b0!=EOF);
  gzputc(out, VGM_END);

  // Copy GD3 tag
  if (VGMHeader.GD3Offset) {
    struct TGD3Header GD3Header;
    int i;
    int NewGD3Offset=gztell(out)-GD3DELTA;
    set_status_text("Copying GD3 tag...");
    gzseek(in, VGMHeader.GD3Offset+GD3DELTA, SEEK_SET);
    gzread(in, &GD3Header, sizeof(GD3Header));
    gzwrite(out, &GD3Header, sizeof(GD3Header));
    for (i=0; i<GD3Header.Length; ++i) {  // Copy strings
      gzputc(out, gzgetc(in));
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
  WriteVGMHeader(Outfilename, VGMHeader);

  CountLength(Outfilename, 0);  // fix lengths

  compress(g_current_filename);

  set_status_text("Rate conversion complete");

  sprintf(MessageBuffer, "File converted to\n%s", Outfilename);
  show_message_box();

  free(Outfilename);
}

*/

void PasteUnicode()
{
    char textbuffer[2048] = "";

    if (OpenClipboard(hWndMain))
    {
        // Open clipboard
        HANDLE DataHandle = GetClipboardData(CF_UNICODETEXT); // See if there's suitable data there
        if (DataHandle)
        {
            // If there is...
            auto wp = static_cast<wchar_t*>(GlobalLock(DataHandle));
            while (static_cast<int>(*wp))
            {
                if (static_cast<int>(*wp) < 128)
                {
                    strcat(textbuffer, (char*)*wp); // Nasty hack!
                }
                else
                {
                    sprintf(textbuffer, "%s&#x%04x;", textbuffer, static_cast<int>(*wp));
                }
                wp++;
            }

            GlobalUnlock(DataHandle);
        }
        else
        {
            // No unicode on clipboard
            strcat(textbuffer, "No Unicode on clipboard");
        }

        CloseClipboard();
    }
    SetDlgItemText(GD3Wnd, edtPasteUnicode, textbuffer);
}

void CopyLengthsToClipboard()
{
    char String[64] = "";
    char MessageBuffer[1024];

    GetDlgItemText(GD3Wnd, edtGD3TitleEn, MessageBuffer, 35);
    sprintf(String, "%-35s", MessageBuffer);
    GetDlgItemText(HeaderWnd, edtLengthTotal, MessageBuffer, 5);
    if (!strlen(MessageBuffer))
    {
        return; // quit if no length in box
    }
    sprintf(String, "%s %s", String, MessageBuffer);
    GetDlgItemText(HeaderWnd, edtLengthLoop, MessageBuffer, 5);
    sprintf(String, "%s   %s\r\n", String, MessageBuffer);

    auto strlength = (strlen(String) + 1) * sizeof(char);
    if (!OpenClipboard(hWndMain))
    {
        return;
    }
    EmptyClipboard();
    HANDLE CopyForClipboard = GlobalAlloc(GMEM_DDESHARE, strlength);
    if (!CopyForClipboard)
    {
        CloseClipboard();
        return;
    }
    auto p = static_cast<char*>(GlobalLock(CopyForClipboard));
    memcpy(p, String, strlength);
    GlobalUnlock(CopyForClipboard);
    SetClipboardData(CF_TEXT, CopyForClipboard);
    CloseClipboard();

    String[strlen(String) - 2] = '\0';
    sprintf(MessageBuffer, "Copied: \"%s\"", String);
    set_status_text(MessageBuffer);
}

// TODO: make this nicer?
;


void ConvertDroppedFiles(HDROP HDrop)
{
    int numConverted = 0;
    const int startTime = GetTickCount();

    // Get number of files dropped
    const int numFiles = DragQueryFile(HDrop, 0xFFFFFFFF, nullptr, 0);

    // Go through files
    for (int i = 0; i < numFiles; ++i)
    {
        // Get filename length
        const int filenameLength = DragQueryFile(HDrop, i, nullptr, 0);
        // Make a string to hold it
        std::string droppedFilename(filenameLength, '\0');
        // Get it into the string
        DragQueryFile(HDrop, i, droppedFilename.data(), filenameLength);
        //  Convert it
        if (Convert::to_vgm(droppedFilename, callback))
        {
            ++numConverted;
        }
    }

    add_convert_text("%d of %d file(s) successfully converted in %dms\r\n", numConverted, numFiles, GetTickCount() - startTime);

    DragFinish(HDrop);
}

LRESULT CALLBACK DialogProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

void make_tabbed_dialogue()
{
    // Load images for tabs
    InitCommonControls(); // required before doing imagelist stuff
    HIMAGELIST il = ImageList_LoadImage(HInst, (LPCSTR)tabimages, 16, 0, RGB(255, 0, 255), IMAGE_BITMAP,
        LR_CREATEDIBSECTION);

    HWND tabCtrlWnd = GetDlgItem(hWndMain, tcMain);
    TabCtrl_SetImageList(tabCtrlWnd, il);
    // Add tabs
    TC_ITEM newTab;
    newTab.mask = TCIF_TEXT | TCIF_IMAGE;
    newTab.pszText = (char*)"Header";
    newTab.iImage = 0;
    TabCtrl_InsertItem(tabCtrlWnd, 0, &newTab);
    newTab.pszText = (char*)"Trim/optimise";
    newTab.iImage = 1;
    TabCtrl_InsertItem(tabCtrlWnd, 1, &newTab);
    newTab.pszText = (char*)"Data usage/strip";
    newTab.iImage = 2;
    TabCtrl_InsertItem(tabCtrlWnd, 2, &newTab);
    newTab.pszText = (char*)"GD3 tag";
    newTab.iImage = 3;
    TabCtrl_InsertItem(tabCtrlWnd, 3, &newTab);
    newTab.pszText = (char*)"Conversion";
    newTab.iImage = 4;
    TabCtrl_InsertItem(tabCtrlWnd, 4, &newTab);
    newTab.pszText = (char*)"More functions";
    newTab.iImage = 5;
    TabCtrl_InsertItem(tabCtrlWnd, 5, &newTab);

    // We need to locate all the child tabs aligned to the tab control.
    // So first we get the area of the tab control...
    RECT tabDisplayRect;
    GetWindowRect(tabCtrlWnd, &tabDisplayRect);
    // And adjust it to the "display area"
    TabCtrl_AdjustRect(tabCtrlWnd, FALSE, &tabDisplayRect);
    // Then make it relative to the main window
    MapWindowPoints(HWND_DESKTOP, hWndMain, (LPPOINT)&tabDisplayRect, 2);

    // Create child windows
    TabChildWnds[0] = CreateDialog(HInst, (LPCTSTR) DlgVGMHeader, hWndMain, DialogProc);
    TabChildWnds[1] = CreateDialog(HInst, (LPCTSTR) DlgTrimming, hWndMain, DialogProc);
    TabChildWnds[2] = CreateDialog(HInst, (LPCTSTR) DlgStripping, hWndMain, DialogProc);
    TabChildWnds[3] = CreateDialog(HInst, (LPCTSTR) DlgGD3, hWndMain, DialogProc);
    TabChildWnds[4] = CreateDialog(HInst, (LPCTSTR) DlgConvert, hWndMain, DialogProc);
    TabChildWnds[5] = CreateDialog(HInst, (LPCTSTR) DlgMisc, hWndMain, DialogProc);

    // Put them in the right place, and hide them
    for (const auto& tabChildWnd : TabChildWnds)
    {
        EnableThemeDialogTexture(tabChildWnd, ETDT_ENABLETAB);

        SetWindowPos(tabChildWnd, HWND_TOP, tabDisplayRect.left, tabDisplayRect.top,
            tabDisplayRect.right - tabDisplayRect.left, tabDisplayRect.bottom - tabDisplayRect.top,
            SWP_HIDEWINDOW);
    }
    // Show the first one, though
    SetWindowPos(TabChildWnds[0], HWND_TOP, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_SHOWWINDOW);
}


// parses items, splitting at separators
// fills combobox id in parent with strings
void FillComboBox(HWND parent, int id, const char* items, const char* separators)
{
    auto buf = static_cast<char*>(malloc(strlen(items) + 1));
    strcpy(buf, items); // I need a writeable local copy
    char* p = strtok(buf, separators);

    while (p)
    {
        SendDlgItemMessage(parent, id, CB_ADDSTRING, 0, (LPARAM)p);
        p = strtok(nullptr, separators);
    }

    free(buf);
}

LRESULT CALLBACK DialogProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    // process messages
    case WM_INITDIALOG: // Initialise dialogue
        {
            if (hWndMain != nullptr)
            {
                return FALSE; // nothing for child windows
            }
            hWndMain = hWnd; // remember our window handle
            HICON hIcon = LoadIcon(HInst, MAKEINTRESOURCE(MAINICON));
            SendMessage(hWnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
            SendMessage(hWnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
            SetDlgItemText(hWndMain, edtFileName, "Drop a file onto the window to load");
            SetWindowText(hWndMain, g_program_name.c_str());
            make_tabbed_dialogue();
            // Fill combo box - see below for Japanese translations
            FillComboBox(GD3Wnd, cbGD3SystemEn,
                "Sega Master System, Sega Game Gear, Sega Master System / Game Gear, Sega Mega Drive / Genesis, Sega Game 1000, Sega Computer 3000, Sega System 16, Capcom Play System 1, Colecovision, BBC Model B, BBC Model B+, BBC Master 128, Custom...",
                ", ");
            FillComboBox(HeaderWnd, edtPlaybackRate, "0 (unknown), 50 (PAL), 60 (NTSC)", ", ");
            FillComboBox(HeaderWnd, edtPSGClock, "0 (disabled), 3546893 (PAL), 3579545 (NTSC), 4000000 (BBC)", ", ");
            FillComboBox(HeaderWnd, edtYM2413Clock, "0 (disabled), 3546893 (PAL), 3579545 (NTSC)", ", ");
            FillComboBox(HeaderWnd, edtYM2612Clock, "0 (disabled), 7600489 (PAL), 7670453 (NTSC)", ", ");
            FillComboBox(HeaderWnd, edtYM2151Clock, "0 (disabled), 7670453 (NTSC)", ", ");

            FillComboBox(HeaderWnd, edtPSGFeedback, "0 (disabled), 0x0009 (Sega VDP), 0x0003 (discrete chip)", ", ");
            FillComboBox(HeaderWnd, edtPSGSRWidth, "0 (disabled), 16 (Sega VDP), 15 (discrete chip)", ", ");

            // Focus 1st control
            SetActiveWindow(hWndMain);
            SetFocus(GetDlgItem(hWndMain, edtFileName));
            return TRUE;
        }
    case WM_CLOSE: // Window is being asked to close ([x], Alt+F4, etc)
        PostQuitMessage(0);
        return TRUE;
    case WM_DESTROY: // Window is being destroyed
        PostQuitMessage(0);
        return TRUE;
    case WM_DROPFILES: // File dropped
        if (hWnd == ConvertWnd)
        {
            ConvertDroppedFiles((HDROP)wParam);
        }
        else
        {
            int FilenameLength = DragQueryFile((HDROP)wParam, 0, nullptr, 0) + 1;
            auto DroppedFilename = static_cast<char*>(malloc(FilenameLength)); // Allocate memory for the filename
            DragQueryFile((HDROP)wParam, 0, DroppedFilename, FilenameLength);
            // Get filename of first file, discard the rest
            DragFinish((HDROP)wParam); // Tell Windows I've finished
            LoadFile(DroppedFilename);
            free(DroppedFilename); // deallocate buffer
        }
        return TRUE;
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case btnUpdateHeader:
            UpdateHeader();
            Utils::compress(g_current_filename);
            LoadFile(g_current_filename.c_str());
            break;
        case btnCheckLengths:
            check_lengths(g_current_filename, TRUE, callback);
            LoadFile(g_current_filename.c_str());
            break;
        case btnTrim:
            {
                int loop = -1;
                BOOL b1, b2, b3 = TRUE;
                const int start = static_cast<int>(GetDlgItemInt(TrimWnd, edtTrimStart, &b1, FALSE));
                const int end = static_cast<int>(GetDlgItemInt(TrimWnd, edtTrimEnd, &b2, FALSE));
                if (IsDlgButtonChecked(TrimWnd, cbLoop))
                {
                    // want looping
                    loop = static_cast<int>(GetDlgItemInt(TrimWnd, edtTrimLoop, &b3, FALSE));
                }

                if (!b1 || !b2 || !b3)
                {
                    // failed to get values
                    show_error_message_box("Invalid edit points!");
                    break;
                }
                trim(g_current_filename, start, loop, end, false, IsDlgButtonChecked(TrimWnd, cbLogTrims), callback);
            }
            break;
        case btnWriteToText:
            write_to_text(g_current_filename, callback);
            break;
        case btnOptimise:
            Optimize(g_current_filename);
            Utils::compress(g_current_filename);
            LoadFile(g_current_filename.c_str());
            break;
        case btnDecompress:
            Utils::decompress(g_current_filename);
            LoadFile(g_current_filename.c_str());
            break;
        case btnRoundTimes:
            {
                BOOL b;
                const int Edits[3] = {edtTrimStart, edtTrimLoop, edtTrimEnd};
                if (!g_current_file_vgm_header.RecordingRate)
                {
                    break; // stop if rate = 0
                }
                int FrameLength = 44100 / g_current_file_vgm_header.RecordingRate;
                for (int i = 0; i < 3; ++i)
                {
                    int Time = GetDlgItemInt(TrimWnd, Edits[i], &b, FALSE);
                    if (b)
                    {
                        const double frames = static_cast<double>(Time) / FrameLength;
                        const int roundedFrames = std::lround(frames) * FrameLength;
                        SetDlgItemInt(TrimWnd, Edits[i], roundedFrames, FALSE);
                    }
                }
            }
            break;
        case btnPlayFile:
            ShellExecute(hWndMain, "Play", g_current_filename.c_str(), nullptr, nullptr, SW_NORMAL);
            break;
        case btnUpdateGD3:
            UpdateGD3();
            Utils::compress(g_current_filename);
            LoadFile(g_current_filename.c_str());
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
            StripChecked(g_current_filename.c_str());
            break;
        case cbPSGTone:
            {
                const int Checked = IsDlgButtonChecked(StripWnd, cbPSGTone);
                for (int i = 0; i < 3; ++i)
                {
                    if (IsWindowEnabled(GetDlgItem(StripWnd, PSGCheckBoxes[i])))
                    {
                        CheckDlgButton(
                            StripWnd, PSGCheckBoxes[i], Checked);
                    }
                }
            }
            break;
        case cbYM2413Tone:
            {
                const int Checked = IsDlgButtonChecked(StripWnd, cbYM2413Tone);
                for (int i = 0; i < 9; ++i)
                {
                    if (IsWindowEnabled(GetDlgItem(StripWnd, YM2413CheckBoxes[i])))
                    {
                        CheckDlgButton(StripWnd, YM2413CheckBoxes[i], Checked);
                    }
                }
            }
            break;
        case cbYM2413Percussion:
            {
                const int Checked = IsDlgButtonChecked(StripWnd, cbYM2413Percussion);
                for (int i = 9; i < 14; ++i)
                {
                    if (IsWindowEnabled(GetDlgItem(StripWnd, YM2413CheckBoxes[i])))
                    {
                        CheckDlgButton(StripWnd, YM2413CheckBoxes[i], Checked);
                    }
                }
            }
            break;
        case btnRateDetect:
            {
                int i = detect_rate(g_current_filename, callback);
                if (i)
                {
                    SetDlgItemInt(HeaderWnd, edtPlaybackRate, i, FALSE);
                }
                // TODO: make sure VGM version is set high enough when updating header
            }
            break;
        case btnPasteUnicode:
            PasteUnicode();
            break;
        case cbGD3SystemEn:
            if (HIWORD(wParam) != CBN_SELENDOK)
            {
                break;
            }
            switch (SendDlgItemMessage(GD3Wnd, cbGD3SystemEn, CB_GETCURSEL, 0, 0))
            {
            case 0: SetDlgItemText(GD3Wnd, edtGD3SystemJp,
                    "&#x30bb;&#x30ac;&#x30de;&#x30b9;&#x30bf;&#x30fc;&#x30b7;&#x30b9;&#x30c6;&#x30e0;");
                break; // Sega Master System
            case 1: SetDlgItemText(GD3Wnd, edtGD3SystemJp, "&#x30bb;&#x30ac;&#x30b2;&#x30fc;&#x30e0;&#x30ae;&#x30a2;");
                break; // Sega Game Gear
            case 2: SetDlgItemText(GD3Wnd, edtGD3SystemJp,
                    "&#x30bb;&#x30ac;&#x30de;&#x30b9;&#x30bf;&#x30fc;&#x30b7;&#x30b9;&#x30c6;&#x30e0; / &#x30b2;&#x30fc;&#x30e0;&#x30ae;&#x30a2;");
                break; // Sega Master System / Game Gear
            case 3: SetDlgItemText(GD3Wnd, edtGD3SystemJp,
                    "&#x30bb;&#x30ac;&#x30e1;&#x30ac;&#x30c9;&#x30e9;&#x30a4;&#x30d6;");
                break; // Sega Megadrive (Japanese name)
            default: SetDlgItemText(GD3Wnd, edtGD3SystemJp, "");
                break;
            }
            break;
        case btnCopyLengths:
            CopyLengthsToClipboard();
            break;
        case btnRemoveGD3:
            remove_gd3(g_current_filename, callback);
            LoadFile(g_current_filename.c_str());
            break;
        case btnRemoveOffsets:
            remove_offset(g_current_filename, callback);
            LoadFile(g_current_filename.c_str());
            break;
        case btnOptimiseVGMData:
            //      OptimiseVGMData(g_current_filename);
            show_error_message_box("TODO: OptimiseVGMData() fixing");
            LoadFile(g_current_filename.c_str());
            break;
        case btnOptimisePauses:
            optimise_vgm_pauses(g_current_filename, callback);
            LoadFile(g_current_filename.c_str());
            break;
        case btnTrimOnly:
            {
                int Loop = -1;
                BOOL b1, b2, b3 = TRUE;
                int Start = GetDlgItemInt(TrimWnd, edtTrimStart, &b1, FALSE);
                int End = GetDlgItemInt(TrimWnd, edtTrimEnd, &b2, FALSE);
                if (IsDlgButtonChecked(TrimWnd, cbLoop))
                {
                    // want looping
                    Loop = GetDlgItemInt(TrimWnd, edtTrimLoop, &b3, FALSE);
                }

                if (!b1 || !b2 || !b3)
                {
                    // failed to get values
                    show_error_message_box("Invalid edit points!");
                    break;
                }

                if (IsDlgButtonChecked(TrimWnd, cbLogTrims))
                {
                    log_trim(g_current_filename, Start, Loop, End, callback);
                }

                new_trim(g_current_filename, Start, Loop, End, callback);
            }
            break;
        case btnCompress:
            Utils::compress(g_current_filename);
            LoadFile(g_current_filename.c_str());
            break;
        case btnNewTrim:
            {
                int Loop = -1;
                BOOL b1, b2, b3 = TRUE;
                int Start = GetDlgItemInt(TrimWnd, edtTrimStart, &b1, FALSE);
                int End = GetDlgItemInt(TrimWnd, edtTrimEnd, &b2, FALSE);
                if (IsDlgButtonChecked(TrimWnd, cbLoop))
                {
                    // want looping
                    Loop = GetDlgItemInt(TrimWnd, edtTrimLoop, &b3, FALSE);
                }

                if (!b1 || !b2 || !b3)
                {
                    // failed to get values
                    show_error_message_box("Invalid edit points!");
                    break;
                }
                new_trim(g_current_filename, Start, Loop, End, callback);
                remove_offset(g_current_filename, callback);
                //        OptimiseVGMData(g_current_filename);
                optimise_vgm_pauses(g_current_filename, callback);
            }
            break;
        case btnGetCounts:
            CheckWriteCounts(g_current_filename.c_str());
            break;
        case btnRoundToFrames:
            round_to_frame_accurate(g_current_filename, callback);
            break;
        } // end switch(LOWORD(wParam))
        {
            // Stuff to happen after every message/button press (?!): update "all" checkboxes for stripping
            int i;
            int Checked = 0;
            int Total = 0;
            for (i = 0; i < 3; ++i)
            {
                Checked += IsDlgButtonChecked(StripWnd, PSGCheckBoxes[i]);
                Total += IsWindowEnabled(GetDlgItem(StripWnd, PSGCheckBoxes[i]));
            }
            CheckDlgButton(StripWnd, cbPSGTone, (Checked == Total) && Total);
            Checked = 0;
            Total = 0;
            for (i = 0; i < 9; ++i)
            {
                Checked += IsDlgButtonChecked(StripWnd, YM2413CheckBoxes[i]);
                Total += IsWindowEnabled(GetDlgItem(StripWnd, YM2413CheckBoxes[i]));
            }
            CheckDlgButton(StripWnd, cbYM2413Tone, (Checked == Total) && Total);
            Checked = 0;
            Total = 0;
            for (i = 9; i < 14; ++i)
            {
                Checked += IsDlgButtonChecked(StripWnd, YM2413CheckBoxes[i]);
                Total += IsWindowEnabled(GetDlgItem(StripWnd, YM2413CheckBoxes[i]));
            }
            CheckDlgButton(StripWnd, cbYM2413Percussion, (Checked == Total) && Total);
        }
        return TRUE; // WM_COMMAND handled
    case WM_NOTIFY:
        switch (LOWORD(wParam))
        {
        case tcMain:
            switch (((LPNMHDR)lParam)->code)
            {
            case TCN_SELCHANGING: // hide current window
                SetWindowPos(TabChildWnds[TabCtrl_GetCurSel(GetDlgItem(hWndMain, tcMain))], HWND_TOP, 0, 0, 0, 0,
                    SWP_NOMOVE | SWP_NOSIZE | SWP_HIDEWINDOW);
                break;
            case TCN_SELCHANGE: // show current window
                {
                    int i = TabCtrl_GetCurSel(GetDlgItem(hWndMain, tcMain));
                    SetWindowPos(TabChildWnds[i], HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
                    SetFocus(TabChildWnds[i]);
                }
                break;
            }
            break;
        } // end switch (LOWORD(wParam))
        return TRUE; // WM_NOTIFY handled
    }
    return FALSE; // return FALSE to signify message not processed
}

/* // Old method, using Windows' built-in message handlers
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
  HInst=hInstance;
  DialogBox(hInstance, (LPCTSTR) MAINDIALOGUE, NULL, DialogProc);  // Open dialog
  if (GD3Strings) free(GD3Strings);
  return 0;
}*/


// I only handle commandlines that look like
// "\"path to file\"blahetcwhatever"
// ie. the filename must be quote-wrapped and I don't handle anything more than that
void HandleCommandline(char* commandline)
{
    if (!commandline || !*commandline)
    {
        return; // do nothing for null pointer or empty string
    }

    char* start = strchr(commandline, '"') + 1;
    if (start)
    {
        char* end = strchr(start, '"');
        if (end)
        {
            *end = 0;
            if (Utils::file_exists(start))
            {
                LoadFile(start);
            }
        }
    }
}

void DoCtrlTab()
{
    const HWND tabCtrlWnd = GetDlgItem(hWndMain, tcMain);
    int currentTab = TabCtrl_GetCurFocus(tabCtrlWnd);

    if (GetKeyState(VK_SHIFT) < 0)
    {
        --currentTab;
    }
    else
    {
        ++currentTab;
    }

    if (currentTab == -1)
    {
        currentTab = NumTabChildWnds - 1;
    }
    else if (currentTab == NumTabChildWnds)
    {
        currentTab = 0;
    }

    TabCtrl_SetCurFocus(tabCtrlWnd, currentTab);
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPSTR lpCmdLine, int nCmdShow)
{
    MSG msg;

    HInst = hInstance;
    CreateDialog(hInstance, (LPCTSTR) MAINDIALOGUE, NULL, DialogProc); // Open dialog
    HandleCommandline(lpCmdLine);
    ShowWindow(hWndMain, nCmdShow);
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        // you can use TranslateAccelerator if you prefer
        if (IsChild(hWndMain, msg.hwnd) && msg.message == WM_KEYDOWN && msg.wParam == VK_TAB && GetKeyState(VK_CONTROL)
            < 0)
        {
            DoCtrlTab();
        }
        else if (IsDialogMessage(hWndMain, &msg))
        {
            /* IsDialogMessage handled the message */
        }
        else
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    if (GD3Strings)
    {
        free(GD3Strings);
    }
    return 0;
}
