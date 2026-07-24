#include "bus.hpp"
#include "apu.hpp"
#include <iostream>

Bus::Bus() {
    memory.fill(0);
    memory[0xFF41] = 0x80; // STAT default (Bit 7 is always 1)
}

void Bus::insert_cartridge(std::shared_ptr<Cartridge> cart) {
    cartridge = cart;
}

void Bus::connect_apu(APU* apu_ptr) {
    apu = apu_ptr;
}

void Bus::check_timer_falling_edge() {
    static constexpr int bit_for_tac[4] = {9, 3, 5, 7};

    int bit = bit_for_tac[tac & 0x03];
    bool tac_enabled = (tac & 0x04) != 0;
    bool current_bit = tac_enabled && ((internal_div >> bit) & 1);

    // Falling Edge: Signal went from HIGH (1) to LOW (0)
    if (prev_timer_bit && !current_bit) {
        tima++;
        if (tima == 0) { // Overflow occurred (0xFF -> 0x00)
            tima_overflow_queued = true;
            tima_reload_delay = 4; // Takes 4 T-cycles (1 M-cycle) to reload TMA & set IF
        }
    }

    prev_timer_bit = current_bit;
}

uint8_t Bus::read(uint16_t address) const {
    // -------------------------------------------------------------------------
    // 1. OAM DMA Lockout
    // During active DMA, the CPU can ONLY access High RAM (0xFF80 - 0xFFFE)
    // -------------------------------------------------------------------------
    if (dma_active && dma_delay == 0) {
        if (address < 0xFF80 || address > 0xFFFE) {
            return 0xFF; // CPU read blocked by DMA
        }
    }

    uint8_t stat = memory[0xFF41];
    uint8_t mode = stat & 0x03;
    bool lcd_enabled = (memory[0xFF40] & 0x80) != 0;

    // -------------------------------------------------------------------------
    // 2. PPU Memory Lockouts
    // -------------------------------------------------------------------------
    if (address >= 0x8000 && address <= 0x9FFF) {
        if (lcd_enabled && mode == 3) {
            return 0xFF; // CPU read blocked during Mode 3
        }
    }

    if (address >= 0xFE00 && address <= 0xFE9F) {
        if (lcd_enabled && (mode == 2 || mode == 3)) {
            return 0xFF; // CPU read blocked during Modes 2 & 3
        }
    }

    // ROM and Save RAM
    if ((address < 0x8000 || (address >= 0xA000 && address <= 0xBFFF)) && cartridge && cartridge->is_loaded()) {
        return cartridge->read(address);
    }

    // APU Audio Registers (0xFF10 - 0xFF3F)
    if (address >= 0xFF10 && address <= 0xFF3F) {
        if (apu) return apu->read(address);
        return 0x00;
    }

    // -------------------------------------------------------------------------
    // 3. Joypad Register Matrix Decoding (0xFF00)
    // -------------------------------------------------------------------------
    if (address == 0xFF00) {
        uint8_t result = 0xC0 | (joypad_select & 0x30); // Bits 6-7 are 1s, keep selection bits
        
        bool select_directions = !(joypad_select & 0x10); // Bit 4 is 0 (Active Low)
        bool select_buttons    = !(joypad_select & 0x20); // Bit 5 is 0 (Active Low)

        if (select_directions && select_buttons) {
            // Both selected: Hardware ANDs the two nibbles together
            result |= (joypad_directions & joypad_buttons & 0x0F);
        } else if (select_directions) {
            result |= (joypad_directions & 0x0F);
        } else if (select_buttons) {
            result |= (joypad_buttons & 0x0F);
        } else {
            result |= 0x0F; // Neither selected: return all 1s (unpressed)
        }

        return result;
    }

    // Hardware Timers
    if (address == 0xFF04) return static_cast<uint8_t>(internal_div >> 8);
    if (address == 0xFF05) return tima;
    if (address == 0xFF06) return tma;
    if (address == 0xFF07) return tac | 0xF8;

    // LCD / PPU Registers
    if (address == 0xFF40) return memory[0xFF40]; 
    if (address == 0xFF41) return memory[0xFF41]; 
    if (address == 0xFF44) return ly;             
    if (address == 0xFF45) return memory[0xFF45]; 
    if (address == 0xFF47) return memory[0xFF47]; 
    if (address == 0xFF48) return memory[0xFF48]; 
    if (address == 0xFF49) return memory[0xFF49]; 

    // Interrupt Request Register (0xFF0F)
    if (address == 0xFF0F) {
        return memory[address] | 0xE0;
    }

    return memory[address];
}

