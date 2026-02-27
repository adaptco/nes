#include "stdafx.h"
#include <cstring>

#include <nes_input.h>
#include <nes_mapper.h>


// Make compiler happy about pure virtual dtors
nes_input_device::~nes_input_device()
{}


void nes_input::init()
{
    _strobe_on = false;
    for (int i = 0; i < NES_MAX_PLAYER; ++i)
    {
        _button_flags[i] = nes_button_flags_none;
        _button_id[i] = 0;
    }
}

void nes_input::reload()
{
    for (int i = 0; i < NES_MAX_PLAYER; ++i)
    {
        auto user_input = _user_inputs[i];
        if (user_input)
            _button_flags[i] = user_input->poll_status();
        else
            _button_flags[i] = nes_button_flags_none;

        _button_id[i] = 0;
    }
}

void nes_input::write_CONTROLLER(uint8_t val)
{
    bool prev_strobe = _strobe_on;
    _strobe_on = (val & NES_CONTROLLER_STROBE_BIT) != 0;
    if (prev_strobe && !_strobe_on)
        reload();
}

uint8_t nes_input::read_CONTROLLER(uint8_t id)
{
    if (_strobe_on)
        reload();

    return 0x40 | ((_button_flags[id] >> (7 - _button_id[id]++)) & 0x1);
}

void nes_input::serialize(vector<uint8_t> &out) const
{
    out.push_back(_strobe_on ? 1 : 0);
    for (int i = 0; i < NES_MAX_PLAYER; ++i)
    {
        out.push_back(uint8_t(_button_flags[i]));
        out.push_back(_button_id[i]);
    }
}

bool nes_input::deserialize(const uint8_t *data, size_t size, size_t &offset)
{
    const size_t bytes = 1 + NES_MAX_PLAYER * 2;
    if (offset + bytes > size)
        return false;

    _strobe_on = data[offset++] != 0;
    for (int i = 0; i < NES_MAX_PLAYER; ++i)
    {
        _button_flags[i] = nes_button_flags(data[offset++]);
        _button_id[i] = data[offset++];
    }

    return true;
}
