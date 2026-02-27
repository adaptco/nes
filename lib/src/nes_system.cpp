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
constexpr uint32_t NES_STATE_MAGIC = 0x54534E45; // ENST
constexpr uint32_t NES_STATE_VERSION = 1;

template <typename T>
void sys_append_value(vector<uint8_t> &out, const T &value)
{
    const auto *ptr = reinterpret_cast<const uint8_t *>(&value);
    out.insert(out.end(), ptr, ptr + sizeof(T));
}

template <typename T>
bool sys_read_value(const uint8_t *&cursor, const uint8_t *end, T &value)
{
    if (cursor + sizeof(T) > end)
        return false;
    memcpy(&value, cursor, sizeof(T));
    cursor += sizeof(T);
    return true;
}
}

nes_system::nes_state nes_system::serialize() const
{
    nes_state state;
    sys_append_value(state.bytes, NES_STATE_MAGIC);
    sys_append_value(state.bytes, NES_STATE_VERSION);
    sys_append_value(state.bytes, _master_cycle);
    sys_append_value(state.bytes, _stop_requested);

    _cpu->serialize(state.bytes);
    _ram->serialize(state.bytes);
    _ppu->serialize(state.bytes);
    if (_ram)
        _ram->get_mapper().serialize(state.bytes);
    _input->serialize(state.bytes);

    return state;
}

bool nes_system::deserialize(const nes_state &state)
{
    const uint8_t *cursor = state.bytes.data();
    const uint8_t *end = cursor + state.bytes.size();

    uint32_t magic = 0;
    uint32_t version = 0;
    if (!sys_read_value(cursor, end, magic) || !sys_read_value(cursor, end, version))
        return false;
    if (magic != NES_STATE_MAGIC || version != NES_STATE_VERSION)
        return false;

    if (!sys_read_value(cursor, end, _master_cycle) || !sys_read_value(cursor, end, _stop_requested))
        return false;

    if (!_cpu->deserialize(cursor, end))
        return false;
    if (!_ram->deserialize(cursor, end))
        return false;
    if (!_ppu->deserialize(cursor, end))
        return false;
    if (_ram && !_ram->get_mapper().deserialize(cursor, end))
        return false;
    if (!_input->deserialize(cursor, end))
        return false;

    return cursor == end;
}
