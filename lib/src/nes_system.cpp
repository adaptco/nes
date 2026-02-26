#include "stdafx.h"

#include "nes_cpu.h"
#include "nes_system.h"
#include "nes_ppu.h"

using namespace std;


namespace
{
    static const uint32_t NES_STATE_MAGIC = 0x3153454e; // NES1
    static const uint32_t NES_STATE_VERSION = 2;

    void push_u32(vector<uint8_t> &out, uint32_t v)
    {
        for (int i = 0; i < 4; ++i)
            out.push_back(uint8_t((v >> (i * 8)) & 0xff));
    }

    void push_u64(vector<uint8_t> &out, uint64_t v)
    {
        for (int i = 0; i < 8; ++i)
            out.push_back(uint8_t((v >> (i * 8)) & 0xff));
    }

    bool read_u32(const uint8_t *data, size_t size, size_t &offset, uint32_t &v)
    {
        if (offset + 4 > size)
            return false;

        v = 0;
        for (int i = 0; i < 4; ++i)
            v |= (uint32_t(data[offset++]) << (i * 8));
        return true;
    }

    bool read_u64(const uint8_t *data, size_t size, size_t &offset, uint64_t &v)
    {
        if (offset + 8 > size)
            return false;

        v = 0;
        for (int i = 0; i < 8; ++i)
            v |= (uint64_t(data[offset++]) << (i * 8));
        return true;
    }
}

nes_system::nes_system()
{
    _ram = make_unique<nes_memory>();
    _cpu = make_unique<nes_cpu>();
    _ppu = make_unique<nes_ppu>();
    _input = make_unique<nes_input>();

    _components.push_back(_ram.get());
    _components.push_back(_cpu.get());
    _components.push_back(_ppu.get());
    _components.push_back(_input.get());
}
                         
nes_system::~nes_system() {}

void nes_system::init()
{
    _stop_requested = false;
    _master_cycle = nes_cycle_t(0);
}

void nes_system::power_on()
{
    init();

    for (auto comp : _components)
        comp->power_on(this);
}

void nes_system::reset()
{
    init();

    for (auto comp : _components)
        comp->reset();
}


nes_system_snapshot nes_system::snapshot() const
{
    nes_system_snapshot s{};

    s.frame_buffer = _ppu->frame_buffer();
    s.frame_width = _ppu->frame_width();
    s.frame_height = _ppu->frame_height();

    s.cpu_ram = _ram->ram_data();
    s.cpu_ram_size = _ram->ram_size();

    s.ppu_vram = _ppu->vram();
    s.ppu_vram_size = _ppu->vram_size();

    s.ppu_oam = _ppu->oam();
    s.ppu_oam_size = _ppu->oam_size();

    return s;
}



nes_state_blob nes_system::serialize() const
{
    nes_state_blob blob;

    push_u32(blob.data, NES_STATE_MAGIC);
    push_u32(blob.data, NES_STATE_VERSION);
    push_u64(blob.data, uint64_t(_master_cycle.count()));
    blob.data.push_back(_stop_requested ? 1 : 0);

    _cpu->serialize(blob.data);
    _ram->serialize(blob.data);
    _ppu->serialize(blob.data);
    _input->serialize(blob.data);

    return blob;
}

bool nes_system::deserialize(const nes_state_blob &state)
{
    const uint8_t *data = state.data.data();
    size_t size = state.data.size();
    size_t offset = 0;

    uint32_t magic = 0;
    uint32_t version = 0;
    uint64_t master_cycle = 0;
    if (!read_u32(data, size, offset, magic) ||
        !read_u32(data, size, offset, version) ||
        !read_u64(data, size, offset, master_cycle))
    {
        return false;
    }

    if (magic != NES_STATE_MAGIC || version != NES_STATE_VERSION || offset >= size)
        return false;

    _master_cycle = nes_cycle_t((int64_t)master_cycle);
    _stop_requested = data[offset++] != 0;

    if (!_cpu->deserialize(data, size, offset))
        return false;
    if (!_ram->deserialize(data, size, offset))
        return false;
    if (!_ppu->deserialize(data, size, offset))
        return false;
    if (!_input->deserialize(data, size, offset))
        return false;

    return offset == size;
}

void nes_system::run_program(vector<uint8_t> &&program, uint16_t addr)
{
    _ram->set_bytes(addr, program.data(), program.size());
    _cpu->PC() = addr;

    test_loop();
}

void nes_system::load_rom(const char *rom_path, nes_rom_exec_mode mode)
{
    auto mapper = nes_rom_loader::load_from(rom_path);
    _ram->load_mapper(mapper);
    _ppu->load_mapper(mapper);

    if (mode == nes_rom_exec_mode_direct)
    {
        nes_mapper_info info;
        mapper->get_info(info);
        _cpu->PC() = info.code_addr;
    }
    else 
    {
        assert(mode == nes_rom_exec_mode_reset);
        _cpu->PC() = ram()->get_word(RESET_HANDLER);
    }
}

void nes_system::run_rom(const char *rom_path, nes_rom_exec_mode mode)
{
    load_rom(rom_path, mode);

    test_loop();
}

void nes_system::test_loop()
{
    auto tick = nes_cycle_t(1);
    while (!_stop_requested)
    {
        step(tick);
    }
}

void nes_system::step(nes_cycle_t count)
{
    _master_cycle += count;

    // Manually step the individual components instead of all components
    // This saves a loop and also it's kinda stupid to step components that doesn't require stepping in the
    // first place. Such as ram / controller, etc. 
    _cpu->step_to(_master_cycle);
    _ppu->step_to(_master_cycle);
}
    
