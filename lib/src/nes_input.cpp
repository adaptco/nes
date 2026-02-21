#include "stdafx.h"

#include <nes_input.h>

// Make compiler happy about pure virtual dtors
nes_input_device::~nes_input_device()
{}

nes_replay_input_device::nes_replay_input_device()
    : _cursor(0), _hold_last(true)
{}

nes_replay_input_device::nes_replay_input_device(const std::vector<nes_button_flags> &stream, bool hold_last)
    : _stream(stream), _cursor(0), _hold_last(hold_last)
{}

nes_button_flags nes_replay_input_device::poll_status()
{
    if (_stream.empty())
    {
        return nes_button_flags_none;
    }

    if (_cursor < _stream.size())
    {
        return _stream[_cursor++];
    }

    if (_hold_last)
    {
        return _stream.back();
    }

    return nes_button_flags_none;
}

void nes_replay_input_device::set_stream(const std::vector<nes_button_flags> &stream, bool hold_last)
{
    _stream = stream;
    _hold_last = hold_last;
    _cursor = 0;
}

void nes_replay_input_device::reset()
{
    _cursor = 0;
}
