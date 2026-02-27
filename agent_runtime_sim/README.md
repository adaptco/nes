# Agent Runtime Simulation Library

`agent_runtime_sim` provides a reusable game library for modeling multiple agents in a deterministic, enclosed world.

## Features

- Enclosed grid-world model with immutable perimeter walls.
- Multi-agent runtime with per-tick state transitions.
- Hazard/resource mechanics for runtime scoring and survivability.
- Controller abstraction so each agent can use custom decision policies.
- Event log for replay, auditing, and downstream analytics.

## Quick Start

```python
from agent_runtime_sim import (
    AgentGameLibrary,
    AgentState,
    RandomWalkController,
    SimulationConfig,
)

lib = AgentGameLibrary()
world = lib.create_world(8, 8)

agents = [
    AgentState(agent_id="alpha", x=1, y=1),
    AgentState(agent_id="bravo", x=6, y=6),
]

controllers = {
    "alpha": RandomWalkController(seed=1),
    "bravo": RandomWalkController(seed=2),
}

runtime, summary = lib.run_simulation(
    world=world,
    agent_states=agents,
    controllers=controllers,
    config=SimulationConfig(max_ticks=50),
)

print(summary)
print(runtime.events[:3])
```

## World Encoding

Interior map values:

- `0` => EMPTY
- `1` => WALL
- `2` => RESOURCE
- `3` => HAZARD

Example interior (for a 6x6 world, interior is 4x4):

```python
interior = [
  [0, 0, 3, 0],
  [0, 1, 0, 0],
  [2, 0, 0, 0],
  [0, 0, 0, 0],
]
world = lib.create_world(6, 6, interior=interior)
```
