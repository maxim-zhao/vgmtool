# AGENTS.md - VGMTool Project Guide for LLMs

## Project Overview

**VGMTool** is a C++ Windows utility for manipulating VGM (Video Game Music) files. VGM is a format for recording sound chip commands from classic gaming consoles and arcade systems. The tool provides GUI and CLI interfaces for trimming, optimizing, converting, and analyzing VGM files.

**Repository**: https://github.com/maxim-zhao/vgmtool

## Architecture

### Core Structure

```
vgmtool/
├── libvgmtool/          # Core library - VGM processing logic
│   ├── trim.cpp/h       # Trim VGM files to specific sample ranges
│   ├── convert.cpp/h    # Convert between audio formats
│   ├── optimise.cpp/h   # Optimize VGM data and file size
│   ├── gd3.cpp/h        # GD3 tag handling (VGM metadata)
│   ├── VgmFile.cpp/h    # Main VGM file class
│   ├── vgm.h            # VGM format constants and structures
│   ├── utils.cpp/h      # Utility functions
│   └── IVGMToolCallback.h # Callback interface for progress/messaging
├── vgmtool/             # GUI Application (MFC-based)
│   ├── gui.cpp/h        # Main GUI logic and dialog handling
│   ├── resources.rc     # Resource definitions
│   └── vgmtool.cpp      # Application entry point
├── vgmtool-cli/         # Command-line interface
│   └── main.cpp         # CLI entry point
├── libpu8/              # UTF-8 utility library
├── zopfli/              # Compression library
└── build files...
```

## Key Modules

### libvgmtool (Core Logic)

#### VgmFile Class (VgmFile.cpp/h)
- **Purpose**: Main class representing a loaded VGM file
- **Key responsibilities**:
  - Load and parse VGM file headers
  - Store and manipulate VGM data
  - Write modified VGM files
  - Perform file integrity checks
- **Important methods**:
  - `load_file()` - Parse VGM file from disk
  - `save_file()` - Write VGM file to disk
  - `header()` - Access VGM header for reading/modifying metadata
  - `check_header()` - Validate and optionally fix header issues
  - `write_to_text()` - Export VGM data as human-readable text

#### trim.cpp/h - Trim Operations
- **Purpose**: Cut VGM files to specific sample ranges
- **Key functions**:
  - `trim()` - Main trim function (used by GUI via file picker)
	- Parameters: filename, start, loop, end, overWrite, logTrims, callback, outFilename
	- Generates output filename if not specified
	- Handles loop point repositioning
  - `log_trim()` - Logs trim operation details to file
  - Helper functions: `WriteVGMInfo()`, `WritePSGState()`, `WriteYM2413State()`
  - Structures: `TPSGState` - Tracks PSG chip state during processing

**Important Design Pattern**: The `outFilename` parameter:
- If empty string `""`, automatically generates filename with " (trimmed).vgm" suffix
- If provided, uses that as output destination
- **GUI Implementation**: Opens file picker dialog to get user-selected output filename

#### convert.cpp/h - Format Conversion
- **Purpose**: Convert VGM to other audio formats (WAV, etc.)
- **Key function**: `Convert::to_vgm()` - Convert various formats to VGM

#### optimise.cpp/h - Optimization
- **Purpose**: Reduce VGM file size and improve efficiency
- **Key functions**:
  - `optimise_vgm_pauses()` - Merge consecutive pause commands
  - `remove_offset()` - Remove PSG offset commands
  - `check_lengths()` - Validate VGM length values

#### gd3.cpp/h - Metadata Handling
- **Purpose**: Manage GD3 tags (VGM metadata: title, game, author, date, etc.)
- **Key classes**: `Gd3Tag`, `IVGMToolCallback`
- **Operations**: Read, write, update, remove GD3 tags

#### IVGMToolCallback.h - Callback Interface
- **Purpose**: Allow library to communicate with GUI/CLI without coupling
- **Virtual methods**:
  - `show_message()` - Display informational message
  - `show_error()` - Display error message
  - `show_status()` - Show operation progress/status
  - `show_conversion_progress()` - Show conversion details
- **Implementation**: Both GUI and CLI classes implement this interface

### vgmtool (GUI Application)

