#pragma once
#include <cstdint>

class Bus;

constexpr uint8_t FLAG_Z = 1 << 7;
constexpr uint8_t FLAG_N = 1 << 6;
constexpr uint8_t FLAG_H = 1 << 5;
constexpr uint8_t FLAG_C = 1 << 4;

class CPU {
public:
    explicit CPU(Bus& bus);
    void reset();
    int step();

    uint8_t a{0}, f{0};
    uint8_t b{0}, c{0};
    uint8_t d{0}, e{0};
    uint8_t h{0}, l{0};

    uint16_t sp{0};
    uint16_t pc{0};

    bool ime{false};
    bool halt_mode{false};
    bool enabling_ime{false};
    bool halt_bug{false}; // Tracks the HALT hardware bug

    void set_flag(uint8_t flag, bool condition) {
        if (condition) f |= flag;
        else f &= ~flag;
        f &= 0xF0;
    }

    bool get_flag(uint8_t flag) const { 
        return (f & flag) != 0; 
    }

    uint16_t get_af() const { return (static_cast<uint16_t>(a) << 8) | (f & 0xF0); }
    void set_af(uint16_t val) { a = (val >> 8) & 0xFF; f = val & 0xF0; }

    uint16_t get_bc() const { return (static_cast<uint16_t>(b) << 8) | c; }
    void set_bc(uint16_t val) { b = (val >> 8) & 0xFF; c = val & 0xFF; }

    uint16_t get_de() const { return (static_cast<uint16_t>(d) << 8) | e; }
    void set_de(uint16_t val) { d = (val >> 8) & 0xFF; e = val & 0xFF; }

    uint16_t get_hl() const { return (static_cast<uint16_t>(h) << 8) | l; }
    void set_hl(uint16_t val) { h = (val >> 8) & 0xFF; l = val & 0xFF; }

    void push16(uint16_t val);
    uint16_t pop16();

private:
    Bus& bus;

    uint8_t fetch_byte();
    uint16_t fetch_word();

    void alu_add(uint8_t val);
    void alu_adc(uint8_t val);
    void alu_sub(uint8_t val);
    void alu_sbc(uint8_t val);
    void alu_and(uint8_t val);
    void alu_or(uint8_t val);
    void alu_xor(uint8_t val);
    void alu_cp(uint8_t val);
    uint8_t alu_inc(uint8_t val);
    uint8_t alu_dec(uint8_t val);

    int execute_cb_opcode();
    int handle_interrupts();
    int execute_opcode(uint8_t opcode);
};