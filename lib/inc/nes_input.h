#pragma once

// Reading NES controller requires setting a strobe bit, clear it, then read the I/O register 8 times
// http://wiki.nesdev.com/w/index.php/Standard_controller

#include <cstdint>

class nes_state_stream;

#define NES_CONTROLLER_STROBE_BIT 0x1

// The controller are reported always in bit 0 in the order of 
// A, B, Select, Start, Up, Down, Left, Right
// When shifting them leftwards, you get the following bits
enum nes_button_flags : uint8_t
{
    nes_button_flags_none = 0x0,
    nes_button_flags_a = 0x80,
    nes_button_flags_b = 0x40,
    nes_button_flags_select = 0x20,
    nes_button_flags_start = 0x10,
    nes_button_flags_up = 0x8,
    nes_button_flags_down = 0x4,
    nes_button_flags_left = 0x2,
    nes_button_flags_right = 0x1
};


#define NES_MAX_PLAYER 4

// Should be implemented by joystick/game controller code that are from other framework or platform specific
// Example: SDL_game_controller : nes_user_input backed by SDL_GameController, etc
class nes_input_device
{
public :
    virtual nes_button_flags poll_status() = 0;

    virtual ~nes_input_device() = 0;
};

class nes_input : public nes_component
{
public :
    //
    // nes_component overrides
    //
    virtual void power_on(nes_system *system)
    {
        init();
    }

    virtual void reset()
    {
        init();
    }

    virtual void step_to(nes_cycle_t count)
    {
        // Do nothing
    }

public :
    void register_input(int id, shared_ptr<nes_input_device> input) { _user_inputs[id] = input; }
    void unregister_input(int id) { _user_inputs[id] = nullptr; }
    void unregister_all_inputs() { for (auto &input : _user_inputs) input = nullptr; }

    void serialize(nes_state_stream &stream) const;
    bool deserialize(nes_state_stream &stream);

    void write_CONTROLLER(uint8_t val)
    {
        bool prev_strobe = _strobe_on;
        _strobe_on = (val & NES_CONTROLLER_STROBE_BIT);
        if (prev_strobe && !_strobe_on)
        {
            reload();
        }
    }

    uint8_t read_CONTROLLER(uint8_t id)
    {
        if (_strobe_on)
            reload();

        return 0x40 | ((_button_flags[id] >> (7 - _button_id[id]++)) & 0x1);
    }

private :
    void init()
    {
        _strobe_on = false;
        for (int i = 0; i < NES_MAX_PLAYER; ++i)
        {
            _button_flags[i] = nes_button_flags_none;
            _button_id[i] = 0;
        }
    }

    void reload()
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

public :
    bool _strobe_on;
    nes_button_flags _button_flags[NES_MAX_PLAYER];
    uint8_t _button_id[NES_MAX_PLAYER];
    shared_ptr<nes_input_device> _user_inputs[NES_MAX_PLAYER];
};
