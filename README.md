# jedGB (v1.0.0 Release)

A lightweight, clean, cross-platform Game Boy (DMG-01) emulator engine and developer SDK written in modern C++20 and powered by SDL2.

**jedGB** implements the core components of the Game Boy hardware, featuring an accurate Sharp-like CPU interpreter, cycle-accurate memory-mapped I/O routing, synchronous 4-mode PPU raster processing, an accurate APU sound synth, and a C-compatible developer API for seamless integration across **Windows, macOS, Linux, iOS, and Android**.

---

## 🛠️ Architectural Blueprint

The emulator architecture is partitioned into cleanly decoupled core components wrapped by an `extern "C"` API interface (`jedgb_api.h`), allowing the engine to run as a standalone SDL2 application or as an embedded library inside custom developer frontends:

```
                 +-----------------------------------+
                |         Frontend / Host           |
                |  (SDL2 App, Mobile Native, etc.)  |
                +-----------------+-----------------+
                                | C API Calls (jedgb_api.h)
                                v
+-----------------+       +-------------------+       +-----------------+
|   CPU (cpu.cpp) | <---> |    Bus (bus.cpp)  | <---> |   PPU (ppu.cpp) |
| Opcode Engine   |       | Memory-Mapped I/O |       | 4-Mode Raster   |
+-----------------+       +---------+---------+       +-----------------+
                                    |
                                    v
               +-------------------+ +-------------------+
               |  APU (apu.cpp)    || Cartridge (cart.cpp)|
               | 4-Ch Sound Synth  | | MBC3 + RAM Save   |
               +-------------------+ +-------------------+

```

### Core Components
*   **C Developer API (`jedgb_api.h` / `jedgb_api.cpp`)**: Exposes an `extern "C"` ABI for seamless integration into custom frontends without C++ compiler name mangling. Supports direct file path loading, in-memory byte buffer loading, framebuffer extraction, audio buffer streaming, and native desktop ROM picker dialogs.
*   **CPU Interpreter (`cpu.cpp`)**: Complete implementation of the Game Boy instruction set, accurate flag behaviors, call/stack manipulation, interrupt servicing prioritization, and precise tracking of the hardware `HALT` bug condition.
*   **System Bus (`bus.cpp`)**: Centralized memory-mapped interconnect managing address decoding across ROM banks, External Save RAM, internal Work RAM, High RAM, Timer structures, and input controllers. Includes cycle-accurate 160-cycle OAM DMA transfer routing and PPU memory lockouts.
*   **PPU Graphics Engine (`ppu.cpp`)**: Line-by-line raster rendering machine operating across Mode 2 (OAM Search), Mode 3 (Pixel Transfer), Mode 0 (H-Blank), and Mode 1 (V-Blank). Supports background layers, internal window line tracking, sprite dynamic sorting, transparency, and rising-edge `STAT` interrupts.
*   **APU Sound Engine (`apu.cpp`)**: Synthesizes 4-channel retro audio streams (Square 1 with Sweep, Square 2, Custom 32-nibble Wave RAM sample tables, and a 15-bit LFSR pseudo-white noise generator) pushed dynamically into standard stereo PCM audio device buffers.
*   **MBC3 Storage Module (`cartridge.cpp`)**: Supports switchable ROM banking, switchable External Game Save RAM with automatic save-to-disk capability (`.sav`), and byte-buffer memory streaming.

---

## 🌐 Cross-Platform Implementation Strategy

To ensure **jedGB** runs seamlessly across **Windows, macOS, Linux, iOS, and Android** without requiring core C++ modifications, the architecture utilizes a multi-layered cross-platform abstraction strategy:

### 1. C ABI Abstraction Layer (`jedgb_api.h` / `jedgb_api.cpp`)
Core C++ objects (`Bus`, `CPU`, `PPU`, `APU`, `Cartridge`) are encapsulated inside an opaque pointer (`JedGBContext*`) using C-compatible function bindings (`extern "C"`). This allows languages like **Swift (iOS)**, **Kotlin (Android)**, **Rust**, and **C#** to call into `jedGB` directly without C++ compiler name mangling issues.

### 2. In-Memory ROM Buffer Streaming (`cartridge.cpp`)
On mobile operating systems (iOS and Android), applications cannot access arbitrary file paths directly due to OS security sandboxing (`content://` URIs on Android and scoped URLs on iOS). **jedGB** provides `jedgb_load_rom_buffer` (`Cartridge::load_from_buffer`), allowing mobile frontends to stream raw ROM bytes directly into the emulator from memory.

