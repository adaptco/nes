import unittest

from agent_runtime_sim import AgentGameLibrary, AgentState, RandomWalkController, SimulationConfig


class RuntimeTests(unittest.TestCase):
    def test_enclosed_world_boundaries(self):
        lib = AgentGameLibrary()
        world = lib.create_world(5, 5)
        self.assertFalse(world.is_walkable(0, 0))
        self.assertTrue(world.is_walkable(2, 2))

    def test_simulation_generates_events(self):
        lib = AgentGameLibrary()
        world = lib.create_world(6, 6)
        agents = [AgentState(agent_id="a", x=1, y=1), AgentState(agent_id="b", x=4, y=4)]
        controllers = {
            "a": RandomWalkController(seed=7),
            "b": RandomWalkController(seed=8),
        }
        runtime, summary = lib.run_simulation(
            world=world,
            agent_states=agents,
            controllers=controllers,
            config=SimulationConfig(max_ticks=10),
        )

        self.assertEqual(summary.total_events, len(runtime.events))
        self.assertGreater(summary.total_events, 0)
        self.assertEqual(summary.ticks_executed, 10)


if __name__ == "__main__":
    unittest.main()
