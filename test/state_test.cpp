#include "stdafx.h"

#include "doctest.h"
#include "nes_system.h"

TEST_CASE("NES state serialize/deserialize round-trip") {
    nes_system system;
    system.power_on();
    system.load_rom("./roms/nestest/nestest.nes", nes_rom_exec_mode_direct);

    for (int i = 0; i < 5000; ++i)
        system.step(nes_cycle_t(1));

    nes_state_blob saved = system.serialize();

    for (int i = 0; i < 2500; ++i)
        system.step(nes_cycle_t(1));

    CHECK(system.deserialize(saved));

    nes_state_blob restored = system.serialize();
    CHECK(restored.bytes == saved.bytes);
}
