# OpenOrbit

**Professional open-source space flight simulator with a first-class deep learning AI layer.**

Combines the rocket-building freedom of KSP with the visual fidelity of MSFS and exposes a
Gymnasium-compatible environment so AI agents can train autonomously.

## Tech Stack

| Layer | Technology |
|-------|-----------|
| Sim engine | C++23, Eigen3, nlohmann_json |
| Windowing / 2D | SFML 2.6 |
| 3D rendering | OpenGL 4.6 core, GLSL 460 |
| Build | CMake 3.28, vcpkg |
| AI layer | Python 3.12, Gymnasium, PyTorch |
| CI | GitHub Actions |

## Directory Structure

```
openorbit/
├── core/           C++ simulation engine (physics, aero, propulsion, thermal)
├── renderer/       SFML + OpenGL rendering pipeline
├── editor/         imgui-sfml Vehicle Assembly Building
├── ai/             Python Gymnasium environment + reference agents
├── assets/         3D models, textures, audio
├── tests/          Catch2 (C++) + pytest (Python)
└── docs/           MkDocs site + API reference
```

## Quick Build

```bash
# Install deps via vcpkg
cmake -B build -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
cmake --build build -j$(nproc)

# Run physics tests
cd build && ctest --output-on-failure

# Headless Phase 1 validation
./build/openorbit_headless

# Full simulator
./build/openorbit
```

## Headless mode (no GPU required)

```bash
cmake -B build-headless -DOPENOROBIT_HEADLESS_ONLY=ON
cmake --build build-headless --target openorbit_headless test_physics
./build-headless/openorbit_headless
```

Expected output:
```
Phase 1 check:   PASS
Position drift:  0.0xyz m   (spec: < 0.1 m)
Energy error:    x.xxe-07   (spec: < 1e-6)
```

## AI Quick Start

```bash
pip install gymnasium numpy stable-baselines3 torch
python -c "
from ai.env import SpacecraftEnv
from stable_baselines3 import PPO
env = SpacecraftEnv()
model = PPO('MultiInputPolicy', env, verbose=1)
model.learn(100_000)
"
```

See `docs/ai/getting_started.md` for full instructions.

## Implementation Phases

| Phase | Status | Description |
|-------|--------|-------------|
| 1 | ✅ Complete | Headless physics: N-body, RK4/RK45/Verlet, atmosphere, propulsion |
| 2 | ✅ Complete | SFML window + OpenGL 4.6 bridge, HUD instruments, telemetry graph |
| 3 | 🔄 Planned | PBR spacecraft meshes, atmospheric scattering, orbital map, VAB |
| 4 | 🔄 Planned | All 4 reference agents + SFML multi-agent viewport |
| 5 | 🔄 Planned | Modding API, planet packs, docs, public alpha |

## License

Engine core: Apache 2.0  
Assets: CC-BY-SA 4.0
