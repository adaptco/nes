from __future__ import annotations

from dataclasses import dataclass
from enum import IntEnum
from typing import Iterable, List, Tuple


class CellType(IntEnum):
    EMPTY = 0
    WALL = 1
    RESOURCE = 2
    HAZARD = 3


@dataclass(frozen=True)
class WorldModel:
    """Rectangular enclosed world model with immutable boundaries."""

    width: int
    height: int
    grid: Tuple[Tuple[CellType, ...], ...]

    @staticmethod
    def enclosed(width: int, height: int, interior: Iterable[Iterable[int]] | None = None) -> "WorldModel":
        if width < 3 or height < 3:
            raise ValueError("Enclosed world must be at least 3x3.")

        base: List[List[CellType]] = [[CellType.EMPTY for _ in range(width)] for _ in range(height)]
        for x in range(width):
            base[0][x] = CellType.WALL
            base[height - 1][x] = CellType.WALL
        for y in range(height):
            base[y][0] = CellType.WALL
            base[y][width - 1] = CellType.WALL

        if interior is not None:
            rows = [list(r) for r in interior]
            if len(rows) != height - 2:
                raise ValueError("Interior row count mismatch.")
            for y, row in enumerate(rows, start=1):
                if len(row) != width - 2:
                    raise ValueError("Interior column count mismatch.")
                for x, v in enumerate(row, start=1):
                    base[y][x] = CellType(v)

        return WorldModel(width=width, height=height, grid=tuple(tuple(r) for r in base))

    def in_bounds(self, x: int, y: int) -> bool:
        return 0 <= x < self.width and 0 <= y < self.height

    def cell(self, x: int, y: int) -> CellType:
        if not self.in_bounds(x, y):
            return CellType.WALL
        return self.grid[y][x]

    def is_walkable(self, x: int, y: int) -> bool:
        return self.cell(x, y) != CellType.WALL
