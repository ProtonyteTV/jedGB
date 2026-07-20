// cartridge.hpp
#pragma once
#include <string>
#include <vector>
#include <cstdint>

class Cartridge {
public:
    Cartridge() = default;
    ~Cartridge(); // Automatically save on exit

    bool load_from_file(const std::string& filename);
    void save_ram_to_file();
    void load_ram_from_file();

    uint8_t read(uint16_t address) const;
    void write(uint16_t address, uint8_t value);

    const std::string& get_title() const { return title; }
    bool is_loaded() const { return loaded; }

private:
    std::vector<uint8_t> rom_data;
    std::vector<uint8_t> ram_data; // 32KB max for MBC3 (4 banks x 8KB)
    std::string title;
    std::string save_filename;

    uint8_t cartridge_type{0};
    bool loaded{false};

    // MBC Registers
    uint8_t rom_bank{1};      
    uint8_t ram_bank{0};      
    bool ram_enabled{false};
    uint8_t banking_mode{0};  
};