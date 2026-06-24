"""
openorbit.rewards — Composable reward function library.

Each reward component implements the RewardPlugin protocol and can be
combined with weights via RewardComposer.
"""

from __future__ import annotations

from typing import Protocol, runtime_checkable
import numpy as np

MU_EARTH = 3.986004418e14
R_EARTH  = 6.3781e6


@runtime_checkable
class RewardPlugin(Protocol):
    """Protocol for all reward components."""

    def compute(self, obs: dict, action: np.ndarray, info: dict) -> float: ...
    def reset(self) -> None: ...


class RewardComposer:
    """
    Linearly combines reward components with per-component weights.

    Example::

        rewards = RewardComposer([
            (OrbitalPrecisionReward(target_orbit), 1.0),
            (FuelEfficiencyReward(),                0.3),
            (SurvivalReward(),                      0.5),
        ])
        r = rewards.compute(obs, action, info)
    """

    def __init__(self, components: list[tuple[RewardPlugin, float]]) -> None:
        self._components = components

    def compute(self, obs: dict, action: np.ndarray, info: dict) -> float:
        total = 0.0
        for plugin, weight in self._components:
            total += weight * plugin.compute(obs, action, info)
        return total

    def reset(self) -> None:
        for plugin, _ in self._components:
            plugin.reset()


# ──────────────────────────────────────────────────────────────────────────────
# Built-in reward components
# ──────────────────────────────────────────────────────────────────────────────

class OrbitalPrecisionReward:
    """
    Reward for matching a target orbit.
    Returns 0 at perfect match, increasingly negative as orbital error grows.
    Normalised so that 10 km altitude error → reward ≈ -1.
    """

    def __init__(self, target_altitude_m: float = 400e3, target_ecc: float = 0.0) -> None:
        self.target_r   = R_EARTH + target_altitude_m
        self.target_ecc = target_ecc

    def compute(self, obs: dict, action: np.ndarray, info: dict) -> float:
        pos = obs.get("position_eci", np.zeros(3))
        vel = obs.get("velocity_eci", np.zeros(3))
        r   = float(np.linalg.norm(pos))
        v2  = float(np.linalg.norm(vel) ** 2)
        energy = 0.5 * v2 - MU_EARTH / r
        a = -MU_EARTH / (2 * energy) if abs(energy) > 1e-6 else 1e30
        alt_err = abs(a - self.target_r) / 10e3   # normalised by 10 km
        return -alt_err

    def reset(self) -> None:
        pass


class FuelEfficiencyReward:
    """Penalty proportional to fuel consumed this step."""

    def __init__(self) -> None:
        self._prev_fuel: float | None = None

    def compute(self, obs: dict, action: np.ndarray, info: dict) -> float:
        fuel_frac = obs.get("fuel_fraction", np.array([1.0]))
        fuel_frac_val = float(np.mean(fuel_frac))
        if self._prev_fuel is None:
            self._prev_fuel = fuel_frac_val
            return 0.0
        consumed = max(0.0, self._prev_fuel - fuel_frac_val)
        self._prev_fuel = fuel_frac_val
        return -consumed * 10.0  # -10 per 1% fuel burned

    def reset(self) -> None:
        self._prev_fuel = None


class SurvivalReward:
    """Fixed +1 reward per step; 0 if crashed (altitude < 50 km)."""

    def compute(self, obs: dict, action: np.ndarray, info: dict) -> float:
        alt = float(obs.get("altitude", np.array([1e6]))[0])
        return 1.0 if alt > 50e3 else 0.0

    def reset(self) -> None:
        pass


class SmoothControlReward:
    """Penalise jerky control inputs (high-frequency changes)."""

    def __init__(self) -> None:
        self._prev_action: np.ndarray | None = None

    def compute(self, obs: dict, action: np.ndarray, info: dict) -> float:
        if self._prev_action is None:
            self._prev_action = action.copy() if isinstance(action, np.ndarray) else action
            return 0.0
        delta = float(np.linalg.norm(action - self._prev_action))
        self._prev_action = action.copy() if isinstance(action, np.ndarray) else action
        return -delta

    def reset(self) -> None:
        self._prev_action = None


class AtmosphericEntryReward:
    """Bonus for staying within the aerocapture thermal corridor."""

    def __init__(self, max_heat_K: float = 1500.0) -> None:
        self.max_heat_K = max_heat_K

    def compute(self, obs: dict, action: np.ndarray, info: dict) -> float:
        temp = float(obs.get("temperature_map", np.array([293.0]))[0])
        if temp < self.max_heat_K * 0.7:
            return 1.0
        elif temp < self.max_heat_K:
            margin = (self.max_heat_K - temp) / (self.max_heat_K * 0.3)
            return margin
        return -5.0  # overheat penalty

    def reset(self) -> None:
        pass
