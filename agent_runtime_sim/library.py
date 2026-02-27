from __future__ import annotations

from dataclasses import dataclass
from typing import Dict, Iterable, List

from .agents import AgentState, BaseAgentController
from .simulation import SimulationConfig, SimulationRuntime
from .world import WorldModel


@dataclass
class SimulationSummary:
    ticks_executed: int
    living_agents: int
    top_score: int
    total_events: int


class AgentGameLibrary:
    """Factory + orchestration API for running shared enclosed-world simulations."""

    def create_world(self, width: int, height: int, interior: Iterable[Iterable[int]] | None = None) -> WorldModel:
        return WorldModel.enclosed(width=width, height=height, interior=interior)

    def run_simulation(
        self,
        world: WorldModel,
        agent_states: List[AgentState],
        controllers: Dict[str, BaseAgentController],
        config: SimulationConfig | None = None,
    ) -> tuple[SimulationRuntime, SimulationSummary]:
        runtime = SimulationRuntime(world=world, config=config or SimulationConfig())
        for state in agent_states:
            controller = controllers.get(state.agent_id)
            if controller is None:
                raise ValueError(f"Missing controller for agent: {state.agent_id}")
            runtime.register_agent(state=state, controller=controller)

        runtime.run()
        summary = SimulationSummary(
            ticks_executed=runtime.tick,
            living_agents=sum(1 for a in runtime.agents.values() if a.alive),
            top_score=max((a.score for a in runtime.agents.values()), default=0),
            total_events=len(runtime.events),
        )
        return runtime, summary
