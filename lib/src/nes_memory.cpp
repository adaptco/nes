#include "stdafx.h"

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
namespace
{
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

void nes_memory::serialize(vector<uint8_t> &out) const
{
    out.insert(out.end(), _ram.begin(), _ram.end());

    append_state(out, _mapper_info.code_addr);
    append_state(out, _mapper_info.reg_start);
    append_state(out, _mapper_info.reg_end);
    append_state(out, _mapper_info.flags);

    bool has_mapper = (_mapper != nullptr);
    append_state(out, has_mapper);
    if (has_mapper)
        _mapper->serialize(out);
}

bool nes_memory::deserialize(const vector<uint8_t> &in, size_t &offset)
{
    if (offset + RAM_SIZE > in.size())
        return false;

    memcpy(_ram.data(), in.data() + offset, RAM_SIZE);
    offset += RAM_SIZE;

    bool ok =
        read_state(in, offset, &_mapper_info.code_addr) &&
        read_state(in, offset, &_mapper_info.reg_start) &&
        read_state(in, offset, &_mapper_info.reg_end) &&
        read_state(in, offset, &_mapper_info.flags);

    bool has_mapper;
    ok = ok && read_state(in, offset, &has_mapper);
    if (!ok)
        return false;

    if (has_mapper)
    {
        if (!_mapper)
            return false;
        return _mapper->deserialize(in, offset);
    }

    return true;
}
