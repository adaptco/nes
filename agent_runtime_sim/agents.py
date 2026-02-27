from __future__ import annotations

import random
from dataclasses import dataclass
from typing import Dict, Tuple


MOVE_VECTORS: Dict[str, Tuple[int, int]] = {
    "UP": (0, -1),
    "DOWN": (0, 1),
    "LEFT": (-1, 0),
    "RIGHT": (1, 0),
    "STAY": (0, 0),
}


@dataclass(frozen=True)
class AgentAction:
    move: str = "STAY"


@dataclass
class AgentState:
    agent_id: str
    x: int
    y: int
    energy: int = 100
    score: int = 0
    alive: bool = True


class BaseAgentController:
    def decide(self, view: dict) -> AgentAction:
        raise NotImplementedError


class RandomWalkController(BaseAgentController):
    def __init__(self, seed: int | None = None) -> None:
        self._rng = random.Random(seed)

    def decide(self, view: dict) -> AgentAction:
        choices = list(MOVE_VECTORS.keys())
        return AgentAction(move=self._rng.choice(choices))
