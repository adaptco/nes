#include "stdafx.h"

#include "nes_cpu.h"
#include "nes_memory.h"
#include "nes_input.h"
#include "nes_system.h"
#include "nes_ppu.h"

using namespace std;

namespace
{
    static const uint32_t NES_STATE_MAGIC = 0x3153454e; // "NES1"
    static const uint32_t NES_STATE_VERSION = 1;

    template <typename T>
    void append_state(std::vector<uint8_t> &out, const T &value)
    {
        auto begin = reinterpret_cast<const uint8_t *>(&value);
        out.insert(out.end(), begin, begin + sizeof(T));
    }

    template <typename T>
    bool read_state(const std::vector<uint8_t> &in, size_t &offset, T *value)
    {
        if (offset + sizeof(T) > in.size())
            return false;

        memcpy(value, in.data() + offset, sizeof(T));
        offset += sizeof(T);
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

    append_state(state.data, NES_STATE_MAGIC);
    append_state(state.data, NES_STATE_VERSION);

    int64_t master_cycle = _master_cycle.count();
    append_state(state.data, master_cycle);
    append_state(state.data, _stop_requested);

    _cpu->serialize(state.data);
    _ram->serialize(state.data);
    _ppu->serialize(state.data);
    _input->serialize(state.data);

    return state;
}

bool nes_system::deserialize(const nes_state &state)
{
    size_t offset = 0;

    uint32_t magic;
    uint32_t version;
    int64_t master_cycle;
    bool stop_requested;

    bool ok =
        read_state(state.data, offset, &magic) &&
        read_state(state.data, offset, &version) &&
        read_state(state.data, offset, &master_cycle) &&
        read_state(state.data, offset, &stop_requested);

    if (!ok)
        return false;

    if (magic != NES_STATE_MAGIC || version != NES_STATE_VERSION)
        return false;

    if (!_cpu->deserialize(state.data, offset) ||
        !_ram->deserialize(state.data, offset) ||
        !_ppu->deserialize(state.data, offset) ||
        !_input->deserialize(state.data, offset))
    {
        return false;
    }

    if (offset != state.data.size())
        return false;

    _master_cycle = nes_cycle_t(master_cycle);
    _stop_requested = stop_requested;
    return true;
}