#### gui.cpp/h - Main GUI Logic
- **Framework**: MFC (Microsoft Foundation Classes) for Windows dialogs
- **Architecture**: Tab-based dialog with multiple dialogs for different operations
- **Key components**:
  - Main dialog (`MAINDIALOGUE` resource ID)
  - Child dialogs for different operations:
	- `DlgVGMHeader` - Edit VGM header/metadata
	- `DlgTrimming` - Trim operation UI
	- `DlgStripping` - Strip chip data UI
	- `DlgGD3` - Edit GD3 tags
	- `DlgConvert` - Format conversion UI
	- `DlgMisc` - Miscellaneous operations

**Important Methods**:
- `load_file()` - Load VGM and populate all UI fields
- `show_save_file_dialog()` - Open Windows file picker for saving
- `dialog_proc()` - Main message handler with button/command switch
- Helper methods: `get_int()`, `get_bool()`, `get_utf8_string()` - Read UI controls

**Button Handlers** (in dialog_proc switch statement):
- `btnTrim` - Trim with file picker dialog (calls `show_save_file_dialog()`)
- `btnOptimise` - Run optimization pipeline
- `btnStrip` - Remove selected chip data
- `btnUpdateGD3` - Save GD3 tag modifications
- `btnCheckLengths` - Validate VGM header lengths

**File Picker Pattern** (Added for trim operation):
```cpp
// Generate suggested filename
std::string suggestedFilename = Utils::make_suffixed_filename(filename, "trimmed");

// Show dialog
const auto outputFilename = show_save_file_dialog(suggestedFilename);
if (!outputFilename.empty()) {
	// Call operation with user-selected filename
	trim(..., outputFilename);
}
```

#### vgmtool.cpp - Application Entry Point
- Creates Gui instance
- Handles command-line file loading
- Main message loop

### Data Structures and Constants

#### vgm.h - VGM Format Definitions
- `OldVGMHeader` - VGM file header structure
- VGM command byte constants:
  - `VGM_PSG` - PSG (SN76489) command
  - `VGM_YM2413` - YM2413 command
  - `VGM_YM2612_0`, `VGM_YM2612_1` - YM2612 port 0/1 commands
  - `VGM_YM2151` - YM2151 command
  - `VGM_PAUSE_WORD`, `VGM_PAUSE_50TH`, `VGM_PAUSE_60TH` - Pause commands
  - `VGM_END` - End of VGM data marker
  - `VGM_GGST` - Game Gear stereo command
- Offset constants: `VGM_DATA_OFFSET`, `GD3DELTA`, `LOOPDELTA`, `EOFDELTA`

#### Chip Types
- **PSG** (Programmable Sound Generator / SN76489): Used by Sega systems
- **YM2413**: FM sound chip (5-channel, drum track)
- **YM2612**: FM sound chip (6-channel polyphonic)
- **YM2151**: FM sound chip (8-channel)
- **Reserved**: Manufacturer-specific extensions

## Common Workflows

### Adding a New Operation to GUI

1. **Define button resource** in `resources.rc` (resource ID: `btnMyOperation`)
2. **Add case to dialog_proc switch** in `gui.cpp`:
   ```cpp
   case btnMyOperation:
	   my_operation(_currentFilename, *this);
	   break;
   ```
3. **Implement operation** in appropriate library file
4. **Pass callback** (`*this`) for progress messages

### Modifying File Picker Behavior

The trim operation uses `show_save_file_dialog()`:
- Located in `gui.cpp`
- Returns selected filename or empty string if cancelled
- Wrapped call in `if (!outputFilename.empty())` check
- Suggested filename follows pattern: `original_filename (suffix).vgm`

### Processing VGM Data

Pattern used in trim/convert operations:
```cpp
// 1. Read header
gzFile in = gzopen(filename.c_str(), "rb");
OldVGMHeader header;
gzread(in, &header, sizeof(header));

// 2. Skip to data section
gzseek(in, VGM_DATA_OFFSET, SEEK_SET);

// 3. Process each command byte
int b0;
while ((b0 = gzgetc(in)) != VGM_END) {
	switch(b0) {
		case VGM_PSG:
			b1 = gzgetc(in);  // Read data byte
			// Process PSG command
			break;
		case VGM_YM2413:
			b1 = gzgetc(in);  // Read register
			b2 = gzgetc(in);  // Read value
			// Process YM2413 command
			break;
		// ... handle other commands
	}
}

// 4. Write modified data
gzclose(in);
gzclose(out);
write_vgm_header(outFilename, header, callback);
```

## File Manipulation Patterns

