#pragma once

// Access to GUI stuff for various functions
// Very OS-dependent

#include <string>
#include <vector>
#include <Windows.h>

#include "libvgmtool/IVGMToolCallback.h"
#include "libvgmtool/vgm.h"


class Gui : IVGMToolCallback
{
public:
    Gui(HINSTANCE hInstance, LPSTR lpCmdLine, int nShowCmd);
    void run();

    LRESULT dialog_proc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

private:
    static LRESULT static_dialog_proc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    void make_tabbed_dialog();
    void load_file(const std::string& filename);
    void do_ctrl_tab() const;
    static void fill_combo_box(HWND parent, int id, const std::vector<std::string>& items);
    void convert_dropped_files(HDROP hDrop) const;
    void update_header() const;
    auto optimize(const std::string& filename) const -> void;
    void update_gd3() const;
    void clear_gd3_strings() const;
    void change_check_boxes(int mode) const;
    void ccb(const std::vector<int>& ids, const std::vector<int>& counts, int mode) const;
    void strip_checked(const std::string& filename) const;
    void strip(const std::string& filename, const std::string& outFilename) const;
    void copy_lengths_to_clipboard() const;
    void check_write_counts(const std::string& filename);
    void update_write_count(const std::vector<int>& ids, const std::vector<int>& counts) const;

    static bool get_int(HWND hDlg, int item, int* result);
    static std::string get_utf8_string(HWND hDlg, int item);
    static std::wstring get_utf16_string(HWND hDlg, int item);
    [[nodiscard]] int show_question_message_box(const std::string& s) const;

public:
    void show_message(const std::string& message) const override;
    void show_error(const std::string& message) const override;
    void show_status(const std::string& message) const override;
    void show_conversion_progress(const std::string& message) const override;

private:
    HINSTANCE _hInstance;
    int _showCommand;
    std::string _commandLine;
    HWND _hWndMain{};

    // Tab windows, in a vector for ordering and as names for ease of use
    std::vector<HWND> _tabChildWindows;
    HWND _headerWnd{};
    HWND _gd3Wnd{};
    HWND _trimWnd{};
    HWND _stripWnd{};
    HWND _convertWnd{};

    // The current filename
    std::string _currentFilename;
    // The header of the current file
    OldVGMHeader _currentFileVgmHeader;

    // We maintain some vectors of control IDs to make iterating them easier.
    // They do not change.
    std::vector<int> _psgCheckBoxes;
    std::vector<int> _ym2413CheckBoxes;
    std::vector<int> _ym2612CheckBoxes;
    std::vector<int> _ym2151CheckBoxes;
    std::vector<int> _reservedCheckboxes;
    std::vector<int> _gd3EditControls;

    // These are used to hold write counts. We probably ought to remove them?
    std::vector<int> _psgWrites;
    std::vector<int> _ym2413Writes;
    std::vector<int> _ym2612Writes;
    std::vector<int> _ym2151Writes;
    std::vector<int> _reservedWrites;

    // We hold a pointer to the only Gui instance so we can dispatch to it from a static method...
    static Gui* _pThis;
    static std::string _programName;
};
