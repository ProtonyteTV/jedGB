#pragma once
#include <cstdint>
#include <vector>
#include <array>

// --- Pulse Channels (Ch1 & Ch2) ---
struct SquareChannel {
    uint8_t sweep{0};       // NR10 (Ch1 only)
    uint8_t duty_length{0}; // NRx1
    uint8_t envelope{0};    // NRx2
    uint16_t frequency{0};  // NRx3 & NRx4
    uint8_t control{0};     // NRx4

    bool enabled{false};
    int timer{0};
    uint8_t duty_pos{0};
    int length_counter{0};
    bool length_enable{false};

    // Envelope runtime state
    uint8_t current_volume{0};
    int envelope_timer{0};
    uint8_t envelope_period{0};
    bool envelope_increase{false};

    const uint8_t duty_patterns[4][8] = {
        {0, 0, 0, 0, 0, 0, 0, 1}, // 12.5%
        {1, 0, 0, 0, 0, 0, 0, 1}, // 25%
        {1, 0, 0, 0, 0, 1, 1, 1}, // 50%
        {0, 1, 1, 1, 1, 1, 1, 0}  // 75%
    };

    void trigger() {
        enabled = true;
        timer = (2048 - frequency) * 4;

        if (length_counter == 0) length_counter = 64;

        current_volume = (envelope >> 4) & 0x0F;
        envelope_period = envelope & 0x07;
        envelope_timer = envelope_period;
        envelope_increase = (envelope & 0x08) != 0;

        if ((envelope & 0xF8) == 0) enabled = false; // DAC Off check
    }

    void tick_timer(int cycles) {
        if (!enabled) return;
        timer -= cycles;
        if (timer <= 0) {
            timer += (2048 - frequency) * 4;
            duty_pos = (duty_pos + 1) & 0x07;
        }
    }

    void tick_length() {
        if (length_enable && length_counter > 0) {
            length_counter--;
            if (length_counter == 0) enabled = false;
        }
    }

    void tick_envelope() {
        if (envelope_period == 0) return;
        if (envelope_timer > 0) envelope_timer--;
        if (envelope_timer == 0) {
            envelope_timer = envelope_period;
            if (envelope_increase && current_volume < 15) {
                current_volume++;
            } else if (!envelope_increase && current_volume > 0) {
                current_volume--;
            }
        }
    }

    uint8_t get_sample() const {
        if (!enabled) return 0;
        uint8_t duty = (duty_length >> 6) & 0x03;
        return duty_patterns[duty][duty_pos] ? current_volume : 0;
    }
};

// --- Wave Channel (Ch3 - Basslines & Custom Samples) ---
struct WaveChannel {
    bool dac_enabled{false}; // NR30 bit 7
    uint8_t length_raw{0};   // NR31
    uint8_t volume_shift{0}; // NR32 (bits 5-6: 0=0%, 1=100%, 2=50%, 3=25%)
    uint16_t frequency{0};   // NR33 & NR34
    uint8_t control{0};      // NR34

    bool enabled{false};
    int timer{0};
    uint8_t sample_pos{0};
    int length_counter{0};
    bool length_enable{false};

    std::array<uint8_t, 16> wave_ram{}; // 16 bytes (32 x 4-bit nibbles)

    void trigger() {
        enabled = dac_enabled;
        timer = (2048 - frequency) * 2;
        sample_pos = 0;
        if (length_counter == 0) length_counter = 256;
    }

    void tick_timer(int cycles) {
        if (!enabled) return;
        timer -= cycles;
        if (timer <= 0) {
            timer += (2048 - frequency) * 2;
            sample_pos = (sample_pos + 1) & 31;
        }
    }

    void tick_length() {
        if (length_enable && length_counter > 0) {
            length_counter--;
            if (length_counter == 0) enabled = false;
        }
    }

