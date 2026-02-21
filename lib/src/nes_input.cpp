#include "stdafx.h"

#include <nes_input.h>
#include <nes_mapper.h>

// Make compiler happy about pure virtual dtors
nes_input_device::~nes_input_device()
{}

void nes_input::serialize(nes_state_stream &stream) const
{
    uint8_t strobe_on = _strobe_on ? 1 : 0;
    stream.write(strobe_on);
    for (int i = 0; i < NES_MAX_PLAYER; ++i)
    {
        auto button_flags = uint8_t(_button_flags[i]);
        stream.write(button_flags);
        stream.write(_button_id[i]);
    }
}

bool nes_input::deserialize(nes_state_stream &stream)
{
    uint8_t strobe_on = 0;
    if (!stream.read(strobe_on)) return false;
    _strobe_on = (strobe_on != 0);

    for (int i = 0; i < NES_MAX_PLAYER; ++i)
    {
        uint8_t button_flags = 0;
        if (!stream.read(button_flags)) return false;
        _button_flags[i] = nes_button_flags(button_flags);
        if (!stream.read(_button_id[i])) return false;
    }

    return stream.ok();
}