### Reading from gzip-compressed VGM
- Use `gzFile` API from zlib
- `gzopen(filename, "rb")` - Open for reading
- `gzgetc()` - Read one byte
- `gzread()` - Read buffer
- `gzseek()` - Seek in file
- `gztell()` - Get current position
- `gzclose()` - Close file

### UTF-8 Handling
- Library code uses `std::string` (UTF-8)
- GUI (MFC) uses `std::wstring` for wide strings
- Conversion functions in `gui.cpp`:
  - `MultiByteToWideChar()` - UTF-8 to UTF-16
  - `WideCharToMultiByte()` - UTF-16 to UTF-8

### Filename Operations
- Use `Utils::make_suffixed_filename()` for adding suffixes
- Example: `filename.vgm` → `filename (trimmed).vgm`
- Use `Utils::file_exists()` to check file presence
- Use `Utils::replace_file()` to atomically replace files

## Important Constants and Offsets

```cpp
#define VGM_DATA_OFFSET    0x34   // VGM command data starts here
#define GD3DELTA            12    // Offset adjustment for GD3 tag position
#define LOOPDELTA            8    // Offset adjustment for loop position
#define EOFDELTA            4     // Offset adjustment for end-of-file position
#define YM2413NumRegs      64     // Number of YM2413 registers
#define LEN60TH        735        // Sample count for 1/60 second
#define LEN50TH        882        // Sample count for 1/50 second
```

## Building the Project

- **IDE**: Visual Studio (Community 2026 or compatible)
- **Language**: C++17
- **Target**: Windows (MFC for GUI)
- **Dependencies**: zlib, zopfli (included)
- **Build command**: `run_build` tool or Visual Studio UI

## Important Notes for Code Changes

### When Modifying VGM Processing:

1. **State Management**: Some operations track chip state (PSG frequencies, YM2413 registers)
   - Initialize state before processing
   - Update state as you read commands
   - Write state at key points (loop, trim boundaries)

2. **Sample Counting**: Track sample count during processing for trim operations
   - Pause commands advance sample count
   - Data commands don't change sample count
   - Use sample count to determine when to write trim boundaries

3. **GD3 Tags**: Always copy GD3 tag to output files when present
   - Read GD3 from input after processing data
   - Write to output at correct offset
   - Update GD3Offset in header

4. **Loop Points**: Loop information is stored as:
   - `LoopOffset` - File offset where loop point is
   - `LoopLength` - Number of samples in loop
   - When trimming, recalculate both values

### When Adding GUI Features:

1. Use file picker dialog for file output operations
2. Pass callbacks (`*this`) for progress reporting
3. Call `load_file()` after modifying VGM to refresh UI
4. Use `get_int()`, `get_bool()` to safely read UI control values
5. Handle empty strings from file picker (user cancelled)

### Code Style Conventions:

- Use `std::string` for UTF-8 filenames
- Use `std::wstring` for GUI strings
- Functions using compression/VGM data use gzFile API
- Library functions take `IVGMToolCallback&` for messaging
- Error conditions are reported via callback, not exceptions (usually)

## Resources

- **VGM Format Specification**: https://www.vgmrips.net/wiki/VGM_Specification
- **GD3 Tag Format**: Part of VGM specification
- **zlib Documentation**: For gzFile operations
- **MFC Documentation**: For GUI modifications

## Recent Changes

### Trim Operation (Latest)
- GUI now opens file picker dialog when trim button is clicked
- User selects output filename instead of auto-generating
- Implementation in `gui.cpp::show_save_file_dialog()`
- Passes user-selected path to `trim()` function via `outFilename` parameter

### Removed Features
- `new_trim()` function removed (was alternative trim implementation)
- `btnTrimOnly` and `btnNewTrim` buttons removed from GUI
- Associated button handlers removed from `dialog_proc()`

## Debugging Tips

1. **File Loading Issues**: Check `VgmFile::load_file()` exception messages
2. **UI Control Values**: Use `get_int()`, `get_bool()` with try-catch
3. **File Picker Cancelled**: Check for empty string return from `show_save_file_dialog()`
4. **GD3 Tag Problems**: Verify GD3 offset and tag structure in hex editor
5. **Sample Count Mismatches**: Trace through all pause command handling

## Contact & Questions

When working on this codebase:
- Consult existing implementations as patterns
- Check git history for similar features
- Test on actual VGM files from VGMRips.net
- Verify output files play correctly in VGM players