### 3. Native Desktop Open-File Dialogs (`tinyfiledialogs.c`)
For desktop platforms, `jedgb_open_rom_picker_dialog` leverages native OS API wrappers (`GetOpenFileNameA` on Windows `comdlg32.dll`) to trigger a file selection window without introducing heavy GUI framework dependencies.

### 4. Universal Event & Touch Input Handling (`main.cpp`)
The standalone frontend utilizes **SDL2** to handle window management, high-DPI scaling, physical keyboard bindings, and touchscreen touch gestures (`SDL_FINGERDOWN` and `SDL_FINGERUP`), allowing the executable to run natively on both desktop monitors and mobile touch displays.

### 5. Multi-Target CMake Pipeline (`CMakeLists.txt`)
The build system produces two distinct build targets:
*   `jedGB`: The standalone executable frontend linked with SDL2.
*   `jedGB-static`: The pre-compiled static engine library ready to be embedded into custom mobile or desktop SDKs.

---

## 📊 Emulation Accuracy & Features (v1.0.0 Status)

All major core hardware timing subsystems have been updated and verified for release:

| Subsystem | Hardware Behavior | Status |
| :--- | :--- | :--- |
| **PPU Mode Timings** | Explicit 4-mode cycle transitions (Modes 2, 3, 0, 1) with rising-edge STAT interrupts | ✅ **Fixed / Accurate** |
| **Memory Lockouts** | VRAM locked during Mode 3; OAM locked during Modes 2 and 3 when LCD is active | ✅ **Fixed / Accurate** |
| **OAM DMA Transfers** | 160 M-cycle background transfer with High RAM (`0xFF80–0xFFFE`) CPU isolation | ✅ **Fixed / Accurate** |
| **Hardware Timers** | 16-bit internal divider network (`DIV`), falling-edge `TIMA` stepping, and 4 T-cycle `TMA` reload delay | ✅ **Fixed / Accurate** |
| **Joypad Matrix** | Active-low line selection (`0xFF00` bits 4/5) for direction/action buttons with edge-triggered interrupts | ✅ **Fixed / Accurate** |
| **Native ROM Picker** | Built-in native file open dialog (`tinyfiledialogs`) for Windows/Mac/Linux | ✅ **Implemented** |

---

## 🚀 Building & Developer SDK Integration

### Prerequisites
*   A compatible **C++20** compiler (GCC, Clang, or MSVC)
*   **SDL2** developer libraries (for the standalone executable target)
*   **CMake** (version 3.20 or newer)

### Build Instructions

```bash
# Clone the repository
git clone [https://github.com/jedPlatforms/jedGB.git](https://github.com/jedPlatforms/jedGB.git)
cd jedGB

# Generate build configuration and compile
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```
---

## 🎮 Controller Layout

The emulator captures classic controller triggers natively via physical hardware mappings:

| Game Boy Button | PC Keyboard Mapping |
| :--- | :--- |
| **D-Pad Up / Down** | `Arrow Up` / `Arrow Down` |
| **D-Pad Left / Right** | `Arrow Left` / `Arrow Right` |
| **Button A** | `Z` |
| **Button B** | `X` |
| **SELECT** | `Backspace` |
| **START** | `Enter` |

---

## 📜 Development Notes & Timeline Synchronization

The current design utilizes an explicit cycle-slicing engine inside the core frame loop (`main.cpp`), feeding exact T-cycle slices sequentially across sub-modules to preserve hardware timing accuracy:

```cpp
int cycles_this_frame = 0;
while (cycles_this_frame < CYCLES_PER_FRAME) {
    int cycles = cpu.step();
    bus.tick(cycles);
    ppu.tick(cycles);
    apu.tick(cycles);
    cycles_this_frame += cycles;
}
```

#include "jedgb_api.h"

// 1. Create emulator context
JedGBContext* gb = jedgb_create();

// 2. Load ROM from file path or memory byte buffer
jedgb_load_rom(gb, "game.gb");

// 3. Step frame (~59.7 FPS)
jedgb_step_frame(gb);

// 4. Retrieve 160x144 RGBA pixel framebuffer
const uint32_t* framebuffer = jedgb_get_framebuffer(gb);

// 5. Cleanup when finished
jedgb_destroy(gb);

---

## 📝 License

Distributed under the MIT License. See `LICENSE` for details.
