# Ultra-Lightweight Windows 10 Metronome (C++ / Win32 / WASAPI)

A high-precision, zero-dependency, ultra-lightweight metronome application for Windows 10/11 built entirely in modern C++ with the native Win32 API and WASAPI (Windows Audio Session API).

## Key Highlights

- **Ultra-Small Binary:** Compiled executable is ~330 KB with stripped symbols and LTO.
- **Minimal Resource Footprint:** Operates at **~0.6 MB – 2.5 MB RAM** with zero external runtime DLL dependencies (links exclusively to core Windows system DLLs: `kernel32`, `user32`, `gdi32`, `ole32`, `comdlg32`, `msvcrt`).
- **Mathematical Zero Clock-Drift:** Timing is derived directly from the physical hardware audio DAC sample clock in the WASAPI audio thread (`framesPerBeat = (sampleRate * 60) / BPM`). No timer drift or clock skew over long practice sessions.
- **Zero-Stutter Modal Loops:** Audio rendering runs independently on a dedicated `THREAD_PRIORITY_TIME_CRITICAL` background thread. Window dragging, resizing, or opening context menus never causes clicks to lag, skip, or stutter.
- **Direct Resource Loading:** Default woodblock click sound is embedded in the executable via `.rc` and loaded directly from memory via `FindResource` / `LoadResource` / `LockResource` without touching disk.
- **Modern Minimal UI:** Frameless `WS_POPUP` floating window with double-buffered GDI rendering, glowing beat pulse indicator, left-click drag, and mouse wheel BPM adjustment.

---

## Controls & Shortcuts

| Action | Control |
| :--- | :--- |
| **Play / Pause** | Left Click window, or press `Space` |
| **Adjust BPM (±1)** | Mouse Wheel Up / Down, or `Arrow Up` / `Arrow Down` / `Left` / `Right` |
| **Adjust BPM (±5)** | `Shift` + Mouse Wheel, or `Shift` + `Arrow Up` / `Arrow Down` |
| **Adjust BPM (±10)**| `Ctrl` + Mouse Wheel |
| **Move Window** | Left-Click and drag anywhere on the window |
| **Context Menu** | Right-Click anywhere on the window |
| **Tap Tempo** | Press `T` or select **Tap Tempo...** from context menu |
| **Set Custom BPM** | Select **Set Custom BPM...** from context menu |
| **Close Application** | Click the subtle `×` button in top-right, or press `Esc` |

---

## Context Menu Features

Right-click anywhere to access:
- **Play / Pause**: Toggle metronome running state.
- **Presets**: Instant selection for 60 (Largo), 90 (Andante), 120 (Allegro), 140 (Vivace), 160 (Presto), 180, and 200 BPM.
- **Set Custom BPM**: Enter any custom tempo from 20 to 300 BPM.
- **Tap Tempo**: Real-time high-precision tap tempo tool (`QueryPerformanceCounter`) with automatic tap averaging.
- **Audio Sample**:
  - *Default Click (Embedded)*: Built-in resonant woodblock tick.
  - *Browse Custom WAV...*: Load any external `.wav` file (PCM 8/16/24/32-bit or IEEE 32-bit float, resampled on-the-fly to device mix format).
- **Always on Top**: Toggle window stay-on-top behavior.
- **Exit**: Clean termination and resource disposal.

---

## Building from Source

### Prerequisites
- CMake 3.20+
- Either:
  - **MinGW-W64** (with `g++`, `windres`, `ninja` or `mingw32-make`)
  - **Microsoft Visual Studio 2019 / 2022** (MSVC toolset with C++ desktop workload)

### Build Instructions (MinGW / Ninja)
```powershell
# Configure Release build
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release

# Compile executable
cmake --build build
```

The output executable will be generated at `build/metronome.exe`.

### Build Instructions (MSVC)
```powershell
# In a Visual Studio Developer Command Prompt / PowerShell:
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```
The output executable will be generated at `build/Release/metronome.exe`.

---

## Project Structure

```
metronome-cplusplus/
├── CMakeLists.txt          # Multi-compiler build system with binary size optimizations
├── README.md               # Documentation & usage guide
├── assets/
│   └── click.wav           # Synthesized default woodblock metronome click (embedded)
└── src/
    ├── main.cpp            # WinMain, DPI awareness, COM init, message loop, memory trimming
    ├── window.h / .cpp     # Frameless WS_POPUP window, GDI double-buffer paint, dragging, menu
    ├── audio_engine.h/.cpp # WASAPI event-driven streaming, sample-exact frame counting, mixing
    ├── wav_loader.h / .cpp # RIFF/WAVE parser, in-memory resource decoding & audio resampler
    ├── custom_dialogs.h/.cpp # Native Win32 Custom BPM & Tap Tempo dialog implementations
    ├── resource.h          # Resource IDs & menu command identifiers
    └── resources.rc        # Windows resource script (embedded WAV, context menu, dialogs)
```
