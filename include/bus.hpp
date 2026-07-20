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

private:
    std::array<uint8_t, 0x10000> memory{};
    std::shared_ptr<Cartridge> cartridge;
    APU* apu{nullptr};

    // Timer state
    uint16_t timer_counter{0};      
    uint8_t tima{0};
    uint8_t tma{0};
    uint8_t tac{0};
    bool prev_and_result{false};    

    // Basic Display State
    uint8_t ly{0};              // 0xFF44 (Scanline counter 0-153)
    uint8_t lcdc{0x91};         // 0xFF40 (LCD Control)
    uint8_t bgp{0xFC};          // 0xFF47 (BG Palette)
    int scanline_cycles{0};     // Tracks cycles per scanline (456 cycles/line)

    uint8_t joypad_directions{0x0F}; // Lower 4 bits default to unpressed
    uint8_t joypad_buttons{0x0F};    // Lower 4 bits default to unpressed
    uint8_t joypad_select{0x30};     // Bits 4 & 5 default to 1 (deselected)
};