from __future__ import annotations

from dataclasses import dataclass, field
from typing import Dict, List, Tuple

from .agents import AgentAction, AgentState, BaseAgentController, MOVE_VECTORS
from .world import CellType, WorldModel


@dataclass(frozen=True)
class SimulationConfig:
    max_ticks: int = 1000
    hazard_damage: int = 20
    resource_reward: int = 10
    idle_energy_cost: int = 1
    move_energy_cost: int = 2


@dataclass
class TickEvent:
    tick: int
    agent_id: str
    action: str
    position: Tuple[int, int]
    energy_delta: int
    score_delta: int
    note: str = ""


@dataclass
class SimulationRuntime:
    world: WorldModel
    config: SimulationConfig = field(default_factory=SimulationConfig)
    agents: Dict[str, AgentState] = field(default_factory=dict)
    controllers: Dict[str, BaseAgentController] = field(default_factory=dict)
    tick: int = 0
    events: List[TickEvent] = field(default_factory=list)

    def register_agent(self, state: AgentState, controller: BaseAgentController) -> None:
        if state.agent_id in self.agents:
            raise ValueError(f"Agent already registered: {state.agent_id}")
        if not self.world.is_walkable(state.x, state.y):
            raise ValueError("Agent spawn must be on walkable cell.")
        self.agents[state.agent_id] = state
        self.controllers[state.agent_id] = controller

    def _resolve_move(self, state: AgentState, action: AgentAction) -> Tuple[int, int, str]:
        dx, dy = MOVE_VECTORS.get(action.move, (0, 0))
        nx, ny = state.x + dx, state.y + dy
        if not self.world.is_walkable(nx, ny):
            return state.x, state.y, "blocked"
        return nx, ny, "moved" if (dx or dy) else "idle"

    def _apply_cell_effects(self, state: AgentState) -> Tuple[int, int, str]:
        cell = self.world.cell(state.x, state.y)
        energy_delta = 0
        score_delta = 0
        note = ""
        if cell == CellType.HAZARD:
            energy_delta -= self.config.hazard_damage
            note = "hazard"
        elif cell == CellType.RESOURCE:
            score_delta += self.config.resource_reward
            note = "resource"
        return energy_delta, score_delta, note

    def step(self) -> None:
        self.tick += 1
        for agent_id, state in self.agents.items():
            if not state.alive:
                continue
            ctrl = self.controllers[agent_id]
            action = ctrl.decide(
                {
                    "tick": self.tick,
                    "self": {"x": state.x, "y": state.y, "energy": state.energy, "score": state.score},
                }
            )

            old_pos = (state.x, state.y)
            state.x, state.y, move_note = self._resolve_move(state, action)
            move_cost = self.config.move_energy_cost if old_pos != (state.x, state.y) else self.config.idle_energy_cost

            energy_delta, score_delta, cell_note = self._apply_cell_effects(state)
            net_energy = energy_delta - move_cost

            state.energy += net_energy
            state.score += score_delta
            if state.energy <= 0:
                state.alive = False

            note = ",".join([n for n in (move_note, cell_note, "dead" if not state.alive else "") if n])
            self.events.append(
                TickEvent(
                    tick=self.tick,
                    agent_id=agent_id,
                    action=action.move,
                    position=(state.x, state.y),
                    energy_delta=net_energy,
                    score_delta=score_delta,
                    note=note,
                )
            )

    def run(self) -> None:
        while self.tick < self.config.max_ticks and any(a.alive for a in self.agents.values()):
            self.step()
