#include "ppu.hpp"
#include "bus.hpp"

PPU::PPU(Bus& bus) : bus(bus) {
    reset();
}

void PPU::reset() {
    frame_buffer.fill(0xFFFFFFFF);
}

uint32_t PPU::get_color_from_palette(uint8_t color_idx, uint8_t palette) {
    uint8_t shade = (palette >> (color_idx * 2)) & 0x03;
    switch (shade) {
        case 0: return 0xE0F8D0FF; // White / Light Green
        case 1: return 0x88C070FF; // Light Gray
        case 2: return 0x346856FF; // Dark Gray
        case 3: return 0x081820FF; // Black / Dark Green
    }
    return 0xFFFFFFFF;
}

void PPU::render_scanline(uint8_t ly) {
    uint8_t lcdc = bus.read(0xFF40);

    if (!(lcdc & 0x80)) { // Display Disabled
        for (int x = 0; x < 160; ++x) {
            frame_buffer[ly * 160 + x] = 0xFFFFFFFF;
        }
        return;
    }

    uint8_t bgp = bus.read(0xFF47);

    // -------------------------------------------------------------------------
    // 1. Background Layer
    // -------------------------------------------------------------------------
    if (lcdc & 0x01) { // BG Display Enable
        uint8_t scy = bus.read(0xFF42);
        uint8_t scx = bus.read(0xFF43);
        uint16_t map_base = (lcdc & 0x08) ? 0x9C00 : 0x9800;
        bool unsigned_addressing = (lcdc & 0x10) != 0;

        uint8_t y_pos = ly + scy;
        uint8_t tile_row = y_pos / 8;

        for (int x = 0; x < 160; ++x) {
            uint8_t x_pos = x + scx;
            uint8_t tile_col = x_pos / 8;

            uint16_t tile_map_addr = map_base + (tile_row * 32) + tile_col;
            uint8_t tile_num = bus.read(tile_map_addr);

            uint16_t tile_data_addr;
            if (unsigned_addressing) {
                tile_data_addr = 0x8000 + (tile_num * 16);
            } else {
                int8_t signed_tile_num = static_cast<int8_t>(tile_num);
                tile_data_addr = 0x9000 + (signed_tile_num * 16);
            }

            uint8_t line_in_tile = y_pos % 8;
            uint8_t byte1 = bus.read(tile_data_addr + (line_in_tile * 2));
            uint8_t byte2 = bus.read(tile_data_addr + (line_in_tile * 2) + 1);

            int bit_idx = 7 - (x_pos % 8);
            uint8_t color_idx = (((byte2 >> bit_idx) & 1) << 1) | ((byte1 >> bit_idx) & 1);

            frame_buffer[ly * 160 + x] = get_color_from_palette(color_idx, bgp);
        }
    } else {
        uint32_t bg_color_0 = get_color_from_palette(0, bgp);
        for (int x = 0; x < 160; ++x) {
            frame_buffer[ly * 160 + x] = bg_color_0;
        }
    }

   // -------------------------------------------------------------------------
    // 2. Window Layer
    // -------------------------------------------------------------------------
    bool window_enable = (lcdc & 0x20) != 0;
    uint8_t wy = bus.read(0xFF4A);
    int16_t wx = static_cast<int16_t>(bus.read(0xFF4B)) - 7;

    // Check if the window is visible on this scanline
    if (window_enable && ly >= wy && wx < 160) {
        uint16_t win_map_base = (lcdc & 0x40) ? 0x9C00 : 0x9800;
        bool unsigned_addressing = (lcdc & 0x10) != 0;

        uint8_t win_y = window_line; // Use the internal counter, NOT (ly - wy)
        uint8_t tile_row = win_y / 8;

        for (int x = 0; x < 160; ++x) {
            if (x < wx) continue; // Skip pixels left of Window start

            uint8_t win_x = x - wx;
            uint8_t tile_col = win_x / 8;

            uint16_t tile_map_addr = win_map_base + (tile_row * 32) + tile_col;
            uint8_t tile_num = bus.read(tile_map_addr);

            uint16_t tile_data_addr;
            if (unsigned_addressing) {
                tile_data_addr = 0x8000 + (tile_num * 16);
            } else {
                int8_t signed_tile_num = static_cast<int8_t>(tile_num);
                tile_data_addr = 0x9000 + (signed_tile_num * 16);
            }

            uint8_t line_in_tile = win_y % 8;
            uint8_t byte1 = bus.read(tile_data_addr + (line_in_tile * 2));
            uint8_t byte2 = bus.read(tile_data_addr + (line_in_tile * 2) + 1);

            int bit_idx = 7 - (win_x % 8);
            uint8_t color_idx = (((byte2 >> bit_idx) & 1) << 1) | ((byte1 >> bit_idx) & 1);

            frame_buffer[ly * 160 + x] = get_color_from_palette(color_idx, bgp);
        }
        
        window_line++; // Increment counter ONLY when the window actually draws a line
    }

    // -------------------------------------------------------------------------
    // 3. Sprite (OBJ) Layer
    // -------------------------------------------------------------------------
    render_sprites(ly);
}

