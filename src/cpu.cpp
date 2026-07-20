#include "cpu.hpp"
#include "bus.hpp"
#include <iostream>

CPU::CPU(Bus& bus) : bus(bus) {
    reset();
}

void CPU::reset() {
    pc = 0x0100;
    sp = 0xFFFE;
    a = 0x01; f = 0xB0;
    b = 0x00; c = 0x13;
    d = 0x00; e = 0xD8;
    h = 0x01; l = 0x4D;
    ime = false;
    enabling_ime = false;
    halt_mode = false;
    halt_bug = false;
}

uint8_t CPU::fetch_byte() {
    uint8_t val = bus.read(pc);
    
    // Process HALT bug: return the byte but do NOT increment the PC
    if (halt_bug) {
        halt_bug = false;
    } else {
        pc++;
    }
    
    return val;
}

uint16_t CPU::fetch_word() {
    uint8_t low = fetch_byte();
    uint8_t high = fetch_byte();
    return (static_cast<uint16_t>(high) << 8) | low;
}

void CPU::push16(uint16_t val) {
    sp--;
    bus.write(sp, (val >> 8) & 0xFF);
    sp--;
    bus.write(sp, val & 0xFF);
}

uint16_t CPU::pop16() {
    uint8_t low = bus.read(sp++);
    uint8_t high = bus.read(sp++);
    return (static_cast<uint16_t>(high) << 8) | low;
}

int CPU::handle_interrupts() {
    uint8_t ie = bus.read(0xFFFF);
    uint8_t if_reg = bus.read(0xFF0F);
    uint8_t pending = ie & if_reg & 0x1F;

    if (pending > 0) {
        // Kahit naka-disable ang IME, nagigising ang CPU mula sa HALT
        halt_mode = false; 

        if (ime) {
            // Hanapin ang unang active bit (highest priority: V-Blank -> LCD -> Timer -> Serial -> Joypad)
            for (int i = 0; i < 5; ++i) {
                if (pending & (1 << i)) {
                    ime = false;
                    enabling_ime = false; 

                    // Isulat ang pag-clear sa IF gamit ang pinakasariwang data mula sa bus
                    uint8_t current_if = bus.read(0xFF0F);
                    bus.write(0xFF0F, current_if & ~(1 << i));

                    push16(pc);
                    pc = 0x0040 + (i * 8);
                    return 20; // 5 M-cycles ang konsumo ng interrupt servicing
                }
            }
        }
    }
    return 0; 
}

void CPU::alu_add(uint8_t val) {
    uint16_t res = a + val;
    set_flag(FLAG_Z, (res & 0xFF) == 0);
    set_flag(FLAG_N, false);
    set_flag(FLAG_H, ((a & 0x0F) + (val & 0x0F)) > 0x0F);
    set_flag(FLAG_C, res > 0xFF);
    a = static_cast<uint8_t>(res);
}

void CPU::alu_adc(uint8_t val) {
    uint8_t carry = get_flag(FLAG_C) ? 1 : 0;
    uint16_t res = a + val + carry;
    set_flag(FLAG_Z, (res & 0xFF) == 0);
    set_flag(FLAG_N, false);
    set_flag(FLAG_H, ((a & 0x0F) + (val & 0x0F) + carry) > 0x0F);
    set_flag(FLAG_C, res > 0xFF);
    a = static_cast<uint8_t>(res);
}

void CPU::alu_sub(uint8_t val) {
    int res = a - val;
    set_flag(FLAG_Z, (res & 0xFF) == 0);
    set_flag(FLAG_N, true);
    set_flag(FLAG_H, (a & 0x0F) < (val & 0x0F));
    set_flag(FLAG_C, a < val);
    a = static_cast<uint8_t>(res);
}

void CPU::alu_sbc(uint8_t val) {
    uint8_t carry = get_flag(FLAG_C) ? 1 : 0;
    int res = a - val - carry;
    set_flag(FLAG_Z, (res & 0xFF) == 0);
    set_flag(FLAG_N, true);
    set_flag(FLAG_H, (a & 0x0F) < ((val & 0x0F) + carry));
    set_flag(FLAG_C, res < 0);
    a = static_cast<uint8_t>(res);
}

void CPU::alu_and(uint8_t val) {
    a &= val;
    set_flag(FLAG_Z, a == 0);
    set_flag(FLAG_N, false);
    set_flag(FLAG_H, true);
    set_flag(FLAG_C, false);
}

void CPU::alu_or(uint8_t val) {
    a |= val;
    set_flag(FLAG_Z, a == 0);
    set_flag(FLAG_N, false);
    set_flag(FLAG_H, false);
    set_flag(FLAG_C, false);
}

void CPU::alu_xor(uint8_t val) {
    a ^= val;
    set_flag(FLAG_Z, a == 0);
    set_flag(FLAG_N, false);
    set_flag(FLAG_H, false);
    set_flag(FLAG_C, false);
}

void CPU::alu_cp(uint8_t val) {
    set_flag(FLAG_Z, a == val);
    set_flag(FLAG_N, true);
    set_flag(FLAG_H, (a & 0x0F) < (val & 0x0F));
    set_flag(FLAG_C, a < val);
}

uint8_t CPU::alu_inc(uint8_t val) {
    uint8_t res = val + 1;
    set_flag(FLAG_Z, res == 0);
    set_flag(FLAG_N, false);
    set_flag(FLAG_H, (val & 0x0F) == 0x0F);
    return res;
}

uint8_t CPU::alu_dec(uint8_t val) {
    uint8_t res = val - 1;
    set_flag(FLAG_Z, res == 0);
    set_flag(FLAG_N, true);
    set_flag(FLAG_H, (val & 0x0F) == 0x00);
    return res;
}

int CPU::step() {
    int interrupt_cycles = handle_interrupts();
    if (interrupt_cycles > 0) {
        return interrupt_cycles;
    }

    if (halt_mode) {
        return 4; 
    }

    bool ime_just_enabled = enabling_ime;   // was EI executed last instruction?

    uint8_t opcode = fetch_byte();
    int cycles = execute_opcode(opcode);    // runs the (now separate) switch

    if (ime_just_enabled) {
        ime = true;
        enabling_ime = false;
    }

    return cycles;
}

int CPU::execute_opcode(uint8_t opcode) {
    switch (opcode) {
        case 0x00: return 4;
        case 0x10: fetch_byte(); return 4;

        
        case 0x76: {
            // HALT Logic processing
            uint8_t pending = bus.read(0xFFFF) & bus.read(0xFF0F) & 0x1F;
            if (ime) {
                halt_mode = true;
            } else {
                if (pending > 0) {
                    halt_bug = true; // Trigger the hardware bug!
                } else {
                    halt_mode = true;
                }
            }
            return 4;
        }
        
        case 0xF3: ime = false; enabling_ime = false; return 4; 
        case 0xFB: enabling_ime = true; return 4; 

        case 0x01: set_bc(fetch_word()); return 12;
        case 0x11: set_de(fetch_word()); return 12;
        case 0x21: set_hl(fetch_word()); return 12;
        case 0x31: sp = fetch_word(); return 12;

        case 0x06: b = fetch_byte(); return 8;
        case 0x0E: c = fetch_byte(); return 8;
        case 0x16: d = fetch_byte(); return 8;
        case 0x1E: e = fetch_byte(); return 8;
        case 0x26: h = fetch_byte(); return 8;
        case 0x2E: l = fetch_byte(); return 8;
        case 0x36: bus.write(get_hl(), fetch_byte()); return 12;
        case 0x3E: a = fetch_byte(); return 8;

        case 0x7F: a = a; return 4; case 0x78: a = b; return 4; case 0x79: a = c; return 4; case 0x7A: a = d; return 4; case 0x7B: a = e; return 4; case 0x7C: a = h; return 4; case 0x7D: a = l; return 4; case 0x7E: a = bus.read(get_hl()); return 8;
        case 0x40: b = b; return 4; case 0x41: b = c; return 4; case 0x42: b = d; return 4; case 0x43: b = e; return 4; case 0x44: b = h; return 4; case 0x45: b = l; return 4; case 0x46: b = bus.read(get_hl()); return 8; case 0x47: b = a; return 4;
        case 0x48: c = b; return 4; case 0x49: c = c; return 4; case 0x4A: c = d; return 4; case 0x4B: c = e; return 4; case 0x4C: c = h; return 4; case 0x4D: c = l; return 4; case 0x4E: c = bus.read(get_hl()); return 8; case 0x4F: c = a; return 4;
        case 0x50: d = b; return 4; case 0x51: d = c; return 4; case 0x52: d = d; return 4; case 0x53: d = e; return 4; case 0x54: d = h; return 4; case 0x55: d = l; return 4; case 0x56: d = bus.read(get_hl()); return 8; case 0x57: d = a; return 4;
        case 0x58: e = b; return 4; case 0x59: e = c; return 4; case 0x5A: e = d; return 4; case 0x5B: e = e; return 4; case 0x5C: e = h; return 4; case 0x5D: e = l; return 4; case 0x5E: e = bus.read(get_hl()); return 8; case 0x5F: e = a; return 4;
        case 0x60: h = b; return 4; case 0x61: h = c; return 4; case 0x62: h = d; return 4; case 0x63: h = e; return 4; case 0x64: h = h; return 4; case 0x65: h = l; return 4; case 0x66: h = bus.read(get_hl()); return 8; case 0x67: h = a; return 4;
        case 0x68: l = b; return 4; case 0x69: l = c; return 4; case 0x6A: l = d; return 4; case 0x6B: l = e; return 4; case 0x6C: l = h; return 4; case 0x6D: l = l; return 4; case 0x6E: l = bus.read(get_hl()); return 8; case 0x6F: l = a; return 4;

        case 0x70: bus.write(get_hl(), b); return 8;
        case 0x71: bus.write(get_hl(), c); return 8;
        case 0x72: bus.write(get_hl(), d); return 8;
        case 0x73: bus.write(get_hl(), e); return 8;
        case 0x74: bus.write(get_hl(), h); return 8;
        case 0x75: bus.write(get_hl(), l); return 8;
        case 0x77: bus.write(get_hl(), a); return 8;

        case 0x02: bus.write(get_bc(), a); return 8;
        case 0x12: bus.write(get_de(), a); return 8;
        case 0x22: bus.write(get_hl(), a); set_hl(get_hl() + 1); return 8;
        case 0x32: bus.write(get_hl(), a); set_hl(get_hl() - 1); return 8;
        case 0x0A: a = bus.read(get_bc()); return 8;
        case 0x1A: a = bus.read(get_de()); return 8;
        case 0x2A: a = bus.read(get_hl()); set_hl(get_hl() + 1); return 8;
        case 0x3A: a = bus.read(get_hl()); set_hl(get_hl() - 1); return 8;

        case 0xE0: bus.write(0xFF00 + fetch_byte(), a); return 12;
        case 0xF0: a = bus.read(0xFF00 + fetch_byte()); return 12;
        case 0xE2: bus.write(0xFF00 + c, a); return 8;
        case 0xF2: a = bus.read(0xFF00 + c); return 8;
        case 0xEA: bus.write(fetch_word(), a); return 16;
        case 0xFA: a = bus.read(fetch_word()); return 16;

        // --- ADD / ADC ---
        case 0x87: alu_add(a); return 4; case 0x80: alu_add(b); return 4; case 0x81: alu_add(c); return 4; case 0x82: alu_add(d); return 4; case 0x83: alu_add(e); return 4; case 0x84: alu_add(h); return 4; case 0x85: alu_add(l); return 4; case 0x86: alu_add(bus.read(get_hl())); return 8; case 0xC6: alu_add(fetch_byte()); return 8;
        case 0x8F: alu_adc(a); return 4; case 0x88: alu_adc(b); return 4; case 0x89: alu_adc(c); return 4; case 0x8A: alu_adc(d); return 4; case 0x8B: alu_adc(e); return 4; case 0x8C: alu_adc(h); return 4; case 0x8D: alu_adc(l); return 4; case 0x8E: alu_adc(bus.read(get_hl())); return 8; case 0xCE: alu_adc(fetch_byte()); return 8;

        // --- SUB / SBC ---
        case 0x97: alu_sub(a); return 4; case 0x90: alu_sub(b); return 4; case 0x91: alu_sub(c); return 4; case 0x92: alu_sub(d); return 4; case 0x93: alu_sub(e); return 4; case 0x94: alu_sub(h); return 4; case 0x95: alu_sub(l); return 4; case 0x96: alu_sub(bus.read(get_hl())); return 8; case 0xD6: alu_sub(fetch_byte()); return 8;
        case 0x9F: alu_sbc(a); return 4; case 0x98: alu_sbc(b); return 4; case 0x99: alu_sbc(c); return 4; case 0x9A: alu_sbc(d); return 4; case 0x9B: alu_sbc(e); return 4; case 0x9C: alu_sbc(h); return 4; case 0x9D: alu_sbc(l); return 4; case 0x9E: alu_sbc(bus.read(get_hl())); return 8; case 0xDE: alu_sbc(fetch_byte()); return 8;

        // --- AND / OR / XOR / CP ---
        case 0xA7: alu_and(a); return 4; case 0xA0: alu_and(b); return 4; case 0xA1: alu_and(c); return 4; case 0xA2: alu_and(d); return 4; case 0xA3: alu_and(e); return 4; case 0xA4: alu_and(h); return 4; case 0xA5: alu_and(l); return 4; case 0xA6: alu_and(bus.read(get_hl())); return 8; case 0xE6: alu_and(fetch_byte()); return 8;
        case 0xB7: alu_or(a); return 4;  case 0xB0: alu_or(b); return 4;  case 0xB1: alu_or(c); return 4;  case 0xB2: alu_or(d); return 4;  case 0xB3: alu_or(e); return 4;  case 0xB4: alu_or(h); return 4;  case 0xB5: alu_or(l); return 4;  case 0xB6: alu_or(bus.read(get_hl())); return 8;  case 0xF6: alu_or(fetch_byte()); return 8;
        case 0xAF: alu_xor(a); return 4; case 0xA8: alu_xor(b); return 4; case 0xA9: alu_xor(c); return 4; case 0xAA: alu_xor(d); return 4; case 0xAB: alu_xor(e); return 4; case 0xAC: alu_xor(h); return 4; case 0xAD: alu_xor(l); return 4; case 0xAE: alu_xor(bus.read(get_hl())); return 8; case 0xEE: alu_xor(fetch_byte()); return 8;
        case 0xBF: alu_cp(a); return 4;  case 0xB8: alu_cp(b); return 4;  case 0xB9: alu_cp(c); return 4;  case 0xBA: alu_cp(d); return 4;  case 0xBB: alu_cp(e); return 4;  case 0xBC: alu_cp(h); return 4;  case 0xBD: alu_cp(l); return 4;  case 0xBE: alu_cp(bus.read(get_hl())); return 8;  case 0xFE: alu_cp(fetch_byte()); return 8;

        // --- INC / DEC ---
        case 0x3C: a = alu_inc(a); return 4; case 0x04: b = alu_inc(b); return 4; case 0x0C: c = alu_inc(c); return 4; case 0x14: d = alu_inc(d); return 4; case 0x1C: e = alu_inc(e); return 4; case 0x24: h = alu_inc(h); return 4; case 0x2C: l = alu_inc(l); return 4; case 0x34: bus.write(get_hl(), alu_inc(bus.read(get_hl()))); return 12;
        case 0x3D: a = alu_dec(a); return 4; case 0x05: b = alu_dec(b); return 4; case 0x0D: c = alu_dec(c); return 4; case 0x15: d = alu_dec(d); return 4; case 0x1D: e = alu_dec(e); return 4; case 0x25: h = alu_dec(h); return 4; case 0x2D: l = alu_dec(l); return 4; case 0x35: bus.write(get_hl(), alu_dec(bus.read(get_hl()))); return 12;

        case 0x03: set_bc(get_bc() + 1); return 8;
        case 0x13: set_de(get_de() + 1); return 8;
        case 0x23: set_hl(get_hl() + 1); return 8;
        case 0x33: sp++; return 8;

        case 0x0B: set_bc(get_bc() - 1); return 8;
        case 0x1B: set_de(get_de() - 1); return 8;
        case 0x2B: set_hl(get_hl() - 1); return 8;
        case 0x3B: sp--; return 8;

        case 0x09: { uint32_t res = get_hl() + get_bc(); set_flag(FLAG_N, false); set_flag(FLAG_H, ((get_hl() & 0x0FFF) + (get_bc() & 0x0FFF)) > 0x0FFF); set_flag(FLAG_C, res > 0xFFFF); set_hl(res & 0xFFFF); return 8; }
        case 0x19: { uint32_t res = get_hl() + get_de(); set_flag(FLAG_N, false); set_flag(FLAG_H, ((get_hl() & 0x0FFF) + (get_de() & 0x0FFF)) > 0x0FFF); set_flag(FLAG_C, res > 0xFFFF); set_hl(res & 0xFFFF); return 8; }
        case 0x29: { uint32_t res = get_hl() + get_hl(); set_flag(FLAG_N, false); set_flag(FLAG_H, ((get_hl() & 0x0FFF) + (get_hl() & 0x0FFF)) > 0x0FFF); set_flag(FLAG_C, res > 0xFFFF); set_hl(res & 0xFFFF); return 8; }
        case 0x39: { uint32_t res = get_hl() + sp;       set_flag(FLAG_N, false); set_flag(FLAG_H, ((get_hl() & 0x0FFF) + (sp & 0x0FFF))       > 0x0FFF); set_flag(FLAG_C, res > 0xFFFF); set_hl(res & 0xFFFF); return 8; }

        case 0x07: { uint8_t bit7 = (a & 0x80) >> 7; a = (a << 1) | bit7; set_flag(FLAG_Z, false); set_flag(FLAG_N, false); set_flag(FLAG_H, false); set_flag(FLAG_C, bit7 == 1); return 4; }
        case 0x0F: { uint8_t bit0 = a & 0x01; a = (a >> 1) | (bit0 << 7); set_flag(FLAG_Z, false); set_flag(FLAG_N, false); set_flag(FLAG_H, false); set_flag(FLAG_C, bit0 == 1); return 4; }
        case 0x17: { uint8_t old_c = get_flag(FLAG_C) ? 1 : 0; uint8_t new_c = (a & 0x80) >> 7; a = (a << 1) | old_c; set_flag(FLAG_Z, false); set_flag(FLAG_N, false); set_flag(FLAG_H, false); set_flag(FLAG_C, new_c == 1); return 4; }
        case 0x1F: { uint8_t old_c = get_flag(FLAG_C) ? 1 : 0; uint8_t new_c = a & 0x01; a = (a >> 1) | (old_c << 7); set_flag(FLAG_Z, false); set_flag(FLAG_N, false); set_flag(FLAG_H, false); set_flag(FLAG_C, new_c == 1); return 4; }

        case 0xC5: push16(get_bc()); return 16;
        case 0xD5: push16(get_de()); return 16;
        case 0xE5: push16(get_hl()); return 16;
        case 0xF5: push16(get_af()); return 16;

        case 0xC1: set_bc(pop16()); return 12;
        case 0xD1: set_de(pop16()); return 12;
        case 0xE1: set_hl(pop16()); return 12;
        case 0xF1: set_af(pop16()); return 12;

        case 0x08: { uint16_t addr = fetch_word(); bus.write(addr, sp & 0xFF); bus.write(addr + 1, (sp >> 8) & 0xFF); return 20; }

        case 0x18: { int8_t offset = static_cast<int8_t>(fetch_byte()); pc += offset; return 12; }
        case 0x20: { int8_t offset = static_cast<int8_t>(fetch_byte()); if (!get_flag(FLAG_Z)) pc += offset; return 12; }
        case 0x28: { int8_t offset = static_cast<int8_t>(fetch_byte()); if (get_flag(FLAG_Z)) pc += offset; return 12; }
        case 0x30: { int8_t offset = static_cast<int8_t>(fetch_byte()); if (!get_flag(FLAG_C)) pc += offset; return 12; }
        case 0x38: { int8_t offset = static_cast<int8_t>(fetch_byte()); if (get_flag(FLAG_C)) pc += offset; return 12; }

        case 0xC3: pc = fetch_word(); return 16;
        case 0xE9: pc = get_hl(); return 4;

        case 0xCD: { uint16_t target = fetch_word(); push16(pc); pc = target; return 24; }
        case 0xC4: { uint16_t target = fetch_word(); if (!get_flag(FLAG_Z)) { push16(pc); pc = target; return 24; } return 12; }
        case 0xCC: { uint16_t target = fetch_word(); if (get_flag(FLAG_Z)) { push16(pc); pc = target; return 24; } return 12; }
        case 0xD4: { uint16_t target = fetch_word(); if (!get_flag(FLAG_C)) { push16(pc); pc = target; return 24; } return 12; }
        case 0xDC: { uint16_t target = fetch_word(); if (get_flag(FLAG_C)) { push16(pc); pc = target; return 24; } return 12; }

        case 0xC9: pc = pop16(); return 16;
        case 0xC0: if (!get_flag(FLAG_Z)) { pc = pop16(); return 20; } return 8;
        case 0xC8: if (get_flag(FLAG_Z))  { pc = pop16(); return 20; } return 8;
        case 0xD0: if (!get_flag(FLAG_C)) { pc = pop16(); return 20; } return 8;
        case 0xD8: if (get_flag(FLAG_C))  { pc = pop16(); return 20; } return 8;
        case 0xD9: pc = pop16(); ime = true; return 16;

        case 0xC7: push16(pc); pc = 0x0000; return 16;
        case 0xCF: push16(pc); pc = 0x0008; return 16;
        case 0xD7: push16(pc); pc = 0x0010; return 16;
        case 0xDF: push16(pc); pc = 0x0018; return 16;
        case 0xE7: push16(pc); pc = 0x0020; return 16;
        case 0xEF: push16(pc); pc = 0x0028; return 16;
        case 0xF7: push16(pc); pc = 0x0030; return 16;
        case 0xFF: push16(pc); pc = 0x0038; return 16;

        // --- CARRY FLAG OPS ---
        case 0x37: { set_flag(FLAG_N, false); set_flag(FLAG_H, false); set_flag(FLAG_C, true); return 4; }
        case 0x3F: { set_flag(FLAG_N, false); set_flag(FLAG_H, false); set_flag(FLAG_C, !get_flag(FLAG_C)); return 4; }

        // --- DAA ---
        case 0x27: {
            uint8_t correction = 0;
            bool set_c = false;

            if (get_flag(FLAG_H) || (!get_flag(FLAG_N) && (a & 0x0F) > 0x09)) {
                correction |= 0x06;
            }
            if (get_flag(FLAG_C) || (!get_flag(FLAG_N) && a > 0x99)) {
                correction |= 0x60;
                set_c = true;
            }

            a += get_flag(FLAG_N) ? -correction : correction;

            set_flag(FLAG_Z, a == 0);
            set_flag(FLAG_H, false);
            set_flag(FLAG_C, set_c || get_flag(FLAG_C));
            return 4;
        }

        // --- CPL ---
        case 0x2F: {
            a = ~a;
            set_flag(FLAG_N, true);
            set_flag(FLAG_H, true);
            return 4;
        }

        // --- STACK POINTER OPS ---
        case 0xF9: sp = get_hl(); return 8;

        case 0xE8: { // ADD SP, e8
            int8_t offset = static_cast<int8_t>(fetch_byte());
            uint8_t u_offset = static_cast<uint8_t>(offset);
            set_flag(FLAG_Z, false);
            set_flag(FLAG_N, false);
            set_flag(FLAG_H, ((sp & 0x0F) + (u_offset & 0x0F)) > 0x0F);
            set_flag(FLAG_C, ((sp & 0xFF) + (u_offset & 0xFF)) > 0xFF);
            sp = static_cast<uint16_t>(sp + offset);
            return 16;
        }

        case 0xF8: { // LD HL, SP+e8
            int8_t offset = static_cast<int8_t>(fetch_byte());
            uint8_t u_offset = static_cast<uint8_t>(offset);
            set_flag(FLAG_Z, false);
            set_flag(FLAG_N, false);
            set_flag(FLAG_H, ((sp & 0x0F) + (u_offset & 0x0F)) > 0x0F);
            set_flag(FLAG_C, ((sp & 0xFF) + (u_offset & 0xFF)) > 0xFF);
            set_hl(static_cast<uint16_t>(sp + offset));
            return 12;
        }

        // --- CONDITIONAL ABSOLUTE JUMPS ---
        case 0xC2: { uint16_t addr = fetch_word(); if (!get_flag(FLAG_Z)) { pc = addr; return 16; } return 12; }
        case 0xCA: { uint16_t addr = fetch_word(); if (get_flag(FLAG_Z))  { pc = addr; return 16; } return 12; }
        case 0xD2: { uint16_t addr = fetch_word(); if (!get_flag(FLAG_C)) { pc = addr; return 16; } return 12; }
        case 0xDA: { uint16_t addr = fetch_word(); if (get_flag(FLAG_C))  { pc = addr; return 16; } return 12; }

        case 0xCB: return execute_cb_opcode();
        
        // --- ILLEGAL OPCODES ---
        case 0xD3: case 0xDB: case 0xDD: case 0xE3: case 0xE4:
        case 0xEB: case 0xEC: case 0xED: case 0xF4: case 0xFC: case 0xFD:
            return 4;

        default:
            return 4;
    }
}

int CPU::execute_cb_opcode() {
    uint8_t cb = fetch_byte();
    uint8_t reg_idx = cb & 0x07;
    uint8_t bit_idx = (cb >> 3) & 0x07;

    auto get_reg = [&](uint8_t idx) -> uint8_t {
        switch (idx) {
            case 0: return b; case 1: return c; case 2: return d; case 3: return e;
            case 4: return h; case 5: return l; case 6: return bus.read(get_hl()); default: return a;
        }
    };

    auto set_reg = [&](uint8_t idx, uint8_t val) {
        switch (idx) {
            case 0: b = val; break; case 1: c = val; break; case 2: d = val; break; case 3: e = val; break;
            case 4: h = val; break; case 5: l = val; break; case 6: bus.write(get_hl(), val); break; default: a = val; break;
        }
    };

    uint8_t val = get_reg(reg_idx);

    if (cb <= 0x07) { // RLC r
        uint8_t bit7 = (val & 0x80) >> 7;
        uint8_t res = (val << 1) | bit7;
        set_flag(FLAG_Z, res == 0); set_flag(FLAG_N, false); set_flag(FLAG_H, false); set_flag(FLAG_C, bit7 == 1);
        set_reg(reg_idx, res);
        return (reg_idx == 6) ? 16 : 8;
    }
    else if (cb >= 0x08 && cb <= 0x0F) { // RRC r
        uint8_t bit0 = val & 0x01;
        uint8_t res = (val >> 1) | (bit0 << 7);
        set_flag(FLAG_Z, res == 0); set_flag(FLAG_N, false); set_flag(FLAG_H, false); set_flag(FLAG_C, bit0 == 1);
        set_reg(reg_idx, res);
        return (reg_idx == 6) ? 16 : 8;
    }
    else if (cb >= 0x10 && cb <= 0x17) { // RL r
        uint8_t old_c = get_flag(FLAG_C) ? 1 : 0;
        uint8_t new_c = (val & 0x80) >> 7;
        uint8_t res = (val << 1) | old_c;
        set_flag(FLAG_Z, res == 0); set_flag(FLAG_N, false); set_flag(FLAG_H, false); set_flag(FLAG_C, new_c == 1);
        set_reg(reg_idx, res);
        return (reg_idx == 6) ? 16 : 8;
    }
    else if (cb >= 0x18 && cb <= 0x1F) { // RR r
        uint8_t old_c = get_flag(FLAG_C) ? 1 : 0;
        uint8_t new_c = val & 0x01;
        uint8_t res = (val >> 1) | (old_c << 7);
        set_flag(FLAG_Z, res == 0); set_flag(FLAG_N, false); set_flag(FLAG_H, false); set_flag(FLAG_C, new_c == 1);
        set_reg(reg_idx, res);
        return (reg_idx == 6) ? 16 : 8;
    }
    else if (cb >= 0x20 && cb <= 0x27) { // SLA r
        uint8_t new_c = (val & 0x80) >> 7;
        uint8_t res = val << 1;
        set_flag(FLAG_Z, res == 0); set_flag(FLAG_N, false); set_flag(FLAG_H, false); set_flag(FLAG_C, new_c == 1);
        set_reg(reg_idx, res);
        return (reg_idx == 6) ? 16 : 8;
    }
    else if (cb >= 0x28 && cb <= 0x2F) { // SRA r
        uint8_t new_c = val & 0x01;
        uint8_t res = (val >> 1) | (val & 0x80); // Keep MSB unchanged
        set_flag(FLAG_Z, res == 0); set_flag(FLAG_N, false); set_flag(FLAG_H, false); set_flag(FLAG_C, new_c == 1);
        set_reg(reg_idx, res);
        return (reg_idx == 6) ? 16 : 8;
    }
    else if (cb >= 0x30 && cb <= 0x37) { // SWAP r
        uint8_t res = (val >> 4) | (val << 4);
        set_flag(FLAG_Z, res == 0); set_flag(FLAG_N, false); set_flag(FLAG_H, false); set_flag(FLAG_C, false);
        set_reg(reg_idx, res);
        return (reg_idx == 6) ? 16 : 8;
    }
    else if (cb >= 0x38 && cb <= 0x3F) { // SRL r
        uint8_t new_c = val & 0x01;
        uint8_t res = val >> 1;
        set_flag(FLAG_Z, res == 0); set_flag(FLAG_N, false); set_flag(FLAG_H, false); set_flag(FLAG_C, new_c == 1);
        set_reg(reg_idx, res);
        return (reg_idx == 6) ? 16 : 8;
    }
    else if (cb >= 0x40 && cb <= 0x7F) { // BIT n, r
        set_flag(FLAG_Z, (val & (1 << bit_idx)) == 0);
        set_flag(FLAG_N, false);
        set_flag(FLAG_H, true);
        return (reg_idx == 6) ? 12 : 8;
    } 
    else if (cb >= 0x80 && cb <= 0xBF) { // RES n, r
        set_reg(reg_idx, val & ~(1 << bit_idx));
        return (reg_idx == 6) ? 16 : 8;
    } 
    else if (cb >= 0xC0 && cb <= 0xFF) { // SET n, r
        set_reg(reg_idx, val | (1 << bit_idx));
        return (reg_idx == 6) ? 16 : 8;
    }

    return 8; 
}