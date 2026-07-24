#pragma once
#include <string>
#include <vector>
#include <cstdint>

class Cartridge {
public:
    Cartridge() = default;
    ~Cartridge(); 

    bool load_from_file(const std::string& filename);
    bool load_from_buffer(const uint8_t* buffer, size_t size); // New buffer loader
    void save_ram_to_file();
    void load_ram_from_file();

    uint8_t read(uint16_t address) const;
    void write(uint16_t address, uint8_t value);

    const std::string& get_title() const { return title; }
    bool is_loaded() const { return loaded; }

private:
    std::vector<uint8_t> rom_data;
    std::vector<uint8_t> ram_data; 
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