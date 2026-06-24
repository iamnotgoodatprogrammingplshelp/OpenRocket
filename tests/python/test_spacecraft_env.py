"""Pytest suite for the SpacecraftEnv Gymnasium environment."""

import numpy as np
import pytest

import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../..'))

from ai.env import SpacecraftEnv, ScenarioConfig, VehicleConfig
from ai.rewards import (
    RewardComposer, OrbitalPrecisionReward,
    FuelEfficiencyReward, SurvivalReward,
)


# ─────────────────────────────────────────────────────────────────────────────
# Environment construction
# ─────────────────────────────────────────────────────────────────────────────

def test_env_creates_valid_spaces():
    env = SpacecraftEnv()
    assert env.observation_space is not None
    assert env.action_space is not None


def test_reset_returns_valid_obs():
    env = SpacecraftEnv()
    obs, info = env.reset(seed=0)
    assert isinstance(obs, dict)
    for key in ["position_eci", "velocity_eci", "altitude", "quaternion"]:
        assert key in obs, f"Missing key: {key}"
    assert isinstance(info, dict)


def test_obs_within_bounds():
    env = SpacecraftEnv()
    obs, _ = env.reset(seed=42)
    for key, space in env.observation_space.spaces.items():
        val = obs[key]
        assert space.contains(val), \
            f"obs['{key}'] = {val} is outside bounds {space}"


# ─────────────────────────────────────────────────────────────────────────────
# Step
# ─────────────────────────────────────────────────────────────────────────────

def test_step_with_zero_action():
    env = SpacecraftEnv()
    obs, _ = env.reset(seed=0)
    action = env.action_space.sample()
    # Zero throttle
    action["throttle"][:] = 0.0
    next_obs, reward, done, truncated, info = env.step(action)
    assert isinstance(reward, float)
    assert isinstance(done, bool)
    assert "altitude_km" in info


def test_step_advances_sim_time():
    env = SpacecraftEnv()
    obs, _ = env.reset(seed=0)
    t0 = obs["position_eci"].copy()
    action = env.action_space.sample()
    action["throttle"][:] = 0.0
    next_obs, _, _, _, _ = env.step(action)
    # Position must change (orbit propagated)
    assert not np.allclose(next_obs["position_eci"], t0)


def test_step_count_truncation():
    cfg = ScenarioConfig(max_steps=3, dt_s=1.0)
    env = SpacecraftEnv(scenario=cfg)
    obs, _ = env.reset(seed=0)
    truncated = False
    for _ in range(10):
        action = env.action_space.sample()
        _, _, done, truncated, _ = env.step(action)
        if truncated or done:
            break
    assert truncated  # should truncate after max_steps


# ─────────────────────────────────────────────────────────────────────────────
# Reward functions
# ─────────────────────────────────────────────────────────────────────────────

def test_survival_reward_in_orbit():
    env = SpacecraftEnv()
    obs, _ = env.reset(seed=0)
    reward = SurvivalReward().compute(obs, np.zeros(1), {})
    assert reward == 1.0  # 400 km orbit is above 50 km threshold


def test_orbital_precision_reward_at_target():
    env = SpacecraftEnv()
    obs, _ = env.reset(seed=0)
    r = OrbitalPrecisionReward(target_altitude_m=400e3)
    reward = r.compute(obs, np.zeros(1), {})
    # At circular LEO the reward should be close to 0 (small error)
    assert reward > -1.0  # within 10 km → reward > -1


def test_reward_composer_sums():
    env = SpacecraftEnv()
    obs, _ = env.reset(seed=0)
    composer = RewardComposer([
        (SurvivalReward(), 1.0),
        (OrbitalPrecisionReward(), 0.5),
    ])
    total = composer.compute(obs, np.zeros(1), {})
    assert isinstance(total, float)


def test_fuel_efficiency_reward_decreases_on_burn():
    r = FuelEfficiencyReward()
    r.reset()
    obs1 = {"fuel_fraction": np.array([1.0])}
    obs2 = {"fuel_fraction": np.array([0.9])}
    r.compute(obs1, np.zeros(1), {})
    reward = r.compute(obs2, np.zeros(1), {})
    assert reward < 0.0  # fuel burned → penalty


# ─────────────────────────────────────────────────────────────────────────────
# Determinism
# ─────────────────────────────────────────────────────────────────────────────

def test_reset_same_seed_is_deterministic():
    env1 = SpacecraftEnv()
    env2 = SpacecraftEnv()
    obs1, _ = env1.reset(seed=99)
    obs2, _ = env2.reset(seed=99)
    for key in obs1:
        np.testing.assert_allclose(obs1[key], obs2[key], err_msg=f"key={key}")


def test_render_rgb_array():
    env = SpacecraftEnv(render_mode="rgb_array")
    env.reset(seed=0)
    frame = env.render()
    assert frame is not None
    assert frame.shape == (240, 320, 3)
    assert frame.dtype == np.uint8
