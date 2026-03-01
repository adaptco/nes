#pragma once

#include <cstdint>
#include <cstddef>
#include <nes_trace.h>
#include <memory>
#include <vector>
#include <cstring>
#include <fstream>
#include <common.h>

using namespace std;

enum nes_mapper_flags : uint16_t
{
    nes_mapper_flags_none = 0,
    nes_mapper_flags_mirroring_mask = 0x3,
    nes_mapper_flags_vertical_mirroring = 0x2,
    nes_mapper_flags_horizontal_mirroring = 0x3,
    nes_mapper_flags_one_screen_upper_bank = 0x1,
    nes_mapper_flags_one_screen_lower_bank = 0x0,
    nes_mapper_flags_has_registers = 0x4,
};

struct nes_mapper_info
{
    uint16_t code_addr;
    uint16_t reg_start;
    uint16_t reg_end;
    nes_mapper_flags flags;
};

class nes_ppu;
class nes_cpu;
class nes_memory;

class nes_mapper
{
public:
    virtual void serialize(vector<uint8_t> &out) const {}
    virtual bool deserialize(const uint8_t *data, size_t size, size_t &offset) { return true; }
    virtual uint16_t mapper_id() const = 0;

    virtual void on_load_ram(nes_memory &mem) {}
    virtual void on_load_ppu(nes_ppu &ppu) {}
    virtual void get_info(nes_mapper_info &) = 0;
    virtual void write_reg(uint16_t addr, uint8_t val) {}

    virtual ~nes_mapper() {}
};

class nes_mapper_nrom : public nes_mapper
{
public:
    nes_mapper_nrom(shared_ptr<vector<uint8_t>> &prg_rom, shared_ptr<vector<uint8_t>> &chr_rom, bool vertical_mirroring)
        : _prg_rom(prg_rom), _chr_rom(chr_rom), _vertical_mirroring(vertical_mirroring) {}

    virtual void on_load_ram(nes_memory &mem);
    virtual void on_load_ppu(nes_ppu &ppu);
    virtual void get_info(nes_mapper_info &info);
    virtual uint16_t mapper_id() const { return 0; }

private:
    shared_ptr<vector<uint8_t>> _prg_rom;
    shared_ptr<vector<uint8_t>> _chr_rom;
    bool _vertical_mirroring;
};

class nes_mapper_mmc1 : public nes_mapper
{
public:
    nes_mapper_mmc1(shared_ptr<vector<uint8_t>> &prg_rom, shared_ptr<vector<uint8_t>> &chr_rom, bool vertical_mirroring)
        : _prg_rom(prg_rom), _chr_rom(chr_rom), _vertical_mirroring(vertical_mirroring)
    {
        _bit_latch = 0;
        _reg = 0;
        _control = 0x0c;
        _chr_bank_0 = 0;
        _chr_bank_1 = 0;
        _prg_bank = 0;
    }

    virtual void on_load_ram(nes_memory &mem);
    virtual void on_load_ppu(nes_ppu &ppu);
    virtual void get_info(nes_mapper_info &info);
    virtual void serialize(vector<uint8_t> &out) const;
    virtual bool deserialize(const uint8_t *data, size_t size, size_t &offset);
    virtual uint16_t mapper_id() const { return 1; }

    virtual void write_reg(uint16_t addr, uint8_t val);

private:
    void write_control(uint8_t val);
    void write_chr_bank_0(uint8_t val);
    void write_chr_bank_1(uint8_t val);
    void write_prg_bank(uint8_t val);

private:
    nes_ppu *_ppu;
    nes_memory *_mem;
    shared_ptr<vector<uint8_t>> _prg_rom;
    shared_ptr<vector<uint8_t>> _chr_rom;
    bool _vertical_mirroring;

    uint8_t _bit_latch;
    uint8_t _reg;
    uint8_t _control;
    uint8_t _chr_bank_0;
    uint8_t _chr_bank_1;
    uint8_t _prg_bank;
};

class nes_mapper_mmc3 : public nes_mapper
{
public:
    nes_mapper_mmc3(shared_ptr<vector<uint8_t>> &prg_rom, shared_ptr<vector<uint8_t>> &chr_rom, bool vertical_mirroring)
        : _prg_rom(prg_rom), _chr_rom(chr_rom), _vertical_mirroring(vertical_mirroring)
    {
        _prev_prg_mode = 1;
        _bank_select = 0;
        memset(_bank_data, 0, sizeof(_bank_data));
    }

