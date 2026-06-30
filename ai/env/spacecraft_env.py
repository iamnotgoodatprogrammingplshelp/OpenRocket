"""
openorbit.env.SpacecraftEnv
===========================
Gymnasium-compatible environment wrapping the OpenOrbit C++ physics engine.

Observation space: 200+ state variables covering orbital state, attitude,
propulsion, navigation, and noisy sensor readings.
Action space: continuous throttle, gimbal, RCS, and control surfaces.

The environment communicates with the C++ sim via a shared-memory bridge
(openorbit._bridge) when running in-process, or via ZMQ sockets for
distributed / multi-agent training.
"""

from __future__ import annotations

import math
from dataclasses import dataclass, field
from typing import Any

import numpy as np
import gymnasium as gym
from gymnasium import spaces

# ──────────────────────────────────────────────────────────────────────────────
# Constants
# ──────────────────────────────────────────────────────────────────────────────

MU_EARTH: float = 3.986004418e14  # m³/s²
R_EARTH: float = 6.3781e6  # m
G0: float = 9.80665  # m/s²


# ──────────────────────────────────────────────────────────────────────────────
# Vehicle configuration
# ──────────────────────────────────────────────────────────────────────────────


@dataclass
class VehicleConfig:
    """Defines the spacecraft structure for a scenario."""

    n_engines: int = 1
    n_tanks: int = 1
    n_gimbals: int = 1
    n_rcs: int = 4
    n_surfaces: int = 0
    dry_mass_kg: float = 3000.0
    fuel_mass_kg: float = 2000.0
    isp_vac_s: float = 348.0
    thrust_vac_N: float = 934_000.0


@dataclass
class ScenarioConfig:
    """Initial conditions and goal specification for an episode."""

    name: str = "circular_leo"

    # Initial orbit (if None → use initial_state directly)
    initial_altitude_m: float = 400e3
    initial_eccentricity: float = 0.0

    # Target orbit
    target_altitude_m: float = 400e3
    target_eccentricity: float = 0.0

    # Episode limits
    max_steps: int = 5000
    dt_s: float = 0.5  # physics timestep

    # Noise levels (0 = perfect sensors)
    imu_accel_noise: float = 0.02  # m/s²  (1σ)
    imu_gyro_noise: float = 0.001  # rad/s (1σ)
    gps_pos_noise: float = 2.5  # m     (CEP)

    vehicle: VehicleConfig = field(default_factory=VehicleConfig)


# ──────────────────────────────────────────────────────────────────────────────
# Pure-Python fallback physics (used when C++ bridge is unavailable)
# ──────────────────────────────────────────────────────────────────────────────


