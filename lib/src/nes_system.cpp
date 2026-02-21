#include "stdafx.h"

#include "nes_cpu.h"
#include "nes_system.h"
#include "nes_ppu.h"

#include <array>

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
    

namespace
{
    constexpr uint32_t NES_STATE_MAGIC = 0x53454E43; // "CNES"
    constexpr uint16_t NES_STATE_VERSION = 1;
}

nes_state nes_system::serialize() const
{
    nes_state state;
    nes_state_stream stream(state.blob);

    stream.write(NES_STATE_MAGIC);
    stream.write(NES_STATE_VERSION);

    auto master_cycle = _master_cycle.count();
    stream.write(master_cycle);

    uint8_t stop_requested = _stop_requested ? 1 : 0;
    stream.write(stop_requested);

    _cpu->serialize(stream);
    _ram->serialize(stream);
    _ppu->serialize(stream);
    _input->serialize(stream);

    _ram->get_mapper().serialize(stream);

    return state;
}

bool nes_system::deserialize(const nes_state &state)
{
    auto state_copy = state.blob;
    nes_state_stream stream(state_copy);

    uint32_t magic = 0;
    uint16_t version = 0;

    if (!stream.read(magic)) return false;
    if (!stream.read(version)) return false;

    if (magic != NES_STATE_MAGIC)
        return false;

    if (version != NES_STATE_VERSION)
        return false;

    int64_t master_cycle = 0;
    if (!stream.read(master_cycle)) return false;

    uint8_t stop_requested = 0;
    if (!stream.read(stop_requested)) return false;

    if (!_cpu->deserialize(stream)) return false;
    if (!_ram->deserialize(stream)) return false;
    if (!_ppu->deserialize(stream)) return false;
    if (!_input->deserialize(stream)) return false;

    if (!_ram->get_mapper().deserialize(stream)) return false;

    if (!stream.ok())
        return false;

    if (stream.offset() != state_copy.size())
        return false;

    _master_cycle = nes_cycle_t(master_cycle);
    _stop_requested = (stop_requested != 0);

    return true;
}
