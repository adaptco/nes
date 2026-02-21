#include "stdafx.h"
#include <cstring>

#include "nes_cpu.h"
#include "nes_system.h"
#include "nes_ppu.h"
#include "nes_memory.h"
#include "nes_input.h"

using namespace std;


namespace
{
    constexpr uint32_t NES_STATE_MAGIC = 0x53454E43; // "CNES"
    constexpr uint32_t NES_STATE_VERSION = 1;

    template<typename T>
    void write_scalar(vector<uint8_t> &out, const T &value)
    {
        const uint8_t *p = reinterpret_cast<const uint8_t *>(&value);
        out.insert(out.end(), p, p + sizeof(T));
    }

    template<typename T>
    bool read_scalar(const uint8_t *&ptr, const uint8_t *end, T &value)
    {
        if (end - ptr < (ptrdiff_t)sizeof(T))
            return false;
        memcpy(&value, ptr, sizeof(T));
        ptr += sizeof(T);
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
    

nes_system::nes_state nes_system::serialize() const
{
    nes_state state;

    write_scalar(state.bytes, NES_STATE_MAGIC);
    write_scalar(state.bytes, NES_STATE_VERSION);

    int64_t master_cycle = _master_cycle.count();
    write_scalar(state.bytes, master_cycle);
    write_scalar(state.bytes, _stop_requested);

    _cpu->serialize(state.bytes);
    _ram->serialize(state.bytes);
    _ppu->serialize(state.bytes);

    if (_ram && _ram->has_mapper())
    {
        _ram->get_mapper().serialize(state.bytes);
    }

    _input->serialize(state.bytes);

    return state;
}

bool nes_system::deserialize(const nes_state &state)
{
    const uint8_t *ptr = state.bytes.data();
    const uint8_t *end = ptr + state.bytes.size();

    uint32_t magic;
    uint32_t version;
    if (!read_scalar(ptr, end, magic) || !read_scalar(ptr, end, version))
        return false;

    if (magic != NES_STATE_MAGIC || version != NES_STATE_VERSION)
        return false;

    int64_t master_cycle;
    if (!read_scalar(ptr, end, master_cycle) || !read_scalar(ptr, end, _stop_requested))
        return false;
    _master_cycle = nes_cycle_t(master_cycle);

    if (!_cpu->deserialize(ptr, end) ||
        !_ram->deserialize(ptr, end) ||
        !_ppu->deserialize(ptr, end))
    {
        return false;
    }

    if (_ram && _ram->has_mapper())
    {
        if (!_ram->get_mapper().deserialize(ptr, end))
            return false;
    }

    if (!_input->deserialize(ptr, end))
        return false;

    return ptr == end;
}