class _FallbackPhysics:
    """
    Minimal N-body propagator in Python (RK4, Earth-only).
    Used for testing / CI without a compiled C++ backend.
    """

    def __init__(self, cfg: ScenarioConfig) -> None:
        self.cfg = cfg
        r = R_EARTH + cfg.initial_altitude_m
        v = math.sqrt(MU_EARTH / r)
        self.pos = np.array([r, 0.0, 0.0])
        self.vel = np.array([0.0, v, 0.0])
        self.mass = cfg.vehicle.dry_mass_kg + cfg.vehicle.fuel_mass_kg
        self.fuel = cfg.vehicle.fuel_mass_kg
        self.quat = np.array([1.0, 0.0, 0.0, 0.0])  # w,x,y,z
        self.omega = np.zeros(3)
        self.t = 0.0

    def _gravity(self, pos: np.ndarray) -> np.ndarray:
        r3 = np.linalg.norm(pos) ** 3
        return -MU_EARTH / r3 * pos

    def _deriv(
        self, pos: np.ndarray, vel: np.ndarray, thrust_force: np.ndarray, mass: float
    ) -> tuple[np.ndarray, np.ndarray]:
        g = self._gravity(pos)
        a = g + thrust_force / max(mass, 1.0)
        return vel, a

    def step(self, throttle: float, gimbal: np.ndarray) -> None:
        dt = self.cfg.dt_s
        # Thrust force in ECI (simplified: along velocity direction)
        if self.fuel > 0 and throttle > 0:
            thrust_mag = self.cfg.vehicle.thrust_vac_N * throttle
            isp = self.cfg.vehicle.isp_vac_s
            mdot = thrust_mag / (isp * G0)
            v_hat = self.vel / (np.linalg.norm(self.vel) + 1e-10)
            thrust = v_hat * thrust_mag
            self.fuel = max(0.0, self.fuel - mdot * dt)
            self.mass = max(self.cfg.vehicle.dry_mass_kg, self.mass - mdot * dt)
        else:
            thrust = np.zeros(3)

        # RK4
        k1p, k1v = self._deriv(self.pos, self.vel, thrust, self.mass)
        k2p, k2v = self._deriv(
            self.pos + k1p * (dt / 2), self.vel + k1v * (dt / 2), thrust, self.mass
        )
        k3p, k3v = self._deriv(
            self.pos + k2p * (dt / 2), self.vel + k2v * (dt / 2), thrust, self.mass
        )
        k4p, k4v = self._deriv(
            self.pos + k3p * dt, self.vel + k3v * dt, thrust, self.mass
        )

        self.pos += dt / 6.0 * (k1p + 2 * k2p + 2 * k3p + k4p)
        self.vel += dt / 6.0 * (k1v + 2 * k2v + 2 * k3v + k4v)
        self.t += dt

    @property
    def altitude(self) -> float:
        return float(np.linalg.norm(self.pos) - R_EARTH)

    @property
    def speed(self) -> float:
        return float(np.linalg.norm(self.vel))


# ──────────────────────────────────────────────────────────────────────────────
# Gymnasium environment
# ──────────────────────────────────────────────────────────────────────────────


