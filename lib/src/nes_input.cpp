#include "stdafx.h"
#include <cstring>

#include <nes_input.h>

// Make compiler happy about pure virtual dtors
nes_input_device::~nes_input_device()
{}
namespace
{
    template<typename T>
    void write_scalar(vector<uint8_t> &out, const T &value)
    {
        const uint8_t *p = reinterpret_cast<const uint8_t *>(&value);
        out.insert(out.end(), p, p + sizeof(T));
    }

    template<typename T>
    bool read_scalar(const uint8_t *&ptr, const uint8_t *end, T &value)
    {
        if (end - ptr < (ptrdiff_t)sizeof(T))
            return false;
        memcpy(&value, ptr, sizeof(T));
        ptr += sizeof(T);
        return true;
    }
}

void nes_input::serialize(vector<uint8_t> &out) const
{
    write_scalar(out, _strobe_on);
    for (int i = 0; i < NES_MAX_PLAYER; ++i)
    {
        uint8_t flags = static_cast<uint8_t>(_button_flags[i]);
        write_scalar(out, flags);
        write_scalar(out, _button_id[i]);
    }
}

bool nes_input::deserialize(const uint8_t *&ptr, const uint8_t *end)
{
    if (!read_scalar(ptr, end, _strobe_on))
        return false;

    for (int i = 0; i < NES_MAX_PLAYER; ++i)
    {
        uint8_t flags;
        if (!read_scalar(ptr, end, flags) || !read_scalar(ptr, end, _button_id[i]))
            return false;
        _button_flags[i] = static_cast<nes_button_flags>(flags);
    }

    return true;
}