    uint8_t get_sample() const {
        if (!enabled || !dac_enabled || volume_shift == 0) return 0;

        uint8_t byte = wave_ram[sample_pos / 2];
        uint8_t nibble = (sample_pos % 2 == 0) ? (byte >> 4) : (byte & 0x0F);

        uint8_t shift = (volume_shift == 1) ? 0 : (volume_shift == 2) ? 1 : (volume_shift == 3) ? 2 : 4;
        return nibble >> shift;
    }
};

// --- Noise Channel (Ch4 - Drums, Percussion, SFX) ---
struct NoiseChannel {
    uint8_t length_raw{0};  // NR41
    uint8_t envelope{0};    // NR42
    uint8_t clock_spec{0};  // NR43
    uint8_t control{0};     // NR44

    bool enabled{false};
    int timer{0};
    uint16_t lfsr{0x7FFF};  // 15-bit shift register
    int length_counter{0};
    bool length_enable{false};

    uint8_t current_volume{0};
    int envelope_timer{0};
    uint8_t envelope_period{0};
    bool envelope_increase{false};

    static constexpr int divisors[8] = {8, 16, 32, 48, 64, 80, 96, 112};

    int get_period() const {
        int div = divisors[clock_spec & 0x07];
        int shift = (clock_spec >> 4) & 0x0F;
        return div << shift;
    }

    void trigger() {
        enabled = true;
        timer = get_period();
        lfsr = 0x7FFF;
        if (length_counter == 0) length_counter = 64;

        current_volume = (envelope >> 4) & 0x0F;
        envelope_period = envelope & 0x07;
        envelope_timer = envelope_period;
        envelope_increase = (envelope & 0x08) != 0;

        if ((envelope & 0xF8) == 0) enabled = false;
    }

    void tick_timer(int cycles) {
        if (!enabled) return;
        timer -= cycles;
        if (timer <= 0) {
            timer += get_period();

            uint8_t result = (lfsr & 1) ^ ((lfsr >> 1) & 1);
            lfsr >>= 1;
            lfsr |= (result << 14);

            if (clock_spec & 0x08) { // 7-bit mode
                lfsr &= ~(1 << 6);
                lfsr |= (result << 6);
            }
        }
    }

    void tick_length() {
        if (length_enable && length_counter > 0) {
            length_counter--;
            if (length_counter == 0) enabled = false;
        }
    }

    void tick_envelope() {
        if (envelope_period == 0) return;
        if (envelope_timer > 0) envelope_timer--;
        if (envelope_timer == 0) {
            envelope_timer = envelope_period;
            if (envelope_increase && current_volume < 15) {
                current_volume++;
            } else if (!envelope_increase && current_volume > 0) {
                current_volume--;
            }
        }
    }

    uint8_t get_sample() const {
        if (!enabled) return 0;
        return (lfsr & 1) == 0 ? current_volume : 0;
    }
};

class APU {
public:
    APU();

    void tick(int cycles);
    uint8_t read(uint16_t address) const;
    void write(uint16_t address, uint8_t value);

    const std::vector<int16_t>& get_sample_buffer() const { return sample_buffer; }
    void clear_sample_buffer() { sample_buffer.clear(); }

private:
    SquareChannel ch1;
    SquareChannel ch2;
    WaveChannel ch3;
    NoiseChannel ch4;

    uint8_t nr50{0x77};
    uint8_t nr51{0xFF};
    uint8_t nr52{0xF1};

    int sample_counter{0};
    int frame_sequencer_counter{0};
    uint8_t frame_sequencer_step{0};

    std::vector<int16_t> sample_buffer;

    double sample_cycles{0.0};
    static constexpr double CYCLES_PER_SAMPLE = 4194304.0 / 44100.0; // ~95.1089 T-cycles per sample

    int16_t prev_left{0};
    int16_t prev_right{0};

    // DC High-pass filter state
    double capacitor_l{0.0};
    double capacitor_r{0.0};

    void step_frame_sequencer();
};