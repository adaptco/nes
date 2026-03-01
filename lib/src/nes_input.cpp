#include "stdafx.h"
#include <cstring>

#include <nes_input.h>

// Make compiler happy about pure virtual dtors
nes_input_device::~nes_input_device()
{}

void nes_input::serialize(vector<uint8_t> &out) const
{
    out.push_back(_strobe_on ? 1 : 0);
    out.insert(out.end(), reinterpret_cast<const uint8_t *>(_button_flags), reinterpret_cast<const uint8_t *>(_button_flags) + sizeof(_button_flags));
    out.insert(out.end(), _button_id, _button_id + sizeof(_button_id));
}

bool nes_input::deserialize(const uint8_t *data, size_t size, size_t &offset)
{
    if (offset + 1 + sizeof(_button_flags) + sizeof(_button_id) > size)
        return false;

    _strobe_on = data[offset++] != 0;
    memcpy(_button_flags, data + offset, sizeof(_button_flags));
    offset += sizeof(_button_flags);
    memcpy(_button_id, data + offset, sizeof(_button_id));
    offset += sizeof(_button_id);

    return true;
}