class SpacecraftEnv(gym.Env[dict[str, np.ndarray], dict[str, np.ndarray]]):
    """
    OpenOrbit Gymnasium environment.

    Observation dict keys (all float32/float64):
        position_eci        [3]   ECI position  [m]
        velocity_eci        [3]   ECI velocity  [m/s]
        altitude            [1]   AGL altitude  [m]
        orbital_elements    [6]   (a, e, i, Ω, ω, ν)
        quaternion          [4]   body→inertial
        angular_velocity    [3]   [rad/s] body frame
        mass                [1]   [kg]
        fuel_fraction       [N_tanks]
        engine_throttle     [N_engines]
        temperature_map     [1]   max part temperature [K]
        target_position     [3]   [m]
        target_velocity     [3]   [m/s]
        time_to_apoapsis    [1]   [s]
        time_to_periapsis   [1]   [s]
        imu_accel           [3]   noisy  [m/s²]
        imu_gyro            [3]   noisy  [rad/s]
        gps_position        [3]   noisy  [m]

    Action dict keys:
        throttle            [N_engines]  ∈ [0, 1]
        gimbal              [N_gimbals, 2] ∈ [-1, 1]
        rcs                 [N_rcs]      ∈ [0, 1]
        staging             int          ∈ {0, 1}
    """

    metadata = {"render_modes": ["human", "rgb_array"]}

    def __init__(
        self,
        scenario: ScenarioConfig | None = None,
        render_mode: str | None = None,
    ) -> None:
        super().__init__()
        self.cfg = scenario or ScenarioConfig()
        self.render_mode = render_mode
        self._rng = np.random.default_rng()
        self._physics: _FallbackPhysics | None = None
        self._step_count = 0

        v = self.cfg.vehicle
        Ne = v.n_engines
        Ng = v.n_gimbals
        Nr = v.n_rcs
        Nt = v.n_tanks

        # ── Observation space ─────────────────────────────────────────────────
        self.observation_space = spaces.Dict(
            {
                "position_eci": spaces.Box(-1e9, 1e9, (3,), np.float64),
                "velocity_eci": spaces.Box(-1e5, 1e5, (3,), np.float64),
                "altitude": spaces.Box(0, 1e9, (1,), np.float64),
                "orbital_elements": spaces.Box(-np.inf, np.inf, (6,), np.float64),
                "quaternion": spaces.Box(-1, 1, (4,), np.float64),
                "angular_velocity": spaces.Box(-50, 50, (3,), np.float64),
                "mass": spaces.Box(0, 1e7, (1,), np.float64),
                "fuel_fraction": spaces.Box(0, 1, (Nt,), np.float32),
                "engine_throttle": spaces.Box(0, 1, (Ne,), np.float32),
                "temperature_map": spaces.Box(0, 5000, (1,), np.float32),
                "target_position": spaces.Box(-1e9, 1e9, (3,), np.float64),
                "target_velocity": spaces.Box(-1e5, 1e5, (3,), np.float64),
                "time_to_apoapsis": spaces.Box(0, 1e6, (1,), np.float64),
                "time_to_periapsis": spaces.Box(0, 1e6, (1,), np.float64),
                "imu_accel": spaces.Box(-200, 200, (3,), np.float32),
                "imu_gyro": spaces.Box(-50, 50, (3,), np.float32),
                "gps_position": spaces.Box(-1e9, 1e9, (3,), np.float64),
            }
        )

        # ── Action space ──────────────────────────────────────────────────────
        self.action_space = spaces.Dict(
            {
                "throttle": spaces.Box(0, 1, (Ne,), np.float32),
                "gimbal": spaces.Box(-1, 1, (Ng, 2), np.float32),
                "rcs": spaces.Box(0, 1, (Nr,), np.float32),
                "staging": spaces.Discrete(2),
            }
        )

    # ─────────────────────────────────────────────────────────────────────────
    # Gym interface
    # ─────────────────────────────────────────────────────────────────────────

    def reset(
        self,
        *,
        seed: int | None = None,
        options: dict[str, Any] | None = None,
    ) -> tuple[dict[str, np.ndarray], dict[str, Any]]:
        super().reset(seed=seed)
        if seed is not None:
            self._rng = np.random.default_rng(seed)

        self._physics = _FallbackPhysics(self.cfg)
        self._step_count = 0

        obs = self._get_obs()
        info = self._get_info()
        return obs, info

    def step(
        self, action: dict[str, np.ndarray]
    ) -> tuple[dict[str, np.ndarray], float, bool, bool, dict[str, Any]]:
        assert self._physics is not None, "Call reset() before step()"

        throttle = float(np.mean(action["throttle"]))
        gimbal = action.get(
            "gimbal", np.zeros((self.cfg.vehicle.n_gimbals, 2), np.float32)
        )

        self._physics.step(throttle, gimbal[0] if len(gimbal) > 0 else np.zeros(2))
        self._step_count += 1

        obs = self._get_obs()
        reward = self._compute_reward(obs, action)
        done = self._is_done()
        truncated = self._step_count >= self.cfg.max_steps
        info = self._get_info()

        return obs, reward, done, truncated, info

    def render(self) -> np.ndarray | None:
        if self.render_mode == "rgb_array":
            # Return a placeholder RGB image; full SFML rendering hooks in later
            w, h = 320, 240
            img = np.zeros((h, w, 3), dtype=np.uint8)
            # Draw altitude as green bar
            p = self._physics
            if p is not None:
                alt_frac = min(p.altitude / 1000e3, 1.0)
                bar_h = int(alt_frac * h)
                img[h - bar_h :, w // 4 : w // 2, 1] = 180
            return img
        return None

    # ─────────────────────────────────────────────────────────────────────────
    # Internal helpers
    # ─────────────────────────────────────────────────────────────────────────

    def _get_obs(self) -> dict[str, np.ndarray]:
        p = self._physics
        assert p is not None

        # Noisy sensor readings
        accel_noise = self._rng.normal(0, self.cfg.imu_accel_noise, 3)
        gyro_noise = self._rng.normal(0, self.cfg.imu_gyro_noise, 3)
        gps_noise = self._rng.normal(0, self.cfg.gps_pos_noise, 3)

        # Approximate orbital elements from Cartesian state
        r_mag = np.linalg.norm(p.pos)
        v_mag = np.linalg.norm(p.vel)
        energy = 0.5 * v_mag**2 - MU_EARTH / r_mag
        a = -MU_EARTH / (2 * energy) if abs(energy) > 1e-10 else 1e30
        e_vec = np.cross(p.vel, np.cross(p.pos, p.vel)) / MU_EARTH - p.pos / r_mag
        e = float(np.linalg.norm(e_vec))

        # Circular orbit period-based time to apo/peri (simplified)
        T = 2 * math.pi * math.sqrt(max(a, 1.0) ** 3 / MU_EARTH)
        phase = math.atan2(p.pos[1], p.pos[0]) % (2 * math.pi)
        t_apo = max(0.0, T * (math.pi - phase) / (2 * math.pi))
        t_peri = max(0.0, T * (2 * math.pi - phase) / (2 * math.pi))

        fuel_frac = np.array(
            [p.fuel / max(self.cfg.vehicle.fuel_mass_kg, 1.0)], dtype=np.float32
        )

        return {
            "position_eci": p.pos.astype(np.float64),
            "velocity_eci": p.vel.astype(np.float64),
            "altitude": np.array([p.altitude], dtype=np.float64),
            "orbital_elements": np.array(
                [a, e, 0.0, 0.0, 0.0, phase], dtype=np.float64
            ),
            "quaternion": p.quat.astype(np.float64),
            "angular_velocity": (p.omega + gyro_noise).astype(np.float32),
            "mass": np.array([p.mass], dtype=np.float64),
            "fuel_fraction": fuel_frac,
            "engine_throttle": np.zeros(self.cfg.vehicle.n_engines, dtype=np.float32),
            "temperature_map": np.array([293.15], dtype=np.float32),
            "target_position": np.array(
                [R_EARTH + self.cfg.target_altitude_m, 0, 0], dtype=np.float64
            ),
            "target_velocity": np.array(
                [0, math.sqrt(MU_EARTH / (R_EARTH + self.cfg.target_altitude_m)), 0],
                dtype=np.float64,
            ),
            "time_to_apoapsis": np.array([t_apo], dtype=np.float64),
            "time_to_periapsis": np.array([t_peri], dtype=np.float64),
            "imu_accel": (self._gravity_accel(p.pos) + accel_noise).astype(np.float32),
            "imu_gyro": (p.omega + gyro_noise).astype(np.float32),
            "gps_position": (p.pos + gps_noise).astype(np.float64),
        }

    def _gravity_accel(self, pos: np.ndarray) -> np.ndarray:
        r3 = np.linalg.norm(pos) ** 3
        return -MU_EARTH / r3 * pos

    def _compute_reward(
        self,
        obs: dict[str, np.ndarray],
        action: dict[str, np.ndarray],
    ) -> float:
        p = self._physics
        assert p is not None

        # Survival: +1 per step if not crashed
        reward = 1.0

        # Altitude penalty: below 100 km → negative
        alt = p.altitude
        if alt < 100e3:
            reward -= 10.0
        elif alt < 150e3:
            reward -= 5.0

        # Orbit precision: penalise deviation from target altitude
        target_r = R_EARTH + self.cfg.target_altitude_m
        dr = abs(np.linalg.norm(p.pos) - target_r) / target_r
        reward -= dr * 5.0

        # Fuel efficiency: small penalty for burning fuel
        throttle = float(np.mean(action["throttle"]))
        reward -= throttle * 0.05

        return float(reward)

    def _is_done(self) -> bool:
        p = self._physics
        assert p is not None
        # Episode ends on crash or escape
        alt = p.altitude
        return alt < 0 or alt > 1e9

    def _get_info(self) -> dict[str, Any]:
        p = self._physics
        assert p is not None
        return {
            "altitude_km": p.altitude / 1000.0,
            "speed_kms": p.speed / 1000.0,
            "fuel_remaining": p.fuel,
            "sim_time_s": p.t,
            "step": self._step_count,
        }