void Bus::write(uint16_t address, uint8_t value) {
    // -------------------------------------------------------------------------
    // 1. OAM DMA Lockout & Trigger
    // -------------------------------------------------------------------------
    if (address == 0xFF46) {
        dma_source = static_cast<uint16_t>(value) << 8;
        dma_active = true;
        dma_byte_index = 0;
        dma_delay = 4;
        return;
    }

    if (dma_active && dma_delay == 0) {
        if (address < 0xFF80 || address > 0xFFFE) {
            return; // CPU write blocked by DMA
        }
    }

    uint8_t stat = memory[0xFF41];
    uint8_t mode = stat & 0x03;
    bool lcd_enabled = (memory[0xFF40] & 0x80) != 0;

    // -------------------------------------------------------------------------
    // 2. PPU Memory Lockouts
    // -------------------------------------------------------------------------
    if (address >= 0x8000 && address <= 0x9FFF) {
        if (lcd_enabled && mode == 3) return;
    }

    if (address >= 0xFE00 && address <= 0xFE9F) {
        if (lcd_enabled && (mode == 2 || mode == 3)) return;
    }

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

    // -------------------------------------------------------------------------
    // 3. Joypad Register Selection Write (0xFF00)
    // -------------------------------------------------------------------------
    if (address == 0xFF00) {
        joypad_select = value & 0x30; // Only bits 4 and 5 are writable
        return;
    }

    // Hardware Timers
    if (address == 0xFF04) { 
        internal_div = 0; 
        check_timer_falling_edge();
        return; 
    }
    if (address == 0xFF05) { 
        if (tima_reload_delay > 0) {
            tima_overflow_queued = false;
            tima_reload_delay = 0;
        }
        tima = value; 
        return; 
    }
    if (address == 0xFF06) { 
        tma = value; 
        if (tima_reload_delay > 0 && tima == 0x00) {
            tima = value;
        }
        return; 
    }
    if (address == 0xFF07) { 
        tac = value & 0x07; 
        check_timer_falling_edge();
        return; 
    }

    // LCD / PPU Registers
    if (address == 0xFF40) { memory[0xFF40] = value; return; }
    if (address == 0xFF41) { 
        memory[0xFF41] = (memory[0xFF41] & 0x07) | (value & 0x78) | 0x80; 
        return; 
    }
    if (address == 0xFF44) { ly = 0; return; } 
    if (address == 0xFF45) { memory[0xFF45] = value; return; }
    if (address == 0xFF47) { memory[0xFF47] = value; return; }
    if (address == 0xFF48) { memory[0xFF48] = value; return; }
    if (address == 0xFF49) { memory[0xFF49] = value; return; }

    // Serial Data Output
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
        target &= ~bit_mask; // Active Low: 0 = pressed
    } else {
        target |= bit_mask;  // Active High: 1 = unpressed
    }

    // Check if line selection enables interrupt request (High-to-Low transition)
    bool select_directions = !(joypad_select & 0x10);
    bool select_buttons    = !(joypad_select & 0x20);

    bool line_selected = (is_action && select_buttons) || (!is_action && select_directions);

    // Trigger Joypad Interrupt (Bit 4 of IF) when pressed on a selected line
    if (line_selected && (old_state & bit_mask) && !(target & bit_mask)) {
        memory[0xFF0F] |= 0x10; // Request Joypad Interrupt
    }
}

void Bus::tick(int cycles) {
    // -------------------------------------------------------------------------
    // 0. OAM DMA Stepping Logic
    // -------------------------------------------------------------------------
    for (int c = 0; c < cycles; ++c) {
        if (dma_active) {
            if (dma_delay > 0) {
                dma_delay--;
            } else {
                if ((dma_byte_index < 0xA0) && (c % 4 == 0)) {
                    uint8_t byte_val = memory[dma_source + dma_byte_index];
                    memory[0xFE00 + dma_byte_index] = byte_val;
                    dma_byte_index++;

                    if (dma_byte_index >= 0xA0) {
                        dma_active = false;
                    }
                }
            }
        }
    }

    // -------------------------------------------------------------------------
    // 1. Hardware Timer Subsystem
    // -------------------------------------------------------------------------
    for (int i = 0; i < cycles; ++i) {
        if (tima_overflow_queued) {
            tima_reload_delay--;
            if (tima_reload_delay == 0) {
                tima = tma;             
                memory[0xFF0F] |= 0x04; // Set Timer Interrupt flag
                tima_overflow_queued = false;
            }
        }

        internal_div++;
        check_timer_falling_edge();
    }

    // -------------------------------------------------------------------------
    // 2. Display Scanline & STAT Interrupt Logic
    // -------------------------------------------------------------------------
    bool lcd_enabled = (memory[0xFF40] & 0x80) != 0;

    if (!lcd_enabled) {
        scanline_cycles = 0;
        ly = 0;
        memory[0xFF41] = (memory[0xFF41] & 0x78) | 0x80; 
        return; 
    }

    uint8_t stat = memory[0xFF41];
    uint8_t current_mode = stat & 0x03;
    uint8_t new_mode = current_mode;
    bool req_stat_interrupt = false;

    scanline_cycles += cycles;

    if (scanline_cycles >= 456) {
        scanline_cycles -= 456;
        ly++;

        if (ly > 153) {
            ly = 0;
        }
    }

    bool lyc_match = (ly == memory[0xFF45]);
    bool prev_lyc_match = (stat & 0x04) != 0;

    if (lyc_match) {
        stat |= 0x04; 
        if (!prev_lyc_match && (stat & 0x40)) {
            req_stat_interrupt = true; 
        }
    } else {
        stat &= ~0x04; 
    }

    if (ly >= 144) {
        new_mode = 1; 
    } else {
        if (scanline_cycles < 80) {
            new_mode = 2; 
        } else if (scanline_cycles < 252) { 
            new_mode = 3; 
        } else {
            new_mode = 0; 
        }
    }

    if (new_mode != current_mode) {
        if (new_mode == 1) {
            memory[0xFF0F] |= 0x01; 
            if (stat & 0x10) req_stat_interrupt = true;
        } else if (new_mode == 2) {
            if (stat & 0x20) req_stat_interrupt = true;
        } else if (new_mode == 0) {
            if (stat & 0x08) req_stat_interrupt = true;
        }
    }

    if (req_stat_interrupt) {
        memory[0xFF0F] |= 0x02;
    }

    memory[0xFF41] = (stat & 0xFC) | new_mode;
}