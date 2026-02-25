#include "stdafx.h"

#include <nes_input.h>

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

// Make compiler happy about pure virtual dtors
nes_input_device::~nes_input_device()
{}

void nes_input::serialize(vector<uint8_t> &out) const
{
    write_value(out, uint8_t(_strobe_on ? 1 : 0));
    for (int i = 0; i < NES_MAX_PLAYER; ++i)
    {
        write_value(out, uint8_t(_button_flags[i]));
        write_value(out, _button_id[i]);
    }
}

bool nes_input::deserialize(const uint8_t *data, size_t size, size_t &offset)
{
    uint8_t value = 0;
    if (!read_value(data, size, offset, value))
        return false;

    _strobe_on = (value != 0);
    for (int i = 0; i < NES_MAX_PLAYER; ++i)
    {
        uint8_t button_flags = 0;
        if (!read_value(data, size, offset, button_flags))
            return false;
        _button_flags[i] = nes_button_flags(button_flags);

        if (!read_value(data, size, offset, _button_id[i]))
            return false;
    }

    return true;
}
