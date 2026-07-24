#include "cartridge.hpp"
#include <fstream>
#include <iostream>

Cartridge::~Cartridge() {
    save_ram_to_file(); 
}

bool Cartridge::load_from_file(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return false;

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    rom_data.resize(size);
    if (!file.read(reinterpret_cast<char*>(rom_data.data()), size)) return false;

    // Build .sav filename
    size_t last_dot = filename.find_last_of(".");
    if (last_dot != std::string::npos) {
        save_filename = filename.substr(0, last_dot) + ".sav";
    } else {
        save_filename = filename + ".sav";
    }

    if (rom_data.size() > 0x0147) {
        cartridge_type = rom_data[0x0147];
    }

    ram_data.resize(0x8000, 0x00);
    load_ram_from_file();

    loaded = true;
    rom_bank = 1;
    return true;
}

// Loads ROM directly from byte buffer (for Mobile/Android/iOS native pickers)
bool Cartridge::load_from_buffer(const uint8_t* buffer, size_t size) {
    if (!buffer || size == 0) return false;

    rom_data.assign(buffer, buffer + size);

    if (rom_data.size() > 0x0147) {
        cartridge_type = rom_data[0x0147];
    }

    ram_data.resize(0x8000, 0x00);
    save_filename = "savegame.sav"; // Default save filename for buffer loads

    loaded = true;
    rom_bank = 1;
    return true;
}

void Cartridge::load_ram_from_file() {
    if (save_filename.empty()) return;
    std::ifstream save_file(save_filename, std::ios::binary);
    if (save_file.is_open()) {
        save_file.read(reinterpret_cast<char*>(ram_data.data()), ram_data.size());
    }
}

void Cartridge::save_ram_to_file() {
    if (save_filename.empty() || ram_data.empty()) return;
    std::ofstream save_file(save_filename, std::ios::binary);
    if (save_file.is_open()) {
        save_file.write(reinterpret_cast<const char*>(ram_data.data()), ram_data.size());
    }
}

uint8_t Cartridge::read(uint16_t address) const {
    if (rom_data.empty()) return 0xFF;

    if (address < 0x4000) {
        return rom_data[address];
    } else if (address < 0x8000) {
        uint32_t target_addr = (static_cast<uint32_t>(rom_bank) * 0x4000) + (address & 0x3FFF);
        if (target_addr < rom_data.size()) return rom_data[target_addr];
    } else if (address >= 0xA000 && address <= 0xBFFF) {
        if (!ram_enabled) return 0xFF;
        if (ram_bank <= 0x03) {
            uint32_t target_addr = (static_cast<uint32_t>(ram_bank) * 0x2000) + (address & 0x1FFF);
            if (target_addr < ram_data.size()) return ram_data[target_addr];
        }
    }
    return 0xFF;
}

void Cartridge::write(uint16_t address, uint8_t value) {
    if (address < 0x2000) {
        bool previous_state = ram_enabled;
        ram_enabled = ((value & 0x0F) == 0x0A);
        if (previous_state && !ram_enabled) save_ram_to_file();
    } else if (address < 0x4000) {
        uint8_t bank = value & 0x7F;
        if (bank == 0) bank = 1;
        rom_bank = bank;
    } else if (address < 0x6000) {
        ram_bank = value;
    } else if (address >= 0xA000 && address <= 0xBFFF) {
        if (!ram_enabled) return;
        if (ram_bank <= 0x03) {
            uint32_t target_addr = (static_cast<uint32_t>(ram_bank) * 0x2000) + (address & 0x1FFF);
            if (target_addr < ram_data.size()) ram_data[target_addr] = value;
        }
    }
}