#include <Windows.h>
#include <cstdio>
#include <cstdarg>
#include <cmath>
#include "gui.h"

#include <CommCtrl.h>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <Uxtheme.h>

#include "convert.h"
#include "gd3.h"
#include "optimise.h"
#include "resource.h"
#include "trim.h"
#include "utils.h"
#include "vgm.h"
#include "writetotext.h"


// GD3 versions I can accept
#define MINGD3VERSION 0x100
#define REQUIREDGD3MAJORVER 0x100

Gui* Gui::_pThis = nullptr;
std::string Gui::_programName = "VGMTool 2 release 6 BETA";

int Gui::show_question_message_box(const std::string& s) const
{
    return MessageBox(_hWndMain, s.c_str(), _programName.c_str(), MB_ICONQUESTION + MB_YESNO);
}


bool Gui::get_int(HWND hDlg, int item, int* result)
{
    BOOL success;
    const auto value = GetDlgItemInt(hDlg, item, &success, FALSE);
    if (success == TRUE || value != 0)
    {
        // success == TRUE -> no errors
        // success == FALSE but value != 0 -> there were errors, but it managed to convert an integer
        // This is the case when the text is something like "1234 (text)"
        *result = static_cast<int>(value);
        return true;
    }
    // However, text like "0 (text)" looks like a total failure.
    // But I can test for that.
    char c[3];
    if (GetDlgItemText(hDlg, item, c, 3) == 2 && c[0] == '0' && (std::isdigit(c[1]) == 0))
    {
        // It looks like it was like that
        *result = 0;
        return true;
    }

    return false;
}

std::string Gui::get_utf8_string(HWND hDlg, int item)
{
    const auto length = GetWindowTextLength(GetDlgItem(hDlg, item)) + 1;
    std::string s(length, L'\0');
    if (length > 1)
    {
        // Non-empty string
        if (static_cast<int>(GetDlgItemText(hDlg, item, s.data(), static_cast<int>(s.size()))) == 0)
        {
            throw std::runtime_error("Failed to get text");
        }
        // It gets us an extra trailing \0, we remove that
        s.resize(length - 1);
    }
    return s;
}

std::wstring Gui::get_utf16_string(HWND hDlg, int item)
{
    const auto length = GetWindowTextLengthW(GetDlgItem(hDlg, item)) + 1;
    std::wstring s(length, L'\0');
    if (length > 1)
    {
        // Non-empty string
        if (static_cast<int>(GetDlgItemTextW(hDlg, item, s.data(), static_cast<int>(s.size()))) == 0)
        {
            throw std::runtime_error("Failed to get text");
        }
        // It gets us an extra trailing \0, we remove that
        s.resize(length - 1);
    }
    return s;
}

Gui::Gui(HINSTANCE hInstance, LPSTR lpCmdLine, int nShowCmd):
    _hInstance(hInstance),
    _showCommand(nShowCmd),
    _commandLine(lpCmdLine),
    // Populate vectors for ease of use later
    _psgCheckBoxes{cbPSG0, cbPSG1, cbPSG2, cbPSGNoise, cbPSGGGSt},
    _ym2413CheckBoxes{
        cbYM24130, cbYM24131, cbYM24132, cbYM24133, cbYM24134, cbYM24135, cbYM24136, cbYM24137, cbYM24138,
        cbYM2413HiHat, cbYM2413Cymbal, cbYM2413TomTom, cbYM2413SnareDrum, cbYM2413BassDrum, cbYM2413UserInst,
        cbYM2413InvalidRegs
    },
    _ym2612CheckBoxes{cbYM2612},
    _ym2151CheckBoxes{cbYM2151},
    _reservedCheckboxes{cbReserved},
    _gd3EditControls{
        edtGD3TitleEn, edtGD3TitleJp, edtGD3GameEn, edtGD3GameJp, cbGD3SystemEn, edtGD3SystemJp, edtGD3AuthorEn,
        edtGD3AuthorJp,edtGD3Date, edtGD3Creator, edtGD3Notes
    }
{
    _pThis = this;
}

// We have to have a non-instance method for this, then we dispatch to the real handler
LRESULT CALLBACK Gui::static_dialog_proc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (_pThis != nullptr)
    {
        return _pThis->dialog_proc(hWnd, uMsg, wParam, lParam);
    }
    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

