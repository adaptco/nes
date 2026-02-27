#pragma once

// Reading NES controller requires setting a strobe bit, clear it, then read the I/O register 8 times
// http://wiki.nesdev.com/w/index.php/Standard_controller

#include <cstdint>
#include <memory>
#include <vector>

#include <nes_component.h>

#define NES_CONTROLLER_STROBE_BIT 0x1
#define NES_MAX_PLAYER 4

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

class nes_input_device
{
public:
    virtual nes_button_flags poll_status() = 0;
    virtual ~nes_input_device() = 0;
};

class nes_input : public nes_component
{
public:
    void serialize(std::vector<uint8_t> &out) const;
    bool deserialize(const uint8_t *data, size_t size, size_t &offset);

public:
    virtual void power_on(nes_system *system) { init(); }
    virtual void reset() { init(); }
    virtual void step_to(nes_cycle_t count) {}

    void register_input(int id, std::shared_ptr<nes_input_device> input) { _user_inputs[id] = input; }
    void unregister_input(int id) { _user_inputs[id] = nullptr; }
    void unregister_all_inputs() { for (auto &input : _user_inputs) input = nullptr; }

private:
    void init();
    void reload();

public:
    void write_CONTROLLER(uint8_t val);
    uint8_t read_CONTROLLER(uint8_t id);

public:
    bool _strobe_on;
    nes_button_flags _button_flags[NES_MAX_PLAYER];
    uint8_t _button_id[NES_MAX_PLAYER];
    std::shared_ptr<nes_input_device> _user_inputs[NES_MAX_PLAYER];
};
