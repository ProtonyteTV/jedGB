#include <iostream>
#include <memory>
#include <vector>
#include <SDL2/SDL.h>
#include "bus.hpp"
#include "cpu.hpp"
#include "ppu.hpp"
#include "apu.hpp"
#include "cartridge.hpp"

constexpr int GB_SCREEN_WIDTH = 160;
constexpr int GB_SCREEN_HEIGHT = 144;
constexpr int SCALE = 4;
constexpr int CYCLES_PER_FRAME = 70224; // Standard Game Boy cycles per frame (~59.7 FPS)

int main(int argc, char* argv[]) {
    auto cartridge = std::make_shared<Cartridge>();

    if (argc > 1) {
        if (!cartridge->load_from_file(argv[1])) {
            std::cerr << "Failed to load ROM: " << argv[1] << "\n";
            return 1;
        }
    } else {
        std::cout << "Usage: jedGB.exe <path_to_rom.gb>\n";
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        std::cerr << "SDL Init Error: " << SDL_GetError() << "\n";
        return 1;
    }

    // Window & Rendering Setup
    SDL_Window* window = SDL_CreateWindow(
        "jedGB",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        GB_SCREEN_WIDTH * SCALE, GB_SCREEN_HEIGHT * SCALE,
        SDL_WINDOW_SHOWN
    );

    SDL_Renderer* renderer = SDL_CreateRenderer(
        window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );

    SDL_Texture* texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_STREAMING,
        GB_SCREEN_WIDTH, GB_SCREEN_HEIGHT
    );

    // Audio Setup
    SDL_AudioSpec wanted_spec, have_spec;
    SDL_zero(wanted_spec);
    wanted_spec.freq = 44100;          // 44.1 kHz sample rate
    wanted_spec.format = AUDIO_S16SYS;  // Signed 16-bit PCM
    wanted_spec.channels = 2;          // Stereo (Left/Right)
    wanted_spec.samples = 2048;        // Increased buffer size to eliminate crackling
    wanted_spec.callback = nullptr;    // Manual queuing mode

    SDL_AudioDeviceID audio_device = SDL_OpenAudioDevice(nullptr, 0, &wanted_spec, &have_spec, 0);
    if (audio_device == 0) {
        std::cerr << "Failed to open audio device: " << SDL_GetError() << "\n";
    } else {
        SDL_PauseAudioDevice(audio_device, 0); // Start playing audio stream
    }

    // Core Components Setup
    Bus bus;
    bus.insert_cartridge(cartridge);
    CPU cpu(bus);
    PPU ppu(bus);
    APU apu;
    bus.connect_apu(&apu);

    bool running = true;
    SDL_Event event;

    while (running) {
        // Handle User Inputs
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }

            if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
                bool pressed = (event.type == SDL_KEYDOWN);

                switch (event.key.keysym.sym) {
                    // Direction Keys (Bits 0-3)
                    case SDLK_RIGHT:     bus.set_button_state(0, pressed); break;
                    case SDLK_LEFT:      bus.set_button_state(1, pressed); break;
                    case SDLK_UP:        bus.set_button_state(2, pressed); break;
                    case SDLK_DOWN:      bus.set_button_state(3, pressed); break;

                    // Action Buttons (Bits 4-7)
                    case SDLK_z:         bus.set_button_state(4, pressed); break; // A
                    case SDLK_x:         bus.set_button_state(5, pressed); break; // B
                    case SDLK_BACKSPACE: bus.set_button_state(6, pressed); break; // Select
                    case SDLK_RETURN:    bus.set_button_state(7, pressed); break; // Start
                }
            }
        }

        // Emulation Frame Step
        int cycles_this_frame = 0;
        while (cycles_this_frame < CYCLES_PER_FRAME) {
            int cycles = cpu.step();
            bus.tick(cycles);
            ppu.tick(cycles);
            apu.tick(cycles);
            cycles_this_frame += cycles;
        }

        // Stream Audio Samples to SDL
        if (audio_device != 0) {
            const auto& samples = apu.get_sample_buffer();
            if (!samples.empty()) {
                // Ensure audio buffer depth stays smoothly populated (~20-80ms of sound)
                // Prevents buffer underruns without dropping frames or causing clicks
                if (SDL_GetQueuedAudioSize(audio_device) < 8192 * sizeof(int16_t)) {
                    SDL_QueueAudio(audio_device, samples.data(), samples.size() * sizeof(int16_t));
                }
                apu.clear_sample_buffer();
            }
        }

        // Render Frame Buffer to Screen
        SDL_UpdateTexture(texture, nullptr, ppu.frame_buffer.data(), GB_SCREEN_WIDTH * sizeof(uint32_t));
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, nullptr, nullptr);
        SDL_RenderPresent(renderer);
    }

    // Cleanup
    if (audio_device != 0) {
        SDL_CloseAudioDevice(audio_device);
    }
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}