#pragma once
#include <cstdint>
#include <array>
#include <memory>
#include "cartridge.hpp"

// Forward declaration for APU
class APU;

class Bus {
public:
    Bus();
    void insert_cartridge(std::shared_ptr<Cartridge> cart);
    void connect_apu(APU* apu_ptr);

    uint8_t read(uint16_t address) const;
    void write(uint16_t address, uint8_t value);
    void tick(int cycles);
    void set_button_state(uint8_t button_bit, bool pressed);

    uint8_t get_ly() const { return ly; }
    bool is_dma_active() const { return dma_active; }

private:
    std::array<uint8_t, 0x10000> memory{};
    std::shared_ptr<Cartridge> cartridge;
    APU* apu{nullptr};

    // --- Timer Subsystem State ---
    uint16_t internal_div{0xABCC}; // Internal 16-bit counter (DIV is internal_div >> 8)
    uint8_t tima{0};
    uint8_t tma{0};
    uint8_t tac{0};

    // Timer Edge Detection & Reload Delay State
    bool prev_timer_bit{false};
    bool tima_overflow_queued{false};
    int tima_reload_delay{0}; // Tracks the 4 T-cycle reload delay

    // --- Display / PPU State ---
    uint8_t ly{0};              // 0xFF44 (Scanline counter 0-153)
    uint8_t lcdc{0x91};         // 0xFF40 (LCD Control)
    uint8_t bgp{0xFC};          // 0xFF47 (BG Palette)
    int scanline_cycles{0};     // Tracks cycles per scanline (456 cycles/line)

    // --- Joypad Matrix State ---
    uint8_t joypad_directions{0x0F}; // Lower 4 bits: Right, Left, Up, Down (1 = released, 0 = pressed)
    uint8_t joypad_buttons{0x0F};    // Lower 4 bits: A, B, Select, Start (1 = released, 0 = pressed)
    uint8_t joypad_select{0x30};     // Bits 4 & 5 default to 1 (deselected)

    // --- OAM DMA State ---
    bool dma_active{false};
    uint16_t dma_source{0};
    uint8_t dma_byte_index{0};
    int dma_delay{0}; // Tracks the 1 M-Cycle startup delay

    void check_timer_falling_edge();
};