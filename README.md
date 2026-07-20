# jedGB

A lightweight, clean, and highly structured Game Boy (DMG-01) emulator written in modern C++ and powered by SDL2. 

jedGB implements the core components of the Game Boy hardware, featuring an accurate sharp-like CPU interpreter, memory-mapped I/O routing, synchronous PPU pixel pipeline processing, and an accurate APU sound generation subsystem.

---

## 🛠️ Architectural Blueprint

The emulator architecture is partitioned into cleanly decoupled components, utilizing an interconnected bus pattern for communication:

```
                  +-----------------------------------+
                  |            main.cpp               |
                  |     (SDL2 Window / Audio Loop)    |
                  +-----------------+-----------------+
                                    | Ticks Components
                                    v
+-----------------+       +-------------------+       +-----------------+
|   CPU (cpu.cpp) | <---> |    Bus (bus.cpp)  | <---> |   PPU (ppu.cpp) |
| Opcode Engine   |       | Memory-Mapped I/O |       | Scanline Raster |
+-----------------+       +---------+---------+       +-----------------+
                                    |
                                    v
                          +-------------------+
                          |   APU (apu.cpp)   |
                          | 4-Ch Sound Synth  |
                          +-------------------+
```

### Core Components
*   **CPU Interpreter (`cpu.cpp`)**: Complete implementation of the Game Boy's unique instruction set, accurate flag behaviors, call/stack management, interrupt servicing prioritization, and precise tracking of the hardware `HALT` bug condition.
*   **System Bus (`bus.cpp`)**: Centralized memory-mapped interconnect managing address decoding across ROM banks, External Save RAM, internal Work RAM, High RAM, Timer structures, and input controllers. Includes automated OAM DMA transfer routing.
*   **PPU Graphics Engine (`ppu.cpp`)**: Line-by-line raster rendering machine mapping full Background layers, an advanced internal Window line tracker for vertical offsets, and accurate Sprite (OBJ) extraction supporting dynamic sorting and transparency mechanics.
*   **APU Sound Engine (`apu.cpp`)**: Synthesizes 4-channel retro audio streams (Square 1 with Sweep, Square 2, Custom 32-nibble Wave RAM sample tables, and a 15-bit LFSR pseudo-white noise generator) pushed dynamically into standard stereo PCM audio device buffers.
*   **MBC3 Storage Module (`cartridge.cpp`)**: Supports switchable ROM banking up to 128 banks, and up to 4 switchable banks of External Game Save RAM with automatic save-to-disk capability serialization (`.sav` tracking).

---

## 🚀 Getting Started

### Prerequisites
*   A compatible **C++17** or **C++20** compiler (e.g., GCC, Clang, or MSVC)
*   **SDL2** developer libraries installed on your development environment
*   **CMake** (version 3.15 or newer recommended)

### Build Instructions

```bash
# Clone the repository
git clone https://github.com/jedPlatforms/jedGB.git
cd jedGB

# Create a build directory
mkdir build && cd build

# Generate build configuration and compile
cmake ..
cmake --build .
```

### Running the Emulator

Provide the target Game Boy ROM (`.gb` or `.dmg`) path as the primary launch argument:

```bash
./jedGB path/to/your/game.gb
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

---

## 📝 License

Distributed under the MIT License. See `LICENSE` for details.