void Gui::run()
{
    _hWndMain = CreateDialog(_hInstance, MAKEINTRESOURCE(MAINDIALOGUE), NULL, static_dialog_proc); // Open dialog
    SetWindowLongPtr(_hWndMain, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

    // Set window icon
    HICON hIcon = LoadIcon(_hInstance, MAKEINTRESOURCE(MAINICON));
    SendMessage(_hWndMain, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(hIcon));
    SendMessage(_hWndMain, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(hIcon));

    // Initialise controls
    SetDlgItemText(_hWndMain, edtFileName, "Drop a file onto the window to load");
    SetWindowText(_hWndMain, _programName.c_str());
    make_tabbed_dialog();
    // Fill combo box - see below for Japanese translations
    fill_combo_box(_gd3Wnd, cbGD3SystemEn,
        {
            "Sega Master System", "Sega Game Gear", "Sega Master System / Game Gear", "Sega Mega Drive / Genesis",
            "Sega Game 1000", "Sega Computer 3000", "Sega System 16", "Capcom Play System 1", "Colecovision",
            "BBC Model B", "BBC Model B+", "BBC Master 128"
        });
    fill_combo_box(_headerWnd, edtPlaybackRate, {"0 (unknown)", "50 (PAL)", "60 (NTSC)"});
    fill_combo_box(_headerWnd, edtPSGClock, {"0 (disabled)", "3546893 (PAL)", "3579545 (NTSC)", "4000000 (BBC)"});
    fill_combo_box(_headerWnd, edtYM2413Clock, {"0 (disabled)", "3546893 (PAL)", "3579545 (NTSC)"});
    fill_combo_box(_headerWnd, edtYM2612Clock, {"0 (disabled)", "7600489 (PAL), 7670453 (NTSC)"});
    fill_combo_box(_headerWnd, edtYM2151Clock, {"0 (disabled)", "7670453 (NTSC)"});

    fill_combo_box(_headerWnd, edtPSGFeedback, {"0 (disabled)", "0x0009 (Sega VDP)", "0x0003 (discrete chip)"});
    fill_combo_box(_headerWnd, edtPSGSRWidth, {"0 (disabled)", "16 (Sega VDP)", "15 (discrete chip)"});

    // Focus 1st control
    SetActiveWindow(_hWndMain);
    SetFocus(GetDlgItem(_hWndMain, edtFileName));

    if (!_commandLine.empty())
    {
        // Remove surrounding quotes
        if (_commandLine[0] == '"' && _commandLine[_commandLine.length() - 1] == '"')
        {
            _commandLine = _commandLine.substr(1, _commandLine.length() - 2);
        }

        if (Utils::file_exists(_commandLine))
        {
            load_file(_commandLine);
        }
    }

    ShowWindow(_hWndMain, _showCommand);
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if ((IsChild(_hWndMain, msg.hwnd) != 0) && msg.message == WM_KEYDOWN && msg.wParam == VK_TAB &&
            GetKeyState(VK_CONTROL)
            < 0)
        {
            // We have to peek the message here to do ctrl+tab (apparently)
            do_ctrl_tab();
        }
        else if (IsDialogMessage(_hWndMain, &msg))
        {
            /* IsDialogMessage handled the message */
        }
        else
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
}

LRESULT CALLBACK Gui::dialog_proc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    try
    {
        switch (message)
        {
        // process messages
        case WM_CLOSE: // Window is being asked to close ([x], Alt+F4, etc)
        case WM_DESTROY: // Window is being destroyed
            PostQuitMessage(0);
            return TRUE;
        case WM_DROPFILES: // File dropped
            if (hWnd == _convertWnd)
            {
                convert_dropped_files(reinterpret_cast<HDROP>(wParam));
            }
            else
            {
                const int filenameLength = DragQueryFile(reinterpret_cast<HDROP>(wParam), 0, nullptr, 0) + 1;
                const auto droppedFilename = static_cast<char*>(malloc(filenameLength));
                // Allocate memory for the filename
                DragQueryFile((HDROP)wParam, 0, droppedFilename, filenameLength);
                // Get filename of first file, discard the rest
                DragFinish((HDROP)wParam); // Tell Windows I've finished
                load_file(droppedFilename);
                free(droppedFilename); // deallocate buffer
            }
            return TRUE;
        case WM_COMMAND:
            switch (LOWORD(wParam))
            {
            case btnUpdateHeader:
                update_header();
                Utils::compress(_currentFilename);
                load_file(_currentFilename);
                break;
            case btnCheckLengths:
                check_lengths(_currentFilename, TRUE, *this);
                load_file(_currentFilename);
                break;
            case btnTrim:
                {
                    int loop = -1;
                    BOOL b1, b2, b3 = TRUE;
                    const int start = static_cast<int>(GetDlgItemInt(_trimWnd, edtTrimStart, &b1, FALSE));
                    const int end = static_cast<int>(GetDlgItemInt(_trimWnd, edtTrimEnd, &b2, FALSE));
                    if (IsDlgButtonChecked(_trimWnd, cbLoop))
                    {
                        // want looping
                        loop = static_cast<int>(GetDlgItemInt(_trimWnd, edtTrimLoop, &b3, FALSE));
                    }

                    if (!b1 || !b2 || !b3)
                    {
                        // failed to get values
                        show_error("Invalid edit points!");
                        break;
                    }
                    trim(_currentFilename, start, loop, end, false, IsDlgButtonChecked(_trimWnd, cbLogTrims), *this);
                }
                break;
            case btnWriteToText:
                write_to_text(_currentFilename, *this);
                break;
            case btnOptimise:
                optimize(_currentFilename);
                Utils::compress(_currentFilename);
                load_file(_currentFilename);
                break;
            case btnDecompress:
                Utils::decompress(_currentFilename);
                load_file(_currentFilename);
                break;
            case btnRoundTimes:
                {
                    BOOL b;
                    const int Edits[3] = {edtTrimStart, edtTrimLoop, edtTrimEnd};
                    if (!_currentFileVgmHeader.RecordingRate)
                    {
                        break; // stop if rate = 0
                    }
                    int FrameLength = 44100 / _currentFileVgmHeader.RecordingRate;
                    for (int i = 0; i < 3; ++i)
                    {
                        int Time = GetDlgItemInt(_trimWnd, Edits[i], &b, FALSE);
                        if (b)
                        {
                            const double frames = static_cast<double>(Time) / FrameLength;
                            const int roundedFrames = std::lround(frames) * FrameLength;
                            SetDlgItemInt(_trimWnd, Edits[i], roundedFrames, FALSE);
                        }
                    }
                }
                break;
            case btnPlayFile:
                ShellExecute(_hWndMain, "Play", _currentFilename.c_str(), nullptr, nullptr, SW_NORMAL);
                break;
            case btnUpdateGD3:
                update_gd3();
                Utils::compress(_currentFilename);
                load_file(_currentFilename);
                break;
            case btnGD3Clear:
                clear_gd3_strings();
                break;
            case btnSelectAll:
                change_check_boxes(1);
                break;
            case btnSelectNone:
                change_check_boxes(2);
                break;
            case btnSelectInvert:
                change_check_boxes(3);
                break;
            case btnSelectGuess:
                change_check_boxes(4);
                break;
            case btnStrip:
                strip_checked(_currentFilename);
                break;
            case cbPSGTone:
                {
                    const int Checked = IsDlgButtonChecked(_stripWnd, cbPSGTone);
                    for (int i = 0; i < 3; ++i)
                    {
                        if (IsWindowEnabled(GetDlgItem(_stripWnd, _psgCheckBoxes[i])))
                        {
                            CheckDlgButton(
                                _stripWnd, _psgCheckBoxes[i], Checked);
                        }
                    }
                }
                break;
            case cbYM2413Tone:
                {
                    const int Checked = IsDlgButtonChecked(_stripWnd, cbYM2413Tone);
                    for (int i = 0; i < 9; ++i)
                    {
                        if (IsWindowEnabled(GetDlgItem(_stripWnd, _ym2413CheckBoxes[i])))
                        {
                            CheckDlgButton(_stripWnd, _ym2413CheckBoxes[i], Checked);
                        }
                    }
                }
                break;
            case cbYM2413Percussion:
                {
                    const int Checked = IsDlgButtonChecked(_stripWnd, cbYM2413Percussion);
                    for (int i = 9; i < 14; ++i)
                    {
                        if (IsWindowEnabled(GetDlgItem(_stripWnd, _ym2413CheckBoxes[i])))
                        {
                            CheckDlgButton(_stripWnd, _ym2413CheckBoxes[i], Checked);
                        }
                    }
                }
                break;
            case btnRateDetect:
                {
                    int i = detect_rate(_currentFilename, *this);
                    if (i)
                    {
                        SetDlgItemInt(_headerWnd, edtPlaybackRate, i, FALSE);
                    }
                    // TODO: make sure VGM version is set high enough when updating header
                }
                break;
            case cbGD3SystemEn:
                if (HIWORD(wParam) != CBN_SELENDOK)
                {
                    break;
                }
                switch (SendDlgItemMessage(_gd3Wnd, cbGD3SystemEn, CB_GETCURSEL, 0, 0))
                {
                case 0: SetDlgItemText(_gd3Wnd, edtGD3SystemJp,
                        "&#x30bb;&#x30ac;&#x30de;&#x30b9;&#x30bf;&#x30fc;&#x30b7;&#x30b9;&#x30c6;&#x30e0;");
                    break; // Sega Master System
                case 1: SetDlgItemText(_gd3Wnd, edtGD3SystemJp,
                        "&#x30bb;&#x30ac;&#x30b2;&#x30fc;&#x30e0;&#x30ae;&#x30a2;");
                    break; // Sega Game Gear
                case 2: SetDlgItemText(_gd3Wnd, edtGD3SystemJp,
                        "&#x30bb;&#x30ac;&#x30de;&#x30b9;&#x30bf;&#x30fc;&#x30b7;&#x30b9;&#x30c6;&#x30e0; / &#x30b2;&#x30fc;&#x30e0;&#x30ae;&#x30a2;");
                    break; // Sega Master System / Game Gear
                case 3: SetDlgItemText(_gd3Wnd, edtGD3SystemJp,
                        "&#x30bb;&#x30ac;&#x30e1;&#x30ac;&#x30c9;&#x30e9;&#x30a4;&#x30d6;");
                    break; // Sega Megadrive (Japanese name)
                default: SetDlgItemText(_gd3Wnd, edtGD3SystemJp, "");
                    break;
                }
                break;
            case btnCopyLengths:
                copy_lengths_to_clipboard();
                break;
            case btnRemoveGD3:
                remove_gd3(_currentFilename, *this);
                load_file(_currentFilename);
                break;
            case btnRemoveOffsets:
                remove_offset(_currentFilename, *this);
                load_file(_currentFilename);
                break;
            case btnOptimiseVGMData:
                //      OptimiseVGMData(_current_filename);
                show_error("TODO: OptimiseVGMData() fixing");
                load_file(_currentFilename);
                break;
            case btnOptimisePauses:
                optimise_vgm_pauses(_currentFilename, *this);
                load_file(_currentFilename);
                break;
            case btnTrimOnly:
                {
                    int Loop = -1;
                    BOOL b1, b2, b3 = TRUE;
                    int Start = GetDlgItemInt(_trimWnd, edtTrimStart, &b1, FALSE);
                    int End = GetDlgItemInt(_trimWnd, edtTrimEnd, &b2, FALSE);
                    if (IsDlgButtonChecked(_trimWnd, cbLoop))
                    {
                        // want looping
                        Loop = GetDlgItemInt(_trimWnd, edtTrimLoop, &b3, FALSE);
                    }

                    if (!b1 || !b2 || !b3)
                    {
                        // failed to get values
                        show_error("Invalid edit points!");
                        break;
                    }

                    if (IsDlgButtonChecked(_trimWnd, cbLogTrims))
                    {
                        log_trim(_currentFilename, Start, Loop, End, *this);
                    }

                    new_trim(_currentFilename, Start, Loop, End, *this);
                }
                break;
            case btnCompress:
                Utils::compress(_currentFilename);
                load_file(_currentFilename);
                break;
            case btnNewTrim:
                {
                    int Loop = -1;
                    BOOL b1, b2, b3 = TRUE;
                    int Start = GetDlgItemInt(_trimWnd, edtTrimStart, &b1, FALSE);
                    int End = GetDlgItemInt(_trimWnd, edtTrimEnd, &b2, FALSE);
                    if (IsDlgButtonChecked(_trimWnd, cbLoop))
                    {
                        // want looping
                        Loop = GetDlgItemInt(_trimWnd, edtTrimLoop, &b3, FALSE);
                    }

                    if (!b1 || !b2 || !b3)
                    {
                        // failed to get values
                        show_error("Invalid edit points!");
                        break;
                    }
                    new_trim(_currentFilename, Start, Loop, End, *this);
                    remove_offset(_currentFilename, *this);
                    //        OptimiseVGMData(_current_filename);
                    optimise_vgm_pauses(_currentFilename, *this);
                }
                break;
            case btnGetCounts:
                check_write_counts(_currentFilename);
                break;
            case btnRoundToFrames:
                round_to_frame_accurate(_currentFilename, *this);
                break;
            } // end switch(LOWORD(wParam))
            {
                // Stuff to happen after every message/button press (?!): update "all" checkboxes for stripping
                int i;
                int Checked = 0;
                int Total = 0;
                for (i = 0; i < 3; ++i)
                {
                    Checked += IsDlgButtonChecked(_stripWnd, _psgCheckBoxes[i]);
                    Total += IsWindowEnabled(GetDlgItem(_stripWnd, _psgCheckBoxes[i]));
                }
                CheckDlgButton(_stripWnd, cbPSGTone, (Checked == Total) && Total);
                Checked = 0;
                Total = 0;
                for (i = 0; i < 9; ++i)
                {
                    Checked += IsDlgButtonChecked(_stripWnd, _ym2413CheckBoxes[i]);
                    Total += IsWindowEnabled(GetDlgItem(_stripWnd, _ym2413CheckBoxes[i]));
                }
                CheckDlgButton(_stripWnd, cbYM2413Tone, (Checked == Total) && Total);
                Checked = 0;
                Total = 0;
                for (i = 9; i < 14; ++i)
                {
                    Checked += IsDlgButtonChecked(_stripWnd, _ym2413CheckBoxes[i]);
                    Total += IsWindowEnabled(GetDlgItem(_stripWnd, _ym2413CheckBoxes[i]));
                }
                CheckDlgButton(_stripWnd, cbYM2413Percussion, (Checked == Total) && Total);
            }
            return TRUE; // WM_COMMAND handled
        case WM_NOTIFY:
            switch (LOWORD(wParam))
            {
            case tcMain:
                switch (((LPNMHDR)lParam)->code)
                {
                case TCN_SELCHANGING: // hide current window
                    SetWindowPos(_tabChildWindows[TabCtrl_GetCurSel(GetDlgItem(_hWndMain, tcMain))], HWND_TOP, 0, 0, 0,
                        0,
                        SWP_NOMOVE | SWP_NOSIZE | SWP_HIDEWINDOW);
                    break;
                case TCN_SELCHANGE: // show current window
                    {
                        int i = TabCtrl_GetCurSel(GetDlgItem(_hWndMain, tcMain));
                        SetWindowPos(_tabChildWindows[i], HWND_TOP, 0, 0, 0, 0,
                            SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
                        SetFocus(_tabChildWindows[i]);
                    }
                    break;
                }
                break;
            } // end switch (LOWORD(wParam))
            return TRUE; // WM_NOTIFY handled
        }
    }
    catch (const std::exception& e)
    {
        show_error(e.what());
    }
    return FALSE; // return FALSE to signify message not processed
}

void Gui::make_tabbed_dialog()
{
    // Load images for tabs
    InitCommonControls(); // required before doing imagelist stuff
    HIMAGELIST imagelist = ImageList_LoadImage(_hInstance, reinterpret_cast<LPCSTR>(tabimages), 16, 0, RGB(255, 0, 255),
        IMAGE_BITMAP, LR_CREATEDIBSECTION);

    HWND tabCtrlWnd = GetDlgItem(_hWndMain, tcMain);
    TabCtrl_SetImageList(tabCtrlWnd, imagelist);

    // Add tabs
    TC_ITEM newTab;
    newTab.mask = TCIF_TEXT | TCIF_IMAGE;
    newTab.pszText = const_cast<char*>("Header");
    newTab.iImage = 0;
    TabCtrl_InsertItem(tabCtrlWnd, 0, &newTab);
    newTab.pszText = const_cast<char*>("Trim/optimise");
    newTab.iImage = 1;
    TabCtrl_InsertItem(tabCtrlWnd, 1, &newTab);
    newTab.pszText = const_cast<char*>("Data usage/strip");
    newTab.iImage = 2;
    TabCtrl_InsertItem(tabCtrlWnd, 2, &newTab);
    newTab.pszText = const_cast<char*>("GD3 tag");
    newTab.iImage = 3;
    TabCtrl_InsertItem(tabCtrlWnd, 3, &newTab);
    newTab.pszText = const_cast<char*>("Conversion");
    newTab.iImage = 4;
    TabCtrl_InsertItem(tabCtrlWnd, 4, &newTab);
    newTab.pszText = const_cast<char*>("More functions");
    newTab.iImage = 5;
    TabCtrl_InsertItem(tabCtrlWnd, 5, &newTab);

    // We need to locate all the child tabs aligned to the tab control.
    // So first we get the area of the tab control...
    RECT tabDisplayRect;
    GetWindowRect(tabCtrlWnd, &tabDisplayRect);
    // And adjust it to the "display area"
    TabCtrl_AdjustRect(tabCtrlWnd, FALSE, &tabDisplayRect);
    // Then make it relative to the main window
    MapWindowPoints(HWND_DESKTOP, _hWndMain, reinterpret_cast<LPPOINT>(&tabDisplayRect), 2);

    // Create child windows
    for (const auto id : {DlgVGMHeader, DlgTrimming, DlgStripping, DlgGD3, DlgConvert, DlgMisc})
    {
        _tabChildWindows.push_back(CreateDialog(_hInstance, MAKEINTRESOURCE(id), _hWndMain, static_dialog_proc));
    }
    _headerWnd = _tabChildWindows[0];
    _trimWnd = _tabChildWindows[1];
    _stripWnd = _tabChildWindows[2];
    _gd3Wnd = _tabChildWindows[3];
    _convertWnd = _tabChildWindows[4];

    // Put them in the right place, and hide them
    for (const auto& tabChildWnd : _tabChildWindows)
    {
        EnableThemeDialogTexture(tabChildWnd, ETDT_ENABLETAB);

        SetWindowPos(tabChildWnd, HWND_TOP, tabDisplayRect.left, tabDisplayRect.top,
            tabDisplayRect.right - tabDisplayRect.left, tabDisplayRect.bottom - tabDisplayRect.top,
            SWP_HIDEWINDOW);
    }
    // Show the first one, though
    SetWindowPos(_tabChildWindows[0], HWND_TOP, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_SHOWWINDOW);
}

void Gui::load_file(const std::string& filename)
{
    char buffer[64];
    TGD3Header GD3Header;
    int FileHasGD3 = 0;

    if (!Utils::file_exists(filename))
    {
        return;
    }

    show_status("Loading file...");

    gzFile in = gzopen(filename.c_str(), "rb");
    gzread(in, &_currentFileVgmHeader, sizeof(_currentFileVgmHeader));

    if (!_currentFileVgmHeader.is_valid())
    {
        // no VGM marker
        show_error(
            "File is not a VGM file!\nIt will not be opened.\n\nMaybe you want to convert GYM, CYM and SSL files to VGM?\nClick on the \"Conversion\" tab.");
        _currentFilename.clear();
        SetDlgItemText(_hWndMain, edtFileName, "Drop a file onto the window to load");
        show_status("");
        return;
    }

    _currentFilename = filename; // Remember it
    SetDlgItemText(_hWndMain, edtFileName, filename.c_str()); // Put it in the box

    if (_currentFileVgmHeader.GD3Offset != 0)
    {
        // GD3 tag exists
        gzseek(in, _currentFileVgmHeader.GD3Offset + GD3DELTA, SEEK_SET);
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
    SetDlgItemInt(_headerWnd, edtPlaybackRate, _currentFileVgmHeader.RecordingRate, FALSE);

    // Lengths
    int Mins = static_cast<int>(_currentFileVgmHeader.TotalLength) / 44100 / 60;
    int Secs = static_cast<int>(_currentFileVgmHeader.TotalLength) / 44100 - Mins * 60;
    sprintf(buffer, "%d:%02d", Mins, Secs);
    SetDlgItemText(_headerWnd, edtLengthTotal, buffer);
    if (_currentFileVgmHeader.LoopLength > 0)
    {
        Mins = static_cast<int>(_currentFileVgmHeader.LoopLength) / 44100 / 60;
        Secs = static_cast<int>(_currentFileVgmHeader.LoopLength) / 44100 - Mins * 60;
        sprintf(buffer, "%d:%02d", Mins, Secs);
    }
    else
    {
        strcpy(buffer, "-");
    }
    SetDlgItemText(_headerWnd, edtLengthLoop, buffer);

    // Version
    sprintf(buffer, "%x.%02x", _currentFileVgmHeader.Version >> 8, _currentFileVgmHeader.Version & 0xff);
    SetDlgItemText(_headerWnd, edtVersion, buffer);

    // Clock speeds
    SetDlgItemInt(_headerWnd, edtPSGClock, _currentFileVgmHeader.PSGClock, FALSE);
    SetDlgItemInt(_headerWnd, edtYM2413Clock, _currentFileVgmHeader.YM2413Clock, FALSE);
    SetDlgItemInt(_headerWnd, edtYM2612Clock, _currentFileVgmHeader.YM2612Clock, FALSE);
    SetDlgItemInt(_headerWnd, edtYM2151Clock, _currentFileVgmHeader.YM2151Clock, FALSE);

    // PSG settings
    sprintf(buffer, "0x%04x", _currentFileVgmHeader.PSGWhiteNoiseFeedback);
    SetDlgItemText(_headerWnd, edtPSGFeedback, buffer);
    SetDlgItemInt(_headerWnd, edtPSGSRWidth, _currentFileVgmHeader.PSGShiftRegisterWidth, FALSE);

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

            if (!SetDlgItemTextW(_gd3Wnd, _gd3EditControls[i], ModifiedString))
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
                SetDlgItemText(_gd3Wnd, _gd3EditControls[i], MBCSstring); // and put it in the edit control
            }

            // Move the pointer to the next string
            GD3string += wcslen(GD3string) + 1;
        }
    }

    check_write_counts(""); // reset counts

    if ((!FileHasGD3) && GD3Strings)
    {
        show_status("File loaded - file has no GD3 tag, previous tag kept");
    }
    else
    {
        show_status("File loaded");
    }
}

void Gui::do_ctrl_tab() const
{
    const HWND tabCtrlWnd = GetDlgItem(_hWndMain, tcMain);
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
        currentTab = static_cast<int>(_tabChildWindows.size() - 1);
    }
    else if (currentTab == static_cast<int>(_tabChildWindows.size()))
    {
        currentTab = 0;
    }

    TabCtrl_SetCurFocus(tabCtrlWnd, currentTab);
}

void Gui::fill_combo_box(HWND parent, int id, const std::vector<std::string>& items)
{
    for (const auto& item : items)
    {
        SendDlgItemMessage(parent, id, CB_ADDSTRING, 0, (LPARAM)item.c_str());
    }
}

void Gui::convert_dropped_files(HDROP hDrop) const
{
    int numConverted = 0;
    const auto startTime = GetTickCount();

    // Get number of files dropped
    const int numFiles = DragQueryFile(hDrop, 0xFFFFFFFF, nullptr, 0);

    // Go through files
    for (int i = 0; i < numFiles; ++i)
    {
        // Get filename length
        const int filenameLength = DragQueryFile(hDrop, i, nullptr, 0);
        // Make a string to hold it
        std::string droppedFilename(filenameLength, '\0');
        // Get it into the string
        DragQueryFile(hDrop, i, droppedFilename.data(), filenameLength);
        //  Convert it
        if (Convert::to_vgm(droppedFilename, *this))
        {
            ++numConverted;
        }
    }

    DragFinish(hDrop);

    show_conversion_progress(Utils::format("%d of %d file(s) successfully converted in %dms", numConverted, numFiles,
        GetTickCount() - startTime));
}

