#include "bus.hpp"
#include "apu.hpp"
#include <iostream>

Bus::Bus() {
    memory.fill(0);
    memory[0xFF41] = 0x80; // STAT default
}

void Bus::insert_cartridge(std::shared_ptr<Cartridge> cart) {
    cartridge = cart;
}

void Bus::connect_apu(APU* apu_ptr) {
    apu = apu_ptr;
}

uint8_t Bus::read(uint16_t address) const {
    // ROM and Save RAM
    if ((address < 0x8000 || (address >= 0xA000 && address <= 0xBFFF)) && cartridge && cartridge->is_loaded()) {
        return cartridge->read(address);
    }

    // APU Audio Registers (0xFF10 - 0xFF3F)
    if (address >= 0xFF10 && address <= 0xFF3F) {
        if (apu) return apu->read(address);
        return 0x00;
    }

    // Joypad Register (0xFF00)
    if (address == 0xFF00) {
        uint8_t result = 0xC0 | (joypad_select & 0x30); // Keep upper bits 1s + select state
        
        bool select_directions = !(joypad_select & 0x10); // Bit 4 is 0
        bool select_buttons    = !(joypad_select & 0x20); // Bit 5 is 0

        if (select_directions) {
            result |= (joypad_directions & 0x0F);
        } else if (select_buttons) {
            result |= (joypad_buttons & 0x0F);
        } else {
            result |= 0x0F; // Neither selected
        }

        return result;
    }

    // Timers
    if (address == 0xFF04) return static_cast<uint8_t>(timer_counter >> 8);
    if (address == 0xFF05) return tima;
    if (address == 0xFF06) return tma;
    if (address == 0xFF07) return tac | 0xF8;

    // LCD / PPU Registers
    if (address == 0xFF40) return memory[0xFF40]; // LCDC
    if (address == 0xFF41) return memory[0xFF41]; // STAT
    if (address == 0xFF44) return ly;             // LY
    if (address == 0xFF45) return memory[0xFF45]; // LYC
    if (address == 0xFF47) return memory[0xFF47]; // BGP
    if (address == 0xFF48) return memory[0xFF48]; // OBP0
    if (address == 0xFF49) return memory[0xFF49]; // OBP1

    // Interrupt Request Register (0xFF0F)
    if (address == 0xFF0F) {
        return memory[address] | 0xE0;
    }

    return memory[address];
}

void Bus::write(uint16_t address, uint8_t value) {
    // ROM and Save RAM Registers
    if ((address < 0x8000 || (address >= 0xA000 && address <= 0xBFFF)) && cartridge) {
        cartridge->write(address, value);
        return;
    }

    // APU Audio Registers (0xFF10 - 0xFF3F)
    if (address >= 0xFF10 && address <= 0xFF3F) {
        if (apu) apu->write(address, value);
        return;
    }

    // Joypad Register (0xFF00)
    if (address == 0xFF00) {
        joypad_select = value & 0x30;
        return;
    }

    // Timers
    if (address == 0xFF04) { timer_counter = 0; return; }
    if (address == 0xFF05) { tima = value; return; }
    if (address == 0xFF06) { tma = value; return; }
    if (address == 0xFF07) { tac = value & 0x07; return; }

    // LCD / PPU Registers
    if (address == 0xFF40) { memory[0xFF40] = value; return; }
    if (address == 0xFF41) { 
        // Bits 0-2 are read-only status bits
        memory[0xFF41] = (memory[0xFF41] & 0x07) | (value & 0xF8); 
        return; 
    }
    if (address == 0xFF44) { ly = 0; return; } // Writing to LY resets it
    if (address == 0xFF45) { memory[0xFF45] = value; return; }
    if (address == 0xFF47) { memory[0xFF47] = value; return; }
    if (address == 0xFF48) { memory[0xFF48] = value; return; }
    if (address == 0xFF49) { memory[0xFF49] = value; return; }

    // OAM DMA Transfer (0xFF46)
    if (address == 0xFF46) {
        uint16_t src_addr = static_cast<uint16_t>(value) << 8;
        for (uint16_t i = 0; i < 0xA0; ++i) {
            write(0xFE00 + i, read(src_addr + i));
        }
        return;
    }

    // Serial Data Output (Blargg tests output)
    if (address == 0xFF02 && value == 0x81) {
        char c = static_cast<char>(memory[0xFF01]);
        std::cout << c << std::flush;
        memory[0xFF02] = 0;
    }

    // Interrupt Request Register
    if (address == 0xFF0F) {
        memory[address] = value & 0x1F;
        return;
    }

    memory[address] = value;
}

void Bus::set_button_state(uint8_t button_bit, bool pressed) {
    bool is_action = (button_bit >= 4);
    uint8_t bit_mask = 1 << (button_bit % 4);

    uint8_t& target = is_action ? joypad_buttons : joypad_directions;
    uint8_t old_state = target;

    if (pressed) {
        target &= ~bit_mask; // Active Low (0 = pressed)
    } else {
        target |= bit_mask;  // Active High (1 = unpressed)
    }

    // Trigger Joypad Interrupt (Bit 4 of IF) on high-to-low transition
    if ((old_state & bit_mask) && !(target & bit_mask)) {
        memory[0xFF0F] |= 0x10; // Request Joypad Interrupt
    }
}

void Bus::tick(int cycles) {
    // 1. Timer logic
    static constexpr int bit_for_tac[4] = {9, 3, 5, 7};

    for (int i = 0; i < cycles; ++i) {
        timer_counter++;

        int bit = bit_for_tac[tac & 0x03];
        bool tac_enabled = (tac & 0x04) != 0;
        bool and_result = tac_enabled && ((timer_counter >> bit) & 1);

        if (prev_and_result && !and_result) {
            if (tima == 0xFF) {
                tima = tma;
                memory[0xFF0F] |= 0x04; // Timer interrupt
            } else {
                tima++;
            }
        }
        prev_and_result = and_result;
    }

    // 2. Display Scanline & STAT Interrupt Logic
    scanline_cycles += cycles;
    if (scanline_cycles >= 456) { // 1 scanline = 456 T-cycles
        scanline_cycles -= 456;
        ly++;

        if (ly > 153) {
            ly = 0;
        }

        // --- Check LY == LYC Compare ---
        uint8_t lyc = memory[0xFF45];
        uint8_t stat = memory[0xFF41];

        if (ly == lyc) {
            stat |= 0x04; // Set LYC==LY flag (Bit 2)
            if (stat & 0x40) { // If Bit 6 (LYC STAT Interrupt Enable) is set
                memory[0xFF0F] |= 0x02; // Request STAT Interrupt (Bit 1)
            }
        } else {
            stat &= ~0x04; // Clear LYC==LY flag
        }

        // --- V-Blank Interrupt ---
        if (ly == 144) {
            memory[0xFF0F] |= 0x01; // Request V-Blank Interrupt (Bit 0)
            if (stat & 0x10) {      // Mode 1 V-Blank STAT Interrupt Enable
                memory[0xFF0F] |= 0x02; // Request STAT Interrupt
            }
        }

        memory[0xFF41] = stat;
    }
}