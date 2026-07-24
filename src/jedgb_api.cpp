#include "jedgb_api.h"
#include "bus.hpp"
#include "cpu.hpp"
#include "ppu.hpp"
#include "apu.hpp"
#include "cartridge.hpp"

#include <memory>
#include <vector>
#include <algorithm>

// Include tinyfiledialogs or native open file dialog library
#if defined(_WIN32) || defined(__APPLE__) || defined(__linux__)
#include "tinyfiledialogs.h"
#endif

struct JedGBContext {
    std::shared_ptr<Cartridge> cartridge;
    Bus bus;
    CPU cpu;
    PPU ppu;
    APU apu;

    JedGBContext() : cartridge(std::make_shared<Cartridge>()), cpu(bus), ppu(bus) {
        bus.insert_cartridge(cartridge);
        bus.connect_apu(&apu);
    }
};

JedGBContext* jedgb_create(void) {
    return new JedGBContext();
}

void jedgb_destroy(JedGBContext* ctx) {
    if (ctx) delete ctx;
}

bool jedgb_load_rom(JedGBContext* ctx, const char* rom_path) {
    if (!ctx || !rom_path) return false;
    return ctx->cartridge->load_from_file(rom_path);
}

bool jedgb_load_rom_buffer(JedGBContext* ctx, const uint8_t* buffer, size_t size) {
    if (!ctx || !buffer) return false;
    return ctx->cartridge->load_from_buffer(buffer, size);
}

bool jedgb_open_rom_picker_dialog(JedGBContext* ctx) {
    if (!ctx) return false;

#if defined(_WIN32) || defined(__APPLE__) || defined(__linux__)
    const char* filter_patterns[2] = { "*.gb", "*.gbc" };
    const char* selection = tinyfd_openFileDialog(
        "Select Game Boy ROM",
        "",
        2,
        filter_patterns,
        "Game Boy ROM Files (*.gb, *.gbc)",
        0
    );

    if (selection) {
        return ctx->cartridge->load_from_file(selection);
    }
#endif

    return false;
}

void jedgb_step_frame(JedGBContext* ctx) {
    if (!ctx) return;

    constexpr int CYCLES_PER_FRAME = 70224;
    int cycles_this_frame = 0;

    while (cycles_this_frame < CYCLES_PER_FRAME) {
        int cycles = ctx->cpu.step();
        ctx->bus.tick(cycles);
        ctx->ppu.tick(cycles);
        ctx->apu.tick(cycles);
        cycles_this_frame += cycles;
    }
}

const uint32_t* jedgb_get_framebuffer(JedGBContext* ctx) {
    if (!ctx) return nullptr;
    return ctx->ppu.frame_buffer.data();
}

size_t jedgb_get_audio_samples(JedGBContext* ctx, int16_t* out_buffer, size_t max_samples) {
    if (!ctx || !out_buffer) return 0;

    const auto& samples = ctx->apu.get_sample_buffer();
    size_t count = std::min(samples.size(), max_samples);

    for (size_t i = 0; i < count; ++i) {
        out_buffer[i] = samples[i];
    }

    ctx->apu.clear_sample_buffer();
    return count;
}

void jedgb_set_button(JedGBContext* ctx, uint8_t button_bit, bool pressed) {
    if (!ctx) return;
    ctx->bus.set_button_state(button_bit, pressed);
}