void Gui::update_header() const
{
    VGMHeader VGMHeader;

    if (!Utils::file_exists(_currentFilename))
    {
        return;
    }

    gzFile in = gzopen(_currentFilename.c_str(), "rb");
    gzread(in, &VGMHeader, sizeof(VGMHeader));
    gzclose(in);

    if (int rate; get_int(_headerWnd, edtPlaybackRate, &rate))
    {
        VGMHeader.RecordingRate = rate;
    }

    auto s = get_utf8_string(_headerWnd, edtVersion);
    if (std::smatch m; std::regex_match(s, m, std::regex(R"((\d{1,2})\.(\d{1,2}))")))
    {
        const auto major = std::stoi(m[0]);
        const auto minor = std::stoi(m[1]) * (m[1].length() == 1 ? 10 : 1); // Make e.g. "1" become "10"
        // valid data
        VGMHeader.Version =
            (major / 10) << 12 | // major tens
            (major % 10) << 8 | // major units
            (minor / 10) << 4 | // minor tens
            (minor % 10); // minor units
    }
    else
    {
        throw std::runtime_error(Utils::format("Invalid version \"%s\"", s.c_str()));
    }

    if (int i; get_int(_headerWnd, edtPSGClock, &i))
    {
        VGMHeader.PSGClock = i;
    }
    if (int i; get_int(_headerWnd, edtYM2413Clock, &i))
    {
        VGMHeader.YM2413Clock = i;
    }
    if (int i; get_int(_headerWnd, edtYM2612Clock, &i))
    {
        VGMHeader.YM2612Clock = i;
    }
    if (int i; get_int(_headerWnd, edtYM2151Clock, &i))
    {
        VGMHeader.YM2151Clock = i;
    }

    s = get_utf8_string(_headerWnd, edtPSGFeedback);
    if (std::smatch m; std::regex_search(s, m, std::regex(R"(^0x([0-9a-fA-F]+))")))
    {
        // valid data
        VGMHeader.PSGWhiteNoiseFeedback = static_cast<uint16_t>(std::stoi(m[0], nullptr, 16));
    }
    if (int i; get_int(_headerWnd, edtPSGSRWidth, &i))
    {
        VGMHeader.PSGShiftRegisterWidth = static_cast<uint8_t>(i);
    }

    write_vgm_header(_currentFilename, VGMHeader, *this);
}

void Gui::optimize(const std::string& filename) const
{
    VGMHeader VGMHeader;
    int NumOffsetsRemoved = 0;

    gzFile in = gzopen(filename.c_str(), "rb");
    gzread(in, &VGMHeader, sizeof(VGMHeader));
    gzclose(in);
    long FileSizeBefore = VGMHeader.EoFOffset + EOFDELTA;

    // Make sure lengths are correct
    check_lengths(filename, FALSE, *this);

    // Remove PSG offsets if selected
    if ((VGMHeader.PSGClock) && IsDlgButtonChecked(_trimWnd, cbRemoveOffset))
    {
        NumOffsetsRemoved = remove_offset(filename, *this);
    }

    // Trim (using the existing edit points), also merges pauses
    if (VGMHeader.LoopLength)
    {
        trim(filename, 0, static_cast<int>(VGMHeader.TotalLength - VGMHeader.LoopLength),
            static_cast<int>(VGMHeader.TotalLength), true, false, *this);
    }
    else
    {
        trim(filename, 0, -1, static_cast<int>(VGMHeader.TotalLength), true, false, *this);
    }

    in = gzopen(filename.c_str(), "rb");
    gzread(in, &VGMHeader, sizeof(VGMHeader));
    gzclose(in);
    long FileSizeAfter = VGMHeader.EoFOffset + EOFDELTA;

    if (show_question_message_box(Utils::format(
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
    )) == IDYES)
    {
        ShellExecute(_hWndMain, "Play", filename.c_str(), nullptr, nullptr, SW_NORMAL);
    }
}