void PPU::render_sprites(uint8_t ly) {
    uint8_t lcdc = bus.read(0xFF40);
    if (!(lcdc & 0x02)) return; // Sprites disabled

    bool sprite_size_16 = (lcdc & 0x04) != 0;
    uint8_t sprite_height = sprite_size_16 ? 16 : 8;

    uint8_t obp0 = bus.read(0xFF48);
    uint8_t obp1 = bus.read(0xFF49);

    int sprites_drawn = 0;

    for (int i = 0; i < 40; ++i) {
        uint16_t oam_addr = 0xFE00 + (i * 4);

        int16_t y_pos = static_cast<int16_t>(bus.read(oam_addr)) - 16;
        int16_t x_pos = static_cast<int16_t>(bus.read(oam_addr + 1)) - 8;
        uint8_t tile_idx = bus.read(oam_addr + 2);
        uint8_t attributes = bus.read(oam_addr + 3);

        if (ly < y_pos || ly >= y_pos + sprite_height) continue;

        sprites_drawn++;
        if (sprites_drawn > 10) break;

        bool priority = (attributes & 0x80) != 0;
        bool y_flip   = (attributes & 0x40) != 0;
        bool x_flip   = (attributes & 0x20) != 0;
        uint8_t palette = (attributes & 0x10) ? obp1 : obp0;

        if (sprite_size_16) tile_idx &= 0xFE;

        uint8_t line = ly - y_pos;
        if (y_flip) line = (sprite_height - 1) - line;

        uint16_t tile_data_addr = 0x8000 + (tile_idx * 16) + (line * 2);
        uint8_t byte1 = bus.read(tile_data_addr);
        uint8_t byte2 = bus.read(tile_data_addr + 1);

        for (int x = 0; x < 8; ++x) {
            int pixel_x = x_pos + x;
            if (pixel_x < 0 || pixel_x >= 160) continue;

            int bit_idx = x_flip ? x : (7 - x);
            uint8_t color_idx = (((byte2 >> bit_idx) & 1) << 1) | ((byte1 >> bit_idx) & 1);

            if (color_idx == 0) continue; // Color index 0 is transparent on sprites

            // If priority bit is set, sprite only displays over BG color index 0
            if (priority) {
                uint32_t current_pixel = frame_buffer[ly * 160 + pixel_x];
                uint32_t bgp_color_0 = get_color_from_palette(0, bus.read(0xFF47));
                if (current_pixel != bgp_color_0) continue;
            }

            frame_buffer[ly * 160 + pixel_x] = get_color_from_palette(color_idx, palette);
        }
    }
}

void PPU::tick(int cycles) {
    uint8_t current_ly = bus.read(0xFF44);

    // Track line changes so we only render the scanline ONCE per line
    if (current_ly != last_ly) {
        if (current_ly == 0) {
            window_line = 0; // Reset the window internal counter at frame start
        }

        if (last_ly < 144) {
            render_scanline(last_ly);
        }
        last_ly = current_ly;
    }
}