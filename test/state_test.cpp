#include "stdafx.h"

#include "doctest.h"
#include "nes_system.h"

namespace
{
    static const uint32_t TEST_STATE_MAGIC = 0x3153454e; // NES1
    static const uint32_t TEST_STATE_VERSION_V1 = 1;

    void push_u32(std::vector<uint8_t> &out, uint32_t value)
    {
        for (int i = 0; i < 4; ++i)
            out.push_back(uint8_t((value >> (i * 8)) & 0xff));
    }

    void push_u64(std::vector<uint8_t> &out, uint64_t value)
    {
        for (int i = 0; i < 8; ++i)
            out.push_back(uint8_t((value >> (i * 8)) & 0xff));
    }

    nes_system make_nestest_system(int steps = 5000)
    {
        nes_system system;
        system.power_on();
        system.load_rom("./roms/nestest/nestest.nes", nes_rom_exec_mode_direct);

        for (int i = 0; i < steps; ++i)
            system.step(nes_cycle_t(1));

        return system;
    }

    nes_state_blob make_v1_fixture_from_system(nes_system &source)
    {
        nes_state_blob fixture;

        std::vector<uint8_t> cpu_state;
        std::vector<uint8_t> ram_state;
        std::vector<uint8_t> ppu_state;
        std::vector<uint8_t> input_state;

        source.cpu()->serialize(cpu_state);
        source.ram()->serialize(ram_state);
        source.ppu()->serialize(ppu_state);
        source.input()->serialize(input_state);

        push_u32(fixture.data, TEST_STATE_MAGIC);
        push_u32(fixture.data, TEST_STATE_VERSION_V1);
        push_u64(fixture.data, uint64_t(0));
        fixture.data.push_back(0);

        push_u32(fixture.data, uint32_t(cpu_state.size()));
        fixture.data.insert(fixture.data.end(), cpu_state.begin(), cpu_state.end());

        push_u32(fixture.data, uint32_t(ram_state.size()));
        fixture.data.insert(fixture.data.end(), ram_state.begin(), ram_state.end());

        push_u32(fixture.data, uint32_t(ppu_state.size()));
        fixture.data.insert(fixture.data.end(), ppu_state.begin(), ppu_state.end());

        push_u32(fixture.data, uint32_t(input_state.size()));
        fixture.data.insert(fixture.data.end(), input_state.begin(), input_state.end());

        return fixture;
    }
}

TEST_CASE("NES state v2 serialize/deserialize round-trip") {
    nes_system system = make_nestest_system();

    nes_state_blob saved = system.serialize();

    for (int i = 0; i < 2500; ++i)
        system.step(nes_cycle_t(1));

    CHECK(system.deserialize(saved));

    nes_state_blob restored = system.serialize();
    CHECK(restored.data == saved.data);
}

TEST_CASE("NES state deserialize accepts known-good v1 fixture") {
    nes_system source = make_nestest_system();
    nes_state_blob v1_fixture = make_v1_fixture_from_system(source);

    nes_system target = make_nestest_system(100);
    CHECK(target.deserialize(v1_fixture));

    nes_state_blob target_after_restore = target.serialize();
    nes_state_blob source_current = source.serialize();
    CHECK(target_after_restore.data == source_current.data);
}

TEST_CASE("NES state deserialize rejects future version") {
    nes_system system = make_nestest_system();
    nes_state_blob blob = system.serialize();

    blob.data[4] = 3;
    blob.data[5] = 0;
    blob.data[6] = 0;
    blob.data[7] = 0;

    CHECK_FALSE(system.deserialize(blob));
}

TEST_CASE("NES state deserialize rejects unknown old version") {
    nes_system system = make_nestest_system();
    nes_state_blob blob = system.serialize();

    blob.data[4] = 0;
    blob.data[5] = 0;
    blob.data[6] = 0;
    blob.data[7] = 0;

    CHECK_FALSE(system.deserialize(blob));
}
