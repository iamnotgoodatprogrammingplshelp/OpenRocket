/**
 * @file headless_main.cpp
 * @brief Phase 1 headless validation — runs physics without any graphics.
 *
 * Runs a circular LEO spacecraft for one full orbital period and reports
 * position drift and energy conservation. Used as the Phase 1 CI smoke test.
 */

#include "core/physics/PhysicsWorld.hpp"
#include "core/physics/OrbitalMechanics.hpp"
#include "core/world/CelestialBody.hpp"
#include <print>
#include <cmath>

using namespace openorbit::physics;
using namespace openorbit::world;

int main() {
    std::print("OpenOrbit headless validation\n");
    std::print("─────────────────────────────\n");

    constexpr double kMu    = 3.986004418e14;
    constexpr double kRE    = 6.3781e6;
    constexpr double kAlt   = 400e3;
    constexpr double kR_LEO = kRE + kAlt;

    // ── Set up world ─────────────────────────────────────────────────────────
    PhysicsWorld world(IntegratorType::RK4);
    auto earth = makeEarth();
    world.addGravBody(earth.toGravBody());
    world.setPrimaryBodyName("Earth");

    double v0 = circularOrbitSpeed(kR_LEO, kMu);
    StateVector s0;
    s0.position = {kR_LEO, 0.0, 0.0};
    s0.velocity = {0.0, v0, 0.0};
    s0.mass_kg  = 1000.0;

    InertiaProperties inertia;
    inertia.mass_kg = 1000.0;
    inertia.inertia = Eigen::Matrix3d::Identity() * 500.0;

    auto id = world.addEntity(s0, inertia);

    double period = orbitalPeriod(kR_LEO, kMu);
    std::print("Orbital period:  {:.2f} s\n", period);
    std::print("Circular speed:  {:.2f} m/s\n", v0);

    // ── Integrate one full period at 1 s steps ────────────────────────────────
    double dt    = 1.0;
    int    steps = static_cast<int>(period / dt);

    for (int i = 0; i < steps; ++i)
        world.step(dt, 1);

    StateVector s1 = world.getState(id);

    double drift  = (s1.position - s0.position).norm();
    double E0     = 0.5 * v0 * v0 - kMu / kR_LEO;
    double v1     = s1.velocity.norm();
    double r1     = s1.position.norm();
    double E1     = 0.5 * v1 * v1 - kMu / r1;
    double dE_rel = std::abs((E1 - E0) / E0);

    std::print("Position drift:  {:.4f} m   (spec: < 0.1 m)\n", drift);
    std::print("Energy error:    {:.2e}   (spec: < 1e-6)\n",     dE_rel);
    std::print("Altitude:        {:.2f} km\n", world.getAltitude(id) / 1000.0);

    bool ok = (drift < 0.1) && (dE_rel < 1e-6);
    std::print("Phase 1 check:   {}\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
