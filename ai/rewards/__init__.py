"""openorbit.rewards — Composable reward functions."""
from .reward_composer import (
    RewardComposer, RewardPlugin,
    OrbitalPrecisionReward, FuelEfficiencyReward,
    SurvivalReward, SmoothControlReward, AtmosphericEntryReward,
)
__all__ = [
    "RewardComposer", "RewardPlugin",
    "OrbitalPrecisionReward", "FuelEfficiencyReward",
    "SurvivalReward", "SmoothControlReward", "AtmosphericEntryReward",
]
