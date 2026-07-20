#pragma once
#include <cstdint>
#include <array>

class Bus;

class PPU {
public:
    explicit PPU(Bus& bus);
    void reset();
    void tick(int cycles);

    // 160x144 RGBA pixel array (0xRRGGBBAA)
    std::array<uint32_t, 160 * 144> frame_buffer{};

private:
    Bus& bus;

    void render_scanline(uint8_t ly);
    void render_sprites(uint8_t ly);
    uint8_t last_ly{0xFF};
    uint8_t window_line{0};
    uint32_t get_color_from_palette(uint8_t color_idx, uint8_t palette);
};