#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "nes_component.h"

using namespace std;

class nes_cpu;
class nes_memory;
class nes_apu;
class nes_ppu;
class nes_input;

struct nes_system_snapshot
{
    // Pointer to the latest fully completed frame (palette-index pixels).
    const uint8_t *frame_buffer;
    uint16_t frame_width;
    uint16_t frame_height;

    // CPU RAM (full CPU-visible RAM array).
    const uint8_t *cpu_ram;
    size_t cpu_ram_size;

    // PPU memory useful for visual/state embeddings.
    const uint8_t *ppu_vram;
    size_t ppu_vram_size;

    const uint8_t *ppu_oam;
    size_t ppu_oam_size;
};

enum nes_rom_exec_mode
{
    nes_rom_exec_mode_direct,
    nes_rom_exec_mode_reset
};

struct nes_state_blob
{
    vector<uint8_t> data;
};

class nes_system
{
public:
    nes_system();
    ~nes_system();

    void power_on();
    void reset();
    void stop() { _stop_requested = true; }

    void run_program(vector<uint8_t> &&program, uint16_t addr);
    void run_rom(const char *rom_path, nes_rom_exec_mode mode);
    void load_rom(const char *rom_path, nes_rom_exec_mode mode);

    nes_state_blob serialize() const;
    bool deserialize(const nes_state_blob &state);

    nes_cpu *cpu() { return _cpu.get(); }
    nes_memory *ram() { return _ram.get(); }
    nes_ppu *ppu() { return _ppu.get(); }
    nes_input *input() { return _input.get(); }

    nes_system_snapshot snapshot() const;

    void step(nes_cycle_t count);
    bool stop_requested() { return _stop_requested; }

private:
    void test_loop();
    void init();

private:
    nes_cycle_t _master_cycle;

    unique_ptr<nes_cpu> _cpu;
    unique_ptr<nes_memory> _ram;
    unique_ptr<nes_ppu> _ppu;
    unique_ptr<nes_input> _input;

    vector<nes_component *> _components;
    bool _stop_requested;
};
