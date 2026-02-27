#include "stdafx.h"
#include <cstring>

#include <nes_input.h>

namespace {
template <typename T>
void input_append_value(vector<uint8_t> &out, const T &value)
{
    const auto *ptr = reinterpret_cast<const uint8_t *>(&value);
    out.insert(out.end(), ptr, ptr + sizeof(T));
}

template <typename T>
bool input_read_value(const uint8_t *&cursor, const uint8_t *end, T &value)
{
    if (cursor + sizeof(T) > end)
        return false;
    memcpy(&value, cursor, sizeof(T));
    cursor += sizeof(T);
    return true;
}
}

// Make compiler happy about pure virtual dtors
nes_input_device::~nes_input_device()
{}

void nes_input::serialize(std::vector<uint8_t> &out) const
{
    input_append_value(out, _strobe_on);
    for (int i = 0; i < NES_MAX_PLAYER; ++i)
    {
        input_append_value(out, _button_flags[i]);
        input_append_value(out, _button_id[i]);
    }
}

bool nes_input::deserialize(const uint8_t *&cursor, const uint8_t *end)
{
    if (!input_read_value(cursor, end, _strobe_on))
        return false;

    for (int i = 0; i < NES_MAX_PLAYER; ++i)
    {
        if (!input_read_value(cursor, end, _button_flags[i]) || !input_read_value(cursor, end, _button_id[i]))
            return false;
    }

    return true;
}
