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

struct nes_memory_view
{
    const uint8_t *data;
    size_t size;
};

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
    // Run the PRG ROM directly - useful for ROM-based automation test path
    nes_rom_exec_mode_direct,

    // At power on, jump directly to the reset 'interrupt' handler which is effectively main
    // This is what ROM does typically
    // Interestingly this isn't really "documented" in nesdev.com - I had to infer it from
    // inspecting ROMs and using debugger from other emulators
    nes_rom_exec_mode_reset
};

struct nes_state_blob
{
    vector<uint8_t> data;
};


//
// The NES system hardware that manages all the invidual components - CPU, PPU, APU, RAM, etc
// It synchronizes between different components
//
class nes_system
{
public :
    nes_system();
    ~nes_system();

public :
    void power_on();
    void reset();

    // Stop the emulation engine and exit the main loop
    void stop() { _stop_requested = true; }

    void run_program(vector<uint8_t> &&program, uint16_t addr);
    void run_rom(const char *rom_path, nes_rom_exec_mode mode);

    void load_rom(const char *rom_path, nes_rom_exec_mode mode);

    nes_cpu     *cpu()      { return _cpu.get();   }
    nes_memory  *ram()      { return _ram.get();   }
    nes_ppu     *ppu()      { return _ppu.get();   } 
    nes_input   *input()    { return _input.get(); }

    // Returns a read-only snapshot view for deterministic embedding extraction.
    //
    // Snapshot contract:
    // - frame_buffer points to the latest fully completed frame only.
    // - The frame pointer remains stable until the next PPU frame boundary swap.
    // - PPU swaps buffers at the 261->0 scanline transition (end of frame).
    // - Callers should sample snapshots at a consistent point in emulation timing for
    //   reproducible embeddings.
    nes_system_snapshot snapshot() const;

    nes_state_blob serialize() const;
    bool deserialize(const nes_state_blob &state);

public :
    //
    // step <count> amount of cycles
    // We have a few options:
    // 1. Let nes_system drive the cycle - and each component own their own stepping towards that cycle
    // 2. Let CPU drive cycle - and other component "catch up"
    // 3. Let each component own their own thread - and synchronizes at cycle granuarity
    //
    // I think option #1 will produce the most accurate timing without subjecting too much to OS resource
    // management.
    //
    void step(nes_cycle_t count);

    bool stop_requested() { return _stop_requested; }

private :
    // Emulation loop that is only intended for tests 
    void test_loop();

    void init();

private :
    nes_cycle_t _master_cycle;              // keep count of current cycle

    unique_ptr<nes_cpu> _cpu;
    unique_ptr<nes_memory> _ram;
    unique_ptr<nes_ppu> _ppu;
    unique_ptr<nes_input> _input;

    vector<nes_component *> _components;

    bool _stop_requested;                   // useful for internal testing, or synchronization to rendering
};
