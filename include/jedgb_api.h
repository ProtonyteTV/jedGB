#ifndef JEDGB_API_H
#define JEDGB_API_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct JedGBContext JedGBContext;

// Core Lifecycle
JedGBContext* jedgb_create(void);
void jedgb_destroy(JedGBContext* ctx);

// ROM Loading Functions
bool jedgb_load_rom(JedGBContext* ctx, const char* rom_path);
bool jedgb_load_rom_buffer(JedGBContext* ctx, const uint8_t* buffer, size_t size);

// Native Desktop Dialog Picker (Windows, macOS, Linux)
// Opens a file dialog window filtering for .gb / .gbc files.
// Returns true if a valid ROM was selected and loaded.
bool jedgb_open_rom_picker_dialog(JedGBContext* ctx);

// Execution Control
void jedgb_step_frame(JedGBContext* ctx);

// Graphics & Audio Buffers
const uint32_t* jedgb_get_framebuffer(JedGBContext* ctx);
size_t jedgb_get_audio_samples(JedGBContext* ctx, int16_t* out_buffer, size_t max_samples);

// Input Control
void jedgb_set_button(JedGBContext* ctx, uint8_t button_bit, bool pressed);

#ifdef __cplusplus
}
#endif

#endif // JEDGB_API_H