    virtual void on_load_ram(nes_memory &mem);
    virtual void on_load_ppu(nes_ppu &ppu);
    virtual void get_info(nes_mapper_info &info);
    virtual void serialize(vector<uint8_t> &out) const;
    virtual bool deserialize(const uint8_t *data, size_t size, size_t &offset);
    virtual uint16_t mapper_id() const { return 4; }

    virtual void write_reg(uint16_t addr, uint8_t val);

private:
    void write_bank_select(uint8_t val);
    void write_bank_data(uint8_t val);
    void write_mirroring(uint8_t val);
    void write_prg_ram_protect(uint8_t val) { assert(false); }
    void write_irq_latch(uint8_t val) { assert(false); }
    void write_irq_reload(uint8_t val) { assert(false); }
    void write_irq_disable(uint8_t val) { }
    void write_irq_enable(uint8_t val) { assert(false); }

private:
    nes_ppu *_ppu;
    nes_memory *_mem;
    shared_ptr<vector<uint8_t>> _prg_rom;
    shared_ptr<vector<uint8_t>> _chr_rom;
    bool _vertical_mirroring;

    uint8_t _bank_select;
    uint8_t _prev_prg_mode;
    uint8_t _bank_data[8];
};

#define FLAG_6_USE_VERTICAL_MIRRORING_MASK 0x1
#define FLAG_6_HAS_BATTERY_BACKED_PRG_RAM_MASK 0x2
#define FLAG_6_HAS_TRAINER_MASK  0x4
#define FLAG_6_USE_FOUR_SCREEN_VRAM_MASK 0x8
#define FLAG_6_LO_MAPPER_NUMBER_MASK 0xf0
#define FLAG_7_HI_MAPPER_NUMBER_MASK 0xf0

class nes_rom_loader
{
public:
    struct ines_header
    {
        uint8_t magic[4];
        uint8_t prg_size;
        uint8_t chr_size;
        uint8_t flag6;
        uint8_t flag7;
        uint8_t prg_ram_size;
        uint8_t flag9;
        uint8_t flag10;
        uint8_t reserved[5];
    };

    static shared_ptr<nes_mapper> load_from(const char *path)
    {
        NES_TRACE1("[NES_ROM] Opening NES ROM file '" << path << "' ...");
        assert(sizeof(ines_header) == 0x10);

        ifstream file;
        file.exceptions(std::ifstream::failbit | std::ifstream::badbit);
        file.open(path, std::ifstream::in | std::ifstream::binary);

        ines_header header;
        file.read((char *)&header, sizeof(header));

        if (header.flag6 & FLAG_6_HAS_TRAINER_MASK)
            file.seekg(0x200, ios_base::cur);

        bool vertical_mirroring = header.flag6 & FLAG_6_USE_VERTICAL_MIRRORING_MASK;
        if (header.flag7 == 0x44)
            header.flag7 = 0;

        int mapper_id = ((header.flag6 & FLAG_6_LO_MAPPER_NUMBER_MASK) >> 4) + ((header.flag7 & FLAG_7_HI_MAPPER_NUMBER_MASK));
        int prg_rom_size = header.prg_size * 0x4000;
        int chr_rom_size = header.chr_size * 0x2000;

        auto prg_rom = make_shared<vector<uint8_t>>(prg_rom_size);
        auto chr_rom = make_shared<vector<uint8_t>>(chr_rom_size);
        file.read((char *)prg_rom->data(), prg_rom->size());
        file.read((char *)chr_rom->data(), chr_rom->size());

        shared_ptr<nes_mapper> mapper;
        switch (mapper_id)
        {
        case 0: mapper = make_shared<nes_mapper_nrom>(prg_rom, chr_rom, vertical_mirroring); break;
        case 1: mapper = make_shared<nes_mapper_mmc1>(prg_rom, chr_rom, vertical_mirroring); break;
        case 4: mapper = make_shared<nes_mapper_mmc3>(prg_rom, chr_rom, vertical_mirroring); break;
        default: assert(!"Unsupported mapper id");
        }

        file.close();
        return mapper;
    }
};
