#include "stdafx.h"
#include <cstring>

#include <nes_input.h>
#include <nes_mapper.h>

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

void nes_input::serialize(vector<uint8_t> &out) const
{
    append_state(out, _strobe_on);
    out.insert(out.end(), reinterpret_cast<const uint8_t *>(_button_flags), reinterpret_cast<const uint8_t *>(_button_flags) + sizeof(_button_flags));
    out.insert(out.end(), _button_id, _button_id + sizeof(_button_id));
}

bool nes_input::deserialize(const vector<uint8_t> &in, size_t &offset)
{
    bool ok = read_state(in, offset, &_strobe_on);
    if (!ok)
        return false;

    if (offset + sizeof(_button_flags) + sizeof(_button_id) > in.size())
        return false;

    memcpy(_button_flags, in.data() + offset, sizeof(_button_flags));
    offset += sizeof(_button_flags);
    memcpy(_button_id, in.data() + offset, sizeof(_button_id));
    offset += sizeof(_button_id);

    return true;
}
