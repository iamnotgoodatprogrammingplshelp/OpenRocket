# Train Your First Agent in 10 Minutes

## Prerequisites

```bash
pip install gymnasium numpy stable-baselines3 torch
```

## Quick start

```python
from ai.env import SpacecraftEnv, ScenarioConfig, VehicleConfig
from stable_baselines3 import PPO

# Create the environment
cfg = ScenarioConfig(
    name="gravity_turn_ascent",
    initial_altitude_m=0,        # launch from sea level
    target_altitude_m=400e3,     # target LEO
    max_steps=2000,
    dt_s=0.5,
)
env = SpacecraftEnv(scenario=cfg)

# Train PPO for 500k steps (~10 minutes on a CPU)
model = PPO(
    "MultiInputPolicy",
    env,
    verbose=1,
    n_steps=2048,
    batch_size=64,
    n_epochs=10,
    learning_rate=3e-4,
    tensorboard_log="./tb_logs/",
)
model.learn(total_timesteps=500_000)
model.save("ppo_gravity_turn")

# Evaluate
obs, _ = env.reset(seed=42)
for _ in range(500):
    action, _ = model.predict(obs, deterministic=True)
    obs, reward, done, truncated, info = env.step(action)
    print(f"Alt={info['altitude_km']:.1f} km  Speed={info['speed_kms']:.2f} km/s")
    if done or truncated:
        break
```

## Observation space

See `docs/ai/observation_space.md` for the full reference with units.

## Benchmark tasks

Run a pretrained agent on all 12 benchmark tasks:

```python
from ai.benchmarks.benchmark_tasks import run_all_benchmarks
results = run_all_benchmarks(model)
print(results)
```

## Multi-agent training viewport

Launch the SFML 16-tile visualiser during training:

```bash
openorbit --ai-viewport --agents 16
```

Tiles show altitude bar, reward curve, and colour-coded status
(green=improving, red=crashed, blue=current best).
