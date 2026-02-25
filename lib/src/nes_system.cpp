#include "stdafx.h"

#include "nes_cpu.h"
#include "nes_system.h"
#include "nes_ppu.h"

namespace
{
    static const uint32_t NES_STATE_MAGIC = 0x3153454E; // NES1
    static const uint16_t NES_STATE_VERSION = 1;

    template<typename T>
    void write_value(vector<uint8_t> &out, T value)
    {
        for (size_t i = 0; i < sizeof(T); ++i)
            out.push_back(uint8_t((uint64_t(value) >> (i * 8)) & 0xff));
    }

    template<typename T>
    bool read_value(const uint8_t *data, size_t size, size_t &offset, T &value)
    {
        if (offset + sizeof(T) > size)
            return false;

        uint64_t v = 0;
        for (size_t i = 0; i < sizeof(T); ++i)
            v |= uint64_t(data[offset++]) << (i * 8);

        value = T(v);
        return true;
    }
}

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

nes_state_blob nes_system::serialize() const
{
    nes_state_blob state;

    write_value(state.bytes, NES_STATE_MAGIC);
    write_value(state.bytes, NES_STATE_VERSION);
    write_value(state.bytes, _master_cycle.count());
    write_value(state.bytes, uint8_t(_stop_requested ? 1 : 0));

    _cpu->serialize(state.bytes);
    _ram->serialize(state.bytes);
    _ppu->serialize(state.bytes);
    _input->serialize(state.bytes);

    auto &mapper = _ram->get_mapper();
    write_value(state.bytes, mapper.mapper_id());

    vector<uint8_t> mapper_state;
    mapper.serialize(mapper_state);
    write_value(state.bytes, uint32_t(mapper_state.size()));
    state.bytes.insert(state.bytes.end(), mapper_state.begin(), mapper_state.end());

    return state;
}

bool nes_system::deserialize(const nes_state_blob &state)
{
    size_t offset = 0;
    const auto *data = state.bytes.data();
    size_t size = state.bytes.size();

    uint32_t magic = 0;
    uint16_t version = 0;
    int64_t master_cycle = 0;
    uint8_t stop_requested = 0;
    uint16_t mapper_id = 0;
    uint32_t mapper_size = 0;

    if (!read_value(data, size, offset, magic) || magic != NES_STATE_MAGIC)
        return false;
    if (!read_value(data, size, offset, version) || version != NES_STATE_VERSION)
        return false;
    if (!read_value(data, size, offset, master_cycle))
        return false;
    if (!read_value(data, size, offset, stop_requested))
        return false;

    if (!_cpu->deserialize(data, size, offset))
        return false;
    if (!_ram->deserialize(data, size, offset))
        return false;
    if (!_ppu->deserialize(data, size, offset))
        return false;
    if (!_input->deserialize(data, size, offset))
        return false;

    if (!read_value(data, size, offset, mapper_id))
        return false;
    if (!read_value(data, size, offset, mapper_size))
        return false;
    if (offset + mapper_size > size)
        return false;

    auto &mapper = _ram->get_mapper();
    if (mapper_id != mapper.mapper_id())
        return false;

    size_t mapper_offset = offset;
    if (!mapper.deserialize(data, offset + mapper_size, mapper_offset))
        return false;
    if (mapper_offset != offset + mapper_size)
        return false;

    _master_cycle = nes_cycle_t(master_cycle);
    _stop_requested = (stop_requested != 0);
    return true;
}
    