void Gui::show_message(const std::string& message) const
{
    MessageBox(_hWndMain, message.c_str(), _programName.c_str(), 0);
}

void Gui::show_status(const std::string& message) const
{
    SetDlgItemText(_hWndMain, txtStatusBar, message.c_str());
}

void Gui::show_conversion_progress(const std::string& message) const
{
    const auto withBreak = message + "\r\n";
    // Get length
    const auto length = SendDlgItemMessage(_convertWnd, edtConvertResults, WM_GETTEXTLENGTH, 0, 0);
    // move caret to end of text
    SendDlgItemMessage(_convertWnd, edtConvertResults, EM_SETSEL, length, length);
    // insert text there
    SendDlgItemMessage(_convertWnd, edtConvertResults, EM_REPLACESEL, FALSE,
        reinterpret_cast<LPARAM>(withBreak.c_str()));
}

void Gui::show_error(const std::string& message) const
{
    MessageBox(_hWndMain, message.c_str(), _programName.c_str(), MB_ICONERROR + MB_OK);
}

void Gui::update_gd3() const
{
    if (!Utils::file_exists(_currentFilename))
    {
        return;
    }

    show_status("Updating GD3 tag...");

    gzFile in = gzopen(_currentFilename.c_str(), "rb");
    VGMHeader VGMHeader;
    if (!ReadVGMHeader(in, &VGMHeader, *this))
    {
        gzclose(in);
        return;
    }

    gzrewind(in);

    auto outFilename = Utils::make_suffixed_filename(_currentFilename, "tagged");
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

    for (auto i = 0; i < NumGD3Strings; ++i)
    {
        // Get string from widget
        auto s = get_utf16_string(_gd3Wnd, _gd3EditControls[i]);

        // Special handling for any strings
        if (i == 10)
        {
            // Notes - change \r\n to \n
            std::erase(s, L'\r');
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

    write_vgm_header(outFilename, VGMHeader, *this); // Write changed header

    Utils::replace_file(_currentFilename, outFilename);

    show_status("GD3 tag updated");
}

void Gui::clear_gd3_strings() const
{
    for (const auto id : _gd3EditControls)
    {
        SetDlgItemText(_gd3Wnd, id, "");
    }
}

void Gui::change_check_boxes(int mode) const
// TODO make mode an enum
{
    ccb(_psgCheckBoxes, _psgWrites, mode);
    ccb(_ym2413CheckBoxes, _ym2413Writes, mode);
    ccb(_ym2612CheckBoxes, _ym2612Writes, mode);
    ccb(_ym2151CheckBoxes, _ym2151Writes, mode);
}

void Gui::ccb(const std::vector<int>& ids, const std::vector<int>& counts, int mode) const
{
    for (auto i = 0u; i < ids.size(); ++i)
    {
        if (IsWindowEnabled(GetDlgItem(_stripWnd, ids[i])))
        {
            switch (mode)
            {
            case 1: // check all
                CheckDlgButton(_stripWnd, ids[i], 1);
                break;
            case 2: // check none
                CheckDlgButton(_stripWnd, ids[i], 0);
                break;
            case 3: // invert selection
                CheckDlgButton(_stripWnd, ids[i], !IsDlgButtonChecked(_stripWnd, ids[i]));
                break;
            case 4: // guess
                {
                    int cutOff = 0;
                    for (const int count : counts)
                    {
                        if (cutOff < count)
                        {
                            cutOff = count;
                        }
                    }
                    cutOff = cutOff / 50; // 2% of largest for that chip

                    CheckDlgButton(
                        _stripWnd,
                        ids[i],
                        ((counts[i] < cutOff) ||
                            get_utf8_string(_stripWnd, ids[i]).find("Invalid") != std::string::npos)
                            ? 1
                            : 0);
                    break;
                }
            default:
                throw std::runtime_error(Utils::format("Invalid checkbox mode %d", mode));
            }
        }
    }
}

void Gui::strip_checked(const std::string& filename) const
{
    VGMHeader VGMHeader;

    if (!Utils::file_exists(filename))
    {
        return;
    }

    show_status("Stripping chip data...");

    gzFile in = gzopen(filename.c_str(), "rb");
    if (!ReadVGMHeader(in, &VGMHeader, *this))
    {
        gzclose(in);
        return;
    }
    gzclose(in);

    // Set up filenames
    auto Outfilename = Utils::make_suffixed_filename(filename, "stripped");

    // Strip data
    strip(filename, Outfilename);
    /*
      // Optimise output file
      if (VGMHeader.LoopLength) Trim(Tmpfilename, 0, VGMHeader.TotalLength-VGMHeader.LoopLength, VGMHeader.TotalLength, TRUE, FALSE);
      else Trim(Tmpfilename, 0, -1, VGMHeader.TotalLength, TRUE, FALSE);
    
      // Strip data again because optimiser added it back
      Strip(Tmpfilename, Outfilename);
      
      // Clean up
      DeleteFile(Tmpfilename);
    */

    show_status("Data stripping complete");

    if (show_question_message_box(Utils::format(
        "Stripped VGM data written to\n%s\nDo you want to open it in the associated program?",
        Outfilename.c_str())) == IDYES)
    {
        ShellExecute(_hWndMain, "open", Outfilename.c_str(), nullptr, nullptr, SW_NORMAL);
    }
}

// Remove data for checked boxes
void Gui::strip(const std::string& filename, const std::string& outfilename) const
{
    VGMHeader VGMHeader;
    signed int b0, b1, b2;
    char LatchedChannel = 0;
    long int NewLoopOffset = 0;

    // Set up masks
    unsigned char PSGMask = (1 << NumPSGTypes) - 1;
    for (auto i = 0; i < NumPSGTypes; i++)
    {
        if ((IsDlgButtonChecked(_stripWnd, _psgCheckBoxes[i])) || (!IsWindowEnabled(
            GetDlgItem(_stripWnd, _psgCheckBoxes[i]))))
        {
            PSGMask ^= (1 << i);
        }
    }
    unsigned long YM2413Mask = (1 << NumYM2413Types) - 1;
    for (auto i = 0; i < NumYM2413Types; i++)
    {
        if ((IsDlgButtonChecked(_stripWnd, _ym2413CheckBoxes[i])) || (!IsWindowEnabled(
            GetDlgItem(_stripWnd, _ym2413CheckBoxes[i]))))
        {
            YM2413Mask ^= (1 << i);
        }
    }
    unsigned char YM2612Mask = (1 << NumYM2612Types) - 1;
    for (auto i = 0; i < NumYM2612Types; i++)
    {
        if ((IsDlgButtonChecked(_stripWnd, _ym2612CheckBoxes[i])) || (!IsWindowEnabled(
            GetDlgItem(_stripWnd, _ym2612CheckBoxes[i]))))
        {
            YM2612Mask ^= (1 << i);
        }
    }
    unsigned char YM2151Mask = (1 << NumYM2151Types) - 1;
    for (auto i = 0; i < NumYM2151Types; i++)
    {
        if ((IsDlgButtonChecked(_stripWnd, _ym2151CheckBoxes[i])) || (!IsWindowEnabled(
            GetDlgItem(_stripWnd, _ym2151CheckBoxes[i]))))
        {
            YM2151Mask ^= (1 << i);
        }
    }

    gzFile in = gzopen(filename.c_str(), "rb");

    // Read header
    if (!ReadVGMHeader(in, &VGMHeader, *this))
    {
        gzclose(in);
        return;
    }
    gzseek(in, VGM_DATA_OFFSET, SEEK_SET);

    gzFile out = gzopen(outfilename.c_str(), "wb0"); // No compression, since I'll recompress it later

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
    write_vgm_header(outfilename, VGMHeader, *this);
}

void Gui::copy_lengths_to_clipboard() const
{
    // We work in Unicode for better compatibility...
    auto result = get_utf16_string(_gd3Wnd, edtGD3TitleEn);
    if (result.length() > 35)
    {
        result.erase(35);
    }
    else if (result.length() < 35)
    {
        result.append(35 - result.length(), L' ');
    }
    result += get_utf16_string(_headerWnd, edtLengthTotal);
    result += L' ';
    result += get_utf16_string(_headerWnd, edtLengthLoop);

    if (OpenClipboard(_hWndMain) == FALSE)
    {
        return;
    }
    EmptyClipboard();
    const auto length = (result.length() + 1) * 2;
    HANDLE copyForClipboard = GlobalAlloc(GMEM_DDESHARE, length);
    if (copyForClipboard == nullptr)
    {
        CloseClipboard();
        return;
    }
    const auto p = GlobalLock(copyForClipboard);
    memcpy(p, result.c_str(), length);
    GlobalUnlock(copyForClipboard);
    SetClipboardData(CF_UNICODETEXT, copyForClipboard);
    CloseClipboard();

    // We need to convert back to the active code page (likely UTF-8?) to show it though
    const int sizeNeeded = WideCharToMultiByte(CP_ACP, 0, result.data(), static_cast<int>(result.size()), nullptr, 0,
        nullptr, nullptr);
    std::string strTo(sizeNeeded, '\0');
    WideCharToMultiByte(CP_ACP, 0, result.data(), static_cast<int>(result.size()), strTo.data(), sizeNeeded, nullptr,
        nullptr);

    show_status(Utils::format("Copied: \"%s\"", strTo.c_str()));
}

// Check checkboxes and show numbers for how many times each channel/data type is used
void Gui::check_write_counts(const std::string& filename)
{
    int i, j;
    GetWriteCounts(filename, _psgWrites, _ym2413Writes, _ym2612Writes, _ym2151Writes, _reservedWrites, *this);

    update_write_count(_psgCheckBoxes, _psgWrites);
    update_write_count(_ym2413CheckBoxes, _ym2413Writes);
    update_write_count(_ym2612CheckBoxes, _ym2612Writes);
    update_write_count(_ym2151CheckBoxes, _ym2151Writes);
    update_write_count(_reservedCheckboxes, _reservedWrites);

    // Sum stuff for group checkboxes and other stuff:
    // PSG tone channels
    for (i = j = 0; i < 3; ++i)
    {
        j += _psgWrites[i]; // count writes
    }
    if (!j)
    {
        CheckDlgButton(_stripWnd, cbPSGTone, 0); // if >0, initialise unchecked
    }
    EnableWindow(GetDlgItem(_stripWnd, cbPSGTone), (j > 0)); // enabled = (>0)
    // YM2413 tone channels
    for (i = j = 0; i < 9; ++i)
    {
        j += _ym2413Writes[i];
    }
    if (!j)
    {
        CheckDlgButton(_stripWnd, cbYM2413Tone, 0);
    }
    EnableWindow(GetDlgItem(_stripWnd, cbYM2413Tone), (j > 0));
    // YM2413 percussion
    for (i = 9, j = 0; i < 14; ++i)
    {
        j += _ym2413Writes[i];
    }
    if (!j)
    {
        CheckDlgButton(_stripWnd, cbYM2413Percussion, 0);
    }
    EnableWindow(GetDlgItem(_stripWnd, cbYM2413Percussion), (j > 0));
    // PSG anything
    for (i = j = 0; i < NumPSGTypes; ++i)
    {
        j += _psgWrites[i];
    }
    EnableWindow(GetDlgItem(_stripWnd, gbPSG), (j != 0));
    // YM2413 anything
    for (i = j = 0; i < NumYM2413Types; ++i)
    {
        j += _ym2413Writes[i];
    }
    EnableWindow(GetDlgItem(_stripWnd, gbYM2413), (j != 0));

    show_status("Scan for chip data complete");
}

void Gui::update_write_count(const std::vector<int>& ids, const std::vector<int>& counts) const
{
    for (int i = 0u; i < ids.size(); ++i)
    {
        auto s = get_utf8_string(_stripWnd, ids[i]);
        // Remove existing count
        if (const auto index = s.find(" ("); index != std::string::npos)
        {
            s = s.substr(0, index);
        }
        // And add the new one
        if (counts[i] > 0)
        {
            s = Utils::format("%s (%d)", s.c_str(), counts[i]);
        }
        SetDlgItemText(_stripWnd, ids[i], s.c_str());
        EnableWindow(GetDlgItem(_stripWnd, ids[i]), counts[i] > 0);
        if (counts[i] == 0)
        {
            CheckDlgButton(_stripWnd, ids[i], 0);
        }
    }
}
