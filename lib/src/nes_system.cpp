#include "stdafx.h"
#include <cstring>

#include "nes_cpu.h"
#include "nes_system.h"
#include "nes_ppu.h"

using namespace std;

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
    
namespace {
constexpr uint32_t NES_STATE_MAGIC = 0x3153454E; // 'NES1'
constexpr uint32_t NES_STATE_VERSION = 1;

template<typename T>
void nes_state_write_sys(vector<uint8_t> &out, const T &v)
{
    const uint8_t *p = reinterpret_cast<const uint8_t*>(&v);
    out.insert(out.end(), p, p + sizeof(T));
}

template<typename T>
bool nes_state_read_sys(const vector<uint8_t> &in, size_t &offset, T &v)
{
    if (offset + sizeof(T) > in.size())
        return false;
    memcpy(&v, in.data() + offset, sizeof(T));
    offset += sizeof(T);
    return true;
}
}

nes_state_blob nes_system::serialize() const
{
    nes_state_blob state;
    auto &out = state.bytes;

    nes_state_write_sys(out, NES_STATE_MAGIC);
    nes_state_write_sys(out, NES_STATE_VERSION);

    auto master_cycle = _master_cycle.count();
    nes_state_write_sys(out, master_cycle);
    nes_state_write_sys(out, _stop_requested);

    _cpu->serialize(out);
    _ram->serialize(out);
    _ppu->serialize(out);
    _input->serialize(out);

    bool has_mapper = _ram->has_mapper();
    nes_state_write_sys(out, has_mapper);
    if (has_mapper)
        _ram->get_mapper().serialize(out);

    return state;
}

bool nes_system::deserialize(const nes_state_blob &state)
{
    size_t offset = 0;
    const auto &in = state.bytes;

    uint32_t magic = 0;
    uint32_t version = 0;
    int64_t master_cycle = 0;

    if (!nes_state_read_sys(in, offset, magic) ||
        !nes_state_read_sys(in, offset, version) ||
        magic != NES_STATE_MAGIC ||
        version != NES_STATE_VERSION ||
        !nes_state_read_sys(in, offset, master_cycle) ||
        !nes_state_read_sys(in, offset, _stop_requested))
    {
        return false;
    }

    if (!_cpu->deserialize(in, offset) ||
        !_ram->deserialize(in, offset) ||
        !_ppu->deserialize(in, offset) ||
        !_input->deserialize(in, offset))
    {
        return false;
    }

    bool has_mapper = false;
    if (!nes_state_read_sys(in, offset, has_mapper))
        return false;

    if (has_mapper)
    {
        if (!_ram->has_mapper())
            return false;
        if (!_ram->get_mapper().deserialize(in, offset))
            return false;
    }

    if (offset != in.size())
        return false;

    _master_cycle = nes_cycle_t(master_cycle);
    return true;
}
