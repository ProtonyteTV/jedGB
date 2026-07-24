#include <iostream>
#include <memory>
#include <vector>
#include <string>
#include <SDL2/SDL.h>

#include "jedgb_api.h"

constexpr int GB_SCREEN_WIDTH = 160;
constexpr int GB_SCREEN_HEIGHT = 144;
constexpr int DEFAULT_SCALE = 4;

int main(int argc, char* argv[]) {
    // 1. Initialize SDL Video and Audio
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) < 0) {
        std::cerr << "SDL Init Error: " << SDL_GetError() << "\n";
        return 1;
    }

    // 2. Create System Window & Renderer
    SDL_Window* window = SDL_CreateWindow(
        "jedGB Emulator",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        GB_SCREEN_WIDTH * DEFAULT_SCALE, GB_SCREEN_HEIGHT * DEFAULT_SCALE,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI
    );

    SDL_Renderer* renderer = SDL_CreateRenderer(
        window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );

    SDL_RenderSetLogicalSize(renderer, GB_SCREEN_WIDTH, GB_SCREEN_HEIGHT);

    SDL_Texture* texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_STREAMING,
        GB_SCREEN_WIDTH, GB_SCREEN_HEIGHT
    );

    // 3. Setup Audio Device
    SDL_AudioSpec wanted_spec, have_spec;
    SDL_zero(wanted_spec);
    wanted_spec.freq = 44100;          
    wanted_spec.format = AUDIO_S16SYS;  
    wanted_spec.channels = 2;          
    wanted_spec.samples = 2048;        
    wanted_spec.callback = nullptr;    

    SDL_AudioDeviceID audio_device = SDL_OpenAudioDevice(nullptr, 0, &wanted_spec, &have_spec, 0);
    if (audio_device > 0) {
        SDL_PauseAudioDevice(audio_device, 0); 
    }

    // 4. Create Emulator Instance via API
    JedGBContext* gb = jedgb_create();

    // 5. Load ROM: Check command-line arg first; if missing, trigger Native ROM Picker!
    bool rom_loaded = false;
    if (argc > 1) {
        rom_loaded = jedgb_load_rom(gb, argv[1]);
    }

    if (!rom_loaded) {
        std::cout << "No valid command line ROM provided. Opening ROM Picker dialog...\n";
        rom_loaded = jedgb_open_rom_picker_dialog(gb);
    }

    if (!rom_loaded) {
        std::cerr << "No ROM selected or failed to load. Exiting application.\n";
        jedgb_destroy(gb);
        if (audio_device > 0) SDL_CloseAudioDevice(audio_device);
        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 0;
    }

    // 6. Main Application Loop
    bool running = true;
    SDL_Event event;
    int16_t audio_buffer[4096];

    while (running) {
        // Process User Inputs
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }

            if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
                bool pressed = (event.type == SDL_KEYDOWN);

                // Option to press 'O' to open a new ROM mid-game
                if (pressed && event.key.keysym.sym == SDLK_o) {
                    jedgb_open_rom_picker_dialog(gb);
                }

                switch (event.key.keysym.sym) {
                    case SDLK_RIGHT:     jedgb_set_button(gb, 0, pressed); break; 
                    case SDLK_LEFT:      jedgb_set_button(gb, 1, pressed); break; 
                    case SDLK_UP:        jedgb_set_button(gb, 2, pressed); break; 
                    case SDLK_DOWN:      jedgb_set_button(gb, 3, pressed); break; 

                    case SDLK_z:         jedgb_set_button(gb, 4, pressed); break; 
                    case SDLK_x:         jedgb_set_button(gb, 5, pressed); break; 
                    case SDLK_BACKSPACE: jedgb_set_button(gb, 6, pressed); break; 
                    case SDLK_RETURN:    jedgb_set_button(gb, 7, pressed); break; 
                }
            }
        }

        // Step Emulation Frame (~59.7 FPS)
        jedgb_step_frame(gb);

        // Fetch & Stream Audio
        if (audio_device > 0) {
            size_t samples_read = jedgb_get_audio_samples(gb, audio_buffer, 4096);
            if (samples_read > 0) {
                if (SDL_GetQueuedAudioSize(audio_device) < 8192 * sizeof(int16_t)) {
                    SDL_QueueAudio(audio_device, audio_buffer, samples_read * sizeof(int16_t));
                }
            }
        }

        // Render Framebuffer
        const uint32_t* fb = jedgb_get_framebuffer(gb);
        if (fb) {
            SDL_UpdateTexture(texture, nullptr, fb, GB_SCREEN_WIDTH * sizeof(uint32_t));
            SDL_RenderClear(renderer);
            SDL_RenderCopy(renderer, texture, nullptr, nullptr);
            SDL_RenderPresent(renderer);
        }
    }

    // Cleanup
    jedgb_destroy(gb);
    if (audio_device > 0) SDL_CloseAudioDevice(audio_device);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}