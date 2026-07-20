#include "apu.hpp"

APU::APU() {
    nr50 = 0x77;
    nr51 = 0xFF;
    nr52 = 0x80;
}

void APU::step_frame_sequencer() {
    // Length Counters (256 Hz - Steps 0, 2, 4, 6)
    if (frame_sequencer_step % 2 == 0) {
        ch1.tick_length();
        ch2.tick_length();
        ch3.tick_length();
        ch4.tick_length();
    }

    // Envelopes (64 Hz - Step 7)
    if (frame_sequencer_step == 7) {
        ch1.tick_envelope();
        ch2.tick_envelope();
        ch4.tick_envelope();
    }

    frame_sequencer_step = (frame_sequencer_step + 1) & 0x07;
}

void APU::tick(int cycles) {
    if (!(nr52 & 0x80)) return; // APU Off

    // 1. Tick Channel timers
    ch1.tick_timer(cycles);
    ch2.tick_timer(cycles);
    ch3.tick_timer(cycles);
    ch4.tick_timer(cycles);

    // 2. Tick Frame Sequencer at 512 Hz (Every 8192 T-Cycles)
    frame_sequencer_counter += cycles;
    if (frame_sequencer_counter >= 8192) {
        frame_sequencer_counter -= 8192;
        step_frame_sequencer();
    }

    // 3. Audio Sample Output Generation (~44.1 kHz)
    sample_counter += cycles;
    if (sample_counter >= 95) {
        sample_counter -= 95;

        uint8_t s1 = ch1.get_sample();
        uint8_t s2 = ch2.get_sample();
        uint8_t s3 = ch3.get_sample();
        uint8_t s4 = ch4.get_sample();

        int left_raw = 0;
        int right_raw = 0;

        // Stereo Routing (NR51)
        if (nr51 & 0x01) right_raw += s1;
        if (nr51 & 0x10) left_raw  += s1;

        if (nr51 & 0x02) right_raw += s2;
        if (nr51 & 0x20) left_raw  += s2;

        if (nr51 & 0x04) right_raw += s3;
        if (nr51 & 0x40) left_raw  += s3;

        if (nr51 & 0x08) right_raw += s4;
        if (nr51 & 0x80) left_raw  += s4;

        // Master Volume (NR50)
        uint8_t left_vol  = ((nr50 >> 4) & 0x07) + 1;
        uint8_t right_vol = (nr50 & 0x07) + 1;

        int16_t final_left  = static_cast<int16_t>(left_raw  * left_vol  * 50);
        int16_t final_right = static_cast<int16_t>(right_raw * right_vol * 50);

        sample_buffer.push_back(final_left);
        sample_buffer.push_back(final_right);
    }
}

uint8_t APU::read(uint16_t address) const {
    // Wave RAM Range (0xFF30 - 0xFF3F)
    if (address >= 0xFF30 && address <= 0xFF3F) {
        return ch3.wave_ram[address - 0xFF30];
    }

    switch (address) {
        // Channel 1
        case 0xFF10: return ch1.sweep | 0x80;
        case 0xFF11: return ch1.duty_length | 0x3F;
        case 0xFF12: return ch1.envelope;
        case 0xFF14: return ch1.control | 0xBF;

        // Channel 2
        case 0xFF16: return ch2.duty_length | 0x3F;
        case 0xFF17: return ch2.envelope;
        case 0xFF19: return ch2.control | 0xBF;

        // Channel 3
        case 0xFF1A: return (ch3.dac_enabled ? 0x80 : 0x00) | 0x7F;
        case 0xFF1C: return (ch3.volume_shift << 5) | 0x9F;
        case 0xFF1E: return ch3.control | 0xBF;

        // Channel 4
        case 0xFF21: return ch4.envelope;
        case 0xFF22: return ch4.clock_spec;
        case 0xFF23: return ch4.control | 0xBF;

        // Master Control
        case 0xFF24: return nr50;
        case 0xFF25: return nr51;
        case 0xFF26: {
            uint8_t status = nr52 & 0x80;
            if (ch1.enabled) status |= 0x01;
            if (ch2.enabled) status |= 0x02;
            if (ch3.enabled) status |= 0x04;
            if (ch4.enabled) status |= 0x08;
            return status | 0x70;
        }

        default:
            return 0xFF;
    }
}

void APU::write(uint16_t address, uint8_t value) {
    // Wave RAM writes (0xFF30 - 0xFF3F)
    if (address >= 0xFF30 && address <= 0xFF3F) {
        ch3.wave_ram[address - 0xFF30] = value;
        return;
    }

    if (!(nr52 & 0x80) && address != 0xFF26) return;

    switch (address) {
        // --- Channel 1 ---
        case 0xFF10: ch1.sweep = value; break;
        case 0xFF11: 
            ch1.duty_length = value; 
            ch1.length_counter = 64 - (value & 0x3F);
            break;
        case 0xFF12: ch1.envelope = value; break;
        case 0xFF13: ch1.frequency = (ch1.frequency & 0x0700) | value; break;
        case 0xFF14:
            ch1.frequency = (ch1.frequency & 0x00FF) | ((value & 0x07) << 8);
            ch1.control = value;
            ch1.length_enable = (value & 0x40) != 0;
            if (value & 0x80) ch1.trigger();
            break;

        // --- Channel 2 ---
        case 0xFF16: 
            ch2.duty_length = value; 
            ch2.length_counter = 64 - (value & 0x3F);
            break;
        case 0xFF17: ch2.envelope = value; break;
        case 0xFF18: ch2.frequency = (ch2.frequency & 0x0700) | value; break;
        case 0xFF19:
            ch2.frequency = (ch2.frequency & 0x00FF) | ((value & 0x07) << 8);
            ch2.control = value;
            ch2.length_enable = (value & 0x40) != 0;
            if (value & 0x80) ch2.trigger();
            break;

        // --- Channel 3 ---
        case 0xFF1A: 
            ch3.dac_enabled = (value & 0x80) != 0;
            if (!ch3.dac_enabled) ch3.enabled = false;
            break;
        case 0xFF1B: 
            ch3.length_raw = value;
            ch3.length_counter = 256 - value;
            break;
        case 0xFF1C: ch3.volume_shift = (value >> 5) & 0x03; break;
        case 0xFF1D: ch3.frequency = (ch3.frequency & 0x0700) | value; break;
        case 0xFF1E:
            ch3.frequency = (ch3.frequency & 0x00FF) | ((value & 0x07) << 8);
            ch3.control = value;
            ch3.length_enable = (value & 0x40) != 0;
            if (value & 0x80) ch3.trigger();
            break;

        // --- Channel 4 ---
        case 0xFF20: 
            ch4.length_raw = value; 
            ch4.length_counter = 64 - (value & 0x3F);
            break;
        case 0xFF21: ch4.envelope = value; break;
        case 0xFF22: ch4.clock_spec = value; break;
        case 0xFF23:
            ch4.control = value;
            ch4.length_enable = (value & 0x40) != 0;
            if (value & 0x80) ch4.trigger();
            break;

        // --- Master Control ---
        case 0xFF24: nr50 = value; break;
        case 0xFF25: nr51 = value; break;
        case 0xFF26:
            nr52 = (value & 0x80) | (nr52 & 0x0F);
            if (!(value & 0x80)) {
                ch1.enabled = false;
                ch2.enabled = false;
                ch3.enabled = false;
                ch4.enabled = false;
            }
            break;

        default:
            break;
    }
}