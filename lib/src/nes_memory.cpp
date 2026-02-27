#include "stdafx.h"
#include <cstring>

namespace
{
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

void nes_memory::power_on(nes_system *system)
{
    memset(&_ram[0], 0, RAM_SIZE);
    _system = system;
    _ppu = _system->ppu();
    _input = _system->input();
}

uint8_t nes_memory::read_io_reg(uint16_t addr)
{
    switch (addr)
    {
    case 0x2002: return _ppu->read_PPUSTATUS();
    case 0x2004: return _ppu->read_OAMDATA();
    case 0x2007: return _ppu->read_PPUDATA();
    case 0x4016: return _input->read_CONTROLLER(0);
    case 0x4017: return _input->read_CONTROLLER(1);
    }

    return _ppu->read_latch();
}

void nes_memory::write_io_reg(uint16_t addr, uint8_t val)
{
    switch (addr)
    {
    case 0x2000: _ppu->write_PPUCTRL(val); return;
    case 0x2001: _ppu->write_PPUMASK(val); return;
    case 0x2003: _ppu->write_OAMADDR(val); return;
    case 0x2004: _ppu->write_OAMDATA(val); return;
    case 0x2005: _ppu->write_PPUSCROLL(val); return;
    case 0x2006: _ppu->write_PPUADDR(val); return;
    case 0x2007: _ppu->write_PPUDATA(val); return;
    case 0x4014: _ppu->write_OAMDMA(val); return;
    case 0x4016: _input->write_CONTROLLER(val); return;
    case 0x4017: _input->write_CONTROLLER(val); return;
    }

    _ppu->write_latch(val);
}

void nes_memory::load_mapper(shared_ptr<nes_mapper> &mapper)
{
    // unset previous mapper
    _mapper = nullptr;

    // Give mapper a chance to copy all the bytes needed
    mapper->on_load_ram(*this);

    _mapper = mapper;
    _mapper->get_info(_mapper_info);
}

void nes_memory::set_byte(uint16_t addr, uint8_t val)
{
    redirect_addr(addr);
    if (is_io_reg(addr))
    {
        write_io_reg(addr, val);
        return;
    }

    if (_mapper && (_mapper_info.flags & nes_mapper_flags_has_registers))
    {
        if (addr >= _mapper_info.reg_start && addr <= _mapper_info.reg_end)
        {
            _mapper->write_reg(addr, val);
            return;
        }
    }

    _ram[addr] = val;
}

void nes_memory::serialize(vector<uint8_t> &out) const
{
    out.insert(out.end(), _ram.begin(), _ram.end());

    uint8_t has_mapper = _mapper ? 1 : 0;
    out.push_back(has_mapper);
    if (!_mapper)
        return;

    uint16_t mapper_id = _mapper->mapper_id();
    out.push_back(uint8_t(mapper_id & 0xff));
    out.push_back(uint8_t((mapper_id >> 8) & 0xff));

    vector<uint8_t> mapper_state;
    _mapper->serialize(mapper_state);
    uint32_t mapper_size = uint32_t(mapper_state.size());
    for (int i = 0; i < 4; ++i)
        out.push_back(uint8_t((mapper_size >> (i * 8)) & 0xff));

    out.insert(out.end(), mapper_state.begin(), mapper_state.end());
}

bool nes_memory::deserialize(const uint8_t *data, size_t size, size_t &offset)
{
    if (offset + RAM_SIZE > size)
        return false;

    memcpy_s(_ram.data(), _ram.size(), data + offset, RAM_SIZE);
    offset += RAM_SIZE;

    if (offset >= size)
        return false;

    bool has_mapper_state = data[offset++] != 0;
    if (!has_mapper_state)
        return true;

    if (!_mapper || offset + 6 > size)
        return false;

    uint16_t mapper_id = uint16_t(data[offset]) | (uint16_t(data[offset + 1]) << 8);
    offset += 2;
    if (mapper_id != _mapper->mapper_id())
        return false;

    uint32_t mapper_size = 0;
    for (int i = 0; i < 4; ++i)
        mapper_size |= uint32_t(data[offset++]) << (i * 8);

    if (offset + mapper_size > size)
        return false;

    size_t mapper_offset = offset;
    bool ok = _mapper->deserialize(data, offset + mapper_size, mapper_offset);
    if (!ok || mapper_offset != offset + mapper_size)
        return false;

    offset += mapper_size;

    return true;
}
