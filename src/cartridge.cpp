// cartridge.cpp
#include "cartridge.hpp"
#include <fstream>
#include <iostream>

Cartridge::~Cartridge() {
    save_ram_to_file(); // Save on emulator close
}

bool Cartridge::load_from_file(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return false;

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    rom_data.resize(size);
    if (!file.read(reinterpret_cast<char*>(rom_data.data()), size)) return false;

    // Build .sav filename (e.g., "pokemon.gb" -> "pokemon.sav")
    size_t last_dot = filename.find_last_of(".");
    if (last_dot != std::string::npos) {
        save_filename = filename.substr(0, last_dot) + ".sav";
    } else {
        save_filename = filename + ".sav";
    }

    if (rom_data.size() > 0x0147) {
        cartridge_type = rom_data[0x0147];
    }

    // Allocate 32KB for Save RAM (4 banks of 8KB)
    ram_data.resize(0x8000, 0x00);
    load_ram_from_file();

    loaded = true;
    rom_bank = 1;
    return true;
}

void Cartridge::load_ram_from_file() {
    std::ifstream save_file(save_filename, std::ios::binary);
    if (save_file.is_open()) {
        save_file.read(reinterpret_cast<char*>(ram_data.data()), ram_data.size());
        std::cout << "Loaded save data from: " << save_filename << "\n";
    }
}

void Cartridge::save_ram_to_file() {
    if (save_filename.empty() || ram_data.empty()) return;

    std::ofstream save_file(save_filename, std::ios::binary);
    if (save_file.is_open()) {
        save_file.write(reinterpret_cast<const char*>(ram_data.data()), ram_data.size());
        std::cout << "Saved game data to: " << save_filename << "\n";
    }
}

uint8_t Cartridge::read(uint16_t address) const {
    if (rom_data.empty()) return 0xFF;

    // 0x0000 - 0x3FFF: Fixed ROM Bank 0
    if (address < 0x4000) {
        return rom_data[address];
    }
    // 0x4000 - 0x7FFF: Switchable ROM Bank
    else if (address < 0x8000) {
        uint32_t target_addr = (static_cast<uint32_t>(rom_bank) * 0x4000) + (address & 0x3FFF);
        if (target_addr < rom_data.size()) {
            return rom_data[target_addr];
        }
    }
    // 0xA000 - 0xBFFF: Switchable RAM Bank (or RTC Registers)
    else if (address >= 0xA000 && address <= 0xBFFF) {
        if (!ram_enabled) return 0xFF;

        // RAM Banks 0x00–0x03
        if (ram_bank <= 0x03) {
            uint32_t target_addr = (static_cast<uint32_t>(ram_bank) * 0x2000) + (address & 0x1FFF);
            if (target_addr < ram_data.size()) {
                return ram_data[target_addr];
            }
        }
        // RTC Registers (0x08–0x0C) - Stubbed returning 0 for now
        else if (ram_bank >= 0x08 && ram_bank <= 0x0C) {
            return 0x00; 
        }
    }
    return 0xFF;
}

void Cartridge::write(uint16_t address, uint8_t value) {
    // 0x0000 - 0x1FFF: RAM Enable
    if (address < 0x2000) {
        bool previous_state = ram_enabled;
        ram_enabled = ((value & 0x0F) == 0x0A);

        // Save automatically whenever the game disables RAM access (e.g., done writing save data)
        if (previous_state && !ram_enabled) {
            save_ram_to_file();
        }
    } 
    // 0x2000 - 0x3FFF: ROM Bank Selector (7 bits for MBC3)
    else if (address < 0x4000) {
        uint8_t bank = value & 0x7F; // 7-bit register
        if (bank == 0) bank = 1;      // Bank 0 maps to Bank 1
        rom_bank = bank;
    } 
    // 0x4000 - 0x5FFF: RAM Bank / RTC Register Select
    else if (address < 0x6000) {
        ram_bank = value;
    }
    // 0x6000 - 0x7FFF: RTC Latch Clock Data (Ignored for basic saves)
    else if (address < 0x8000) {
        // RTC latching code goes here if needed later
    }
    // 0xA000 - 0xBFFF: Write to External RAM / RTC
    else if (address >= 0xA000 && address <= 0xBFFF) {
        if (!ram_enabled) return;

        if (ram_bank <= 0x03) {
            uint32_t target_addr = (static_cast<uint32_t>(ram_bank) * 0x2000) + (address & 0x1FFF);
            if (target_addr < ram_data.size()) {
                ram_data[target_addr] = value;
            }
        }
    }
}