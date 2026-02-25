#include "stdafx.h"
#include <cstring>

#include <nes_input.h>

// Make compiler happy about pure virtual dtors
nes_input_device::~nes_input_device()
{}
namespace {
template<typename T>
void nes_state_write_input(vector<uint8_t> &out, const T &v)
{
    const uint8_t *p = reinterpret_cast<const uint8_t*>(&v);
    out.insert(out.end(), p, p + sizeof(T));
}

template<typename T>
bool nes_state_read_input(const vector<uint8_t> &in, size_t &offset, T &v)
{
    if (offset + sizeof(T) > in.size())
        return false;
    memcpy(&v, in.data() + offset, sizeof(T));
    offset += sizeof(T);
    return true;
}
}

void nes_input::serialize(vector<uint8_t> &out) const
{
    nes_state_write_input(out, _strobe_on);
    for (int i = 0; i < NES_MAX_PLAYER; ++i)
    {
        uint8_t flags = static_cast<uint8_t>(_button_flags[i]);
        nes_state_write_input(out, flags);
        nes_state_write_input(out, _button_id[i]);
    }
}

bool nes_input::deserialize(const vector<uint8_t> &in, size_t &offset)
{
    if (!nes_state_read_input(in, offset, _strobe_on))
        return false;

    for (int i = 0; i < NES_MAX_PLAYER; ++i)
    {
        uint8_t flags = 0;
        if (!nes_state_read_input(in, offset, flags) ||
            !nes_state_read_input(in, offset, _button_id[i]))
        {
            return false;
        }
        _button_flags[i] = static_cast<nes_button_flags>(flags);
    }

    return true;
}
