#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "core/physics/OrbitalMechanics.hpp"
#include "core/physics/Integrators.hpp"
#include "core/physics/PhysicsWorld.hpp"
#include "core/world/CelestialBody.hpp"
#include <cmath>
#include <numbers>

using namespace openorbit::physics;
using namespace Catch::Matchers;

// Earth constants (WGS-84 compatible)
static constexpr double kMuEarth   = 3.986004418e14;  // m³/s²
static constexpr double kREarth    = 6.3781e6;         // m
static constexpr double kAlt_LEO   = 400e3;             // 400 km
static constexpr double kR_LEO     = kREarth + kAlt_LEO;

// ─────────────────────────────────────────────────────────────────────────────
// Helper: create a perfect circular LEO state
// ─────────────────────────────────────────────────────────────────────────────

StateVector makeCircularLEO(double r = kR_LEO) {
    double v = circularOrbitSpeed(r, kMuEarth);
    StateVector s;
    s.position = {r, 0.0, 0.0};
    s.velocity = {0.0, v,  0.0};
    s.mass_kg  = 1000.0;
    s.fuel_kg  = 0.0;
    return s;
}

// ─────────────────────────────────────────────────────────────────────────────
// Gravity utilities
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("circularOrbitSpeed matches vis-viva", "[orbital_mechanics]") {
    double r = kR_LEO;
    double v = circularOrbitSpeed(r, kMuEarth);
    // Vis-viva for circular: v = sqrt(mu/r)
    double expected = std::sqrt(kMuEarth / r);
    REQUIRE_THAT(v, WithinRel(expected, 1e-12));
}

TEST_CASE("orbitalPeriod matches analytical", "[orbital_mechanics]") {
    // ISS-like orbit: 400 km altitude
    double period = orbitalPeriod(kR_LEO, kMuEarth);
    // T = 2π sqrt(a³/μ)
    double expected = 2.0 * std::numbers::pi * std::sqrt(kR_LEO*kR_LEO*kR_LEO / kMuEarth);
    REQUIRE_THAT(period, WithinRel(expected, 1e-10));
    // Should be ~5553 s
    REQUIRE(period > 5500.0);
    REQUIRE(period < 5600.0);
}

TEST_CASE("computeGravity at LEO matches analytical centripetal", "[orbital_mechanics]") {
    GravBody earth{"Earth", kMuEarth, kREarth, Eigen::Vector3d::Zero()};
    Eigen::Vector3d pos{kR_LEO, 0.0, 0.0};
    Eigen::Vector3d g = computeGravity(pos, std::span<const GravBody>(&earth, 1));
    // Should point toward origin with magnitude mu/r²
    double expected_mag = kMuEarth / (kR_LEO * kR_LEO);
    REQUIRE_THAT(g.norm(), WithinRel(expected_mag, 1e-10));
    REQUIRE(g.x() < 0.0);   // points toward Earth
    REQUIRE_THAT(g.y(), WithinAbs(0.0, 1e-10));
    REQUIRE_THAT(g.z(), WithinAbs(0.0, 1e-10));
}

TEST_CASE("N-body: Moon adds perturbation to Earth gravity", "[orbital_mechanics]") {
    GravBody earth{"Earth", kMuEarth, kREarth, Eigen::Vector3d::Zero()};
    GravBody moon{"Moon", 4.9048695e12, 1.7374e6, Eigen::Vector3d{3.844e8, 0.0, 0.0}};
    std::array<GravBody,2> bodies{earth, moon};

    Eigen::Vector3d pos{kR_LEO, 0.0, 0.0};
    Eigen::Vector3d g_both = computeGravity(pos, std::span<const GravBody>(bodies));
    Eigen::Vector3d g_earth = computeGravity(pos, std::span<const GravBody>(&earth, 1));

    // Moon should add a small (positive x) perturbation (pulls toward Moon)
    double dx = g_both.x() - g_earth.x();
    REQUIRE(dx > 0.0);
    REQUIRE(std::abs(dx) < 1e-4); // moon perturbation is tiny ~5e-6 m/s²
}

// ─────────────────────────────────────────────────────────────────────────────
// Kepler equation solver
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("solveKepler: circular orbit E=M", "[orbital_mechanics]") {
    for (double M : {0.0, 0.5, 1.0, 2.0, std::numbers::pi}) {
        double E = solveKepler(M, 0.0);
        REQUIRE_THAT(E, WithinAbs(M, 1e-12));
    }
}

TEST_CASE("solveKepler: high eccentricity convergence", "[orbital_mechanics]") {
    // e=0.9, M=1.0 rad — notoriously difficult
    double E = solveKepler(1.0, 0.9);
    double check = E - 0.9 * std::sin(E);
    REQUIRE_THAT(check, WithinAbs(1.0, 1e-12));
}

TEST_CASE("solveKepler satisfies Kepler equation for range of e and M", "[orbital_mechanics]") {
    for (double e : {0.0, 0.1, 0.3, 0.5, 0.7, 0.9}) {
        for (double M : {0.01, 0.5, 1.5, 3.0, 5.0}) {
            double E = solveKepler(M, e);
            double residual = E - e * std::sin(E) - M;
            INFO("e=" << e << " M=" << M << " E=" << E);
            REQUIRE_THAT(residual, WithinAbs(0.0, 1e-11));
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Orbital element round-trip
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("stateToElements → elementsToState round-trip: circular LEO", "[orbital_mechanics]") {
    Eigen::Vector3d r0{kR_LEO, 0.0, 0.0};
    double v_circ = circularOrbitSpeed(kR_LEO, kMuEarth);
    Eigen::Vector3d v0{0.0, v_circ, 0.0};

    OrbitalElements el = stateToElements(r0, v0, kMuEarth);

    REQUIRE_THAT(el.a, WithinRel(kR_LEO, 1e-8));
    REQUIRE_THAT(el.e, WithinAbs(0.0, 1e-10));

    Eigen::Vector3d r1, v1;
    elementsToState(el, kMuEarth, r1, v1);

    REQUIRE_THAT((r1-r0).norm(), WithinAbs(0.0, 1e-3));  // < 1 mm
    REQUIRE_THAT((v1-v0).norm(), WithinAbs(0.0, 1e-6));  // < 1 µm/s
}

TEST_CASE("stateToElements → elementsToState round-trip: elliptic orbit", "[orbital_mechanics]") {
    // Hohmann transfer: apoapsis at GEO altitude
    double r_pe = kREarth + 200e3;
    double r_ap = kREarth + 35786e3; // GEO
    double a    = (r_pe + r_ap) / 2.0;
    double e    = (r_ap - r_pe) / (r_ap + r_pe);
    double v_pe = std::sqrt(kMuEarth * (2.0/r_pe - 1.0/a));

    Eigen::Vector3d r0{r_pe, 0.0, 0.0};
    Eigen::Vector3d v0{0.0, v_pe, 0.0};

    OrbitalElements el = stateToElements(r0, v0, kMuEarth);

    REQUIRE_THAT(el.a, WithinRel(a, 1e-8));
    REQUIRE_THAT(el.e, WithinRel(e, 1e-8));

    Eigen::Vector3d r1, v1;
    elementsToState(el, kMuEarth, r1, v1);

    REQUIRE_THAT((r1-r0).norm(), WithinAbs(0.0, 1.0)); // < 1 m
    REQUIRE_THAT((v1-v0).norm(), WithinAbs(0.0, 1e-4));
}

// ─────────────────────────────────────────────────────────────────────────────
// RK4 integrator: LEO orbit closure test
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("RK4: circular LEO drifts < 0.1 m over one orbital period", "[integrators]") {
    double r = kR_LEO;
    StateVector s0 = makeCircularLEO(r);

    GravBody earth{"Earth", kMuEarth, kREarth, Eigen::Vector3d::Zero()};
    std::vector<GravBody> bodies{earth};

    auto derivFn = [&](double /*t*/, const StateVector& s) -> StateDerivative {
        StateDerivative d;
        d.dposition = s.velocity;
        d.dvelocity = computeGravity(s.position, std::span<const GravBody>(bodies));
        return d;
    };

    double T = orbitalPeriod(r, kMuEarth);
    double dt = 1.0;  // 1 second fixed step
    int n_steps = static_cast<int>(T / dt);

    StateVector s = s0;
    double t = 0.0;
    for (int i = 0; i < n_steps; ++i) {
        s = stepRK4(derivFn, s, t, dt);
        t += dt;
    }

    double pos_drift_m = (s.position - s0.position).norm();
    double vel_drift = (s.velocity - s0.velocity).norm();

    INFO("Period=" << T << "s, dt=" << dt << "s, drift=" << pos_drift_m << "m");
    REQUIRE(pos_drift_m < 0.1);   // spec: < 0.1 m per orbit
    REQUIRE(vel_drift < 0.001);    // < 1 mm/s velocity drift
}

TEST_CASE("RK4: energy conservation over one orbit (< 1e-6 relative)", "[integrators]") {
    double r = kR_LEO;
    StateVector s0 = makeCircularLEO(r);

    GravBody earth{"Earth", kMuEarth, kREarth, Eigen::Vector3d::Zero()};
    std::vector<GravBody> bodies{earth};

    auto derivFn = [&](double, const StateVector& s) -> StateDerivative {
        StateDerivative d;
        d.dposition = s.velocity;
        d.dvelocity = computeGravity(s.position, std::span<const GravBody>(bodies));
        return d;
    };

    auto specificEnergy = [&](const StateVector& s) {
        return 0.5 * s.velocity.squaredNorm() - kMuEarth / s.position.norm();
    };

    double E0 = specificEnergy(s0);
    double T  = orbitalPeriod(r, kMuEarth);
    double dt = 1.0;

    StateVector s = s0;
    double t = 0.0;
    for (int i = 0; i < static_cast<int>(T); ++i) {
        s = stepRK4(derivFn, s, t, dt);
        t += dt;
    }

    double E1 = specificEnergy(s);
    double rel_err = std::abs((E1 - E0) / E0);
    INFO("E0=" << E0 << " E1=" << E1 << " rel_err=" << rel_err);
    REQUIRE(rel_err < 1e-6);
}

// ─────────────────────────────────────────────────────────────────────────────
// RK45 adaptive integrator test
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("RK45: circular LEO drifts < 0.01 m over one orbital period", "[integrators]") {
    double r = kR_LEO;
    StateVector s0 = makeCircularLEO(r);

    GravBody earth{"Earth", kMuEarth, kREarth, Eigen::Vector3d::Zero()};
    std::vector<GravBody> bodies{earth};

    auto derivFn = [&](double, const StateVector& s) -> StateDerivative {
        StateDerivative d;
        d.dposition = s.velocity;
        d.dvelocity = computeGravity(s.position, std::span<const GravBody>(bodies));
        return d;
    };

    double T  = orbitalPeriod(r, kMuEarth);
    double t  = 0.0;
    double dt = 10.0;
    StateVector s = s0;

    while (t < T) {
        double step = std::min(dt, T - t);
        auto result = stepRK45(derivFn, s, t, step, 1e-10, 1e-10);
        s  = result.state;
        dt = result.dt_next;
        t += result.dt_used;
    }

    double drift = (s.position - s0.position).norm();
    INFO("RK45 drift=" << drift << "m over T=" << T << "s");
    REQUIRE(drift < 0.01);
}

// ─────────────────────────────────────────────────────────────────────────────
// Verlet integrator test
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("Verlet: long-term energy stability over 100 orbits", "[integrators]") {
    double r = kR_LEO;
    StateVector s0 = makeCircularLEO(r);

    GravBody earth{"Earth", kMuEarth, kREarth, Eigen::Vector3d::Zero()};
    std::vector<GravBody> bodies{earth};

    auto derivFn = [&](double, const StateVector& s) -> StateDerivative {
        StateDerivative d;
        d.dposition = s.velocity;
        d.dvelocity = computeGravity(s.position, std::span<const GravBody>(bodies));
        return d;
    };

    auto specificEnergy = [&](const StateVector& s) {
        return 0.5 * s.velocity.squaredNorm() - kMuEarth / s.position.norm();
    };

    double E0 = specificEnergy(s0);
    double T  = orbitalPeriod(r, kMuEarth);
    double dt = 1.0;

    StateVector s = s0;
    double t = 0.0;
    for (int i = 0; i < static_cast<int>(100.0 * T); ++i) {
        s = stepVerlet(derivFn, s, t, dt);
        t += dt;
    }

    double E1 = specificEnergy(s);
    double rel_err = std::abs((E1 - E0) / E0);
    INFO("Verlet 100-orbit energy rel_err=" << rel_err);
    // Verlet is symplectic — energy oscillates but does not drift secularly
    REQUIRE(rel_err < 1e-4);
}

// ─────────────────────────────────────────────────────────────────────────────
// PhysicsWorld integration test
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("PhysicsWorld: LEO orbit closes within 1 m over one period", "[physics_world]") {
    PhysicsWorld world(IntegratorType::RK4);

    auto earth = openorbit::world::makeEarth();
    world.addGravBody(earth.toGravBody());
    world.setPrimaryBodyName("Earth");

    StateVector s0 = makeCircularLEO();
    InertiaProperties inertia;
    inertia.mass_kg = 1000.0;
    inertia.inertia = Eigen::Matrix3d::Identity() * 500.0;

    EntityID id = world.addEntity(s0, inertia);

    double T  = orbitalPeriod(kR_LEO, kMuEarth);
    double dt = 1.0;
    int n = static_cast<int>(T / dt);

    for (int i = 0; i < n; ++i)
        world.step(dt, 1);

    StateVector s1 = world.getState(id);
    double drift = (s1.position - s0.position).norm();
    INFO("PhysicsWorld orbit drift=" << drift << "m");
    REQUIRE(drift < 1.0); // 1 m tolerance including integrator overhead

    double alt = world.getAltitude(id);
    REQUIRE_THAT(alt, WithinRel(kAlt_LEO, 1e-4)); // altitude should match
}

TEST_CASE("PhysicsWorld: time warp x100 matches 1x within 10 m", "[physics_world]") {
    PhysicsWorld world1(IntegratorType::RK4);
    PhysicsWorld world100(IntegratorType::RK4);

    GravBody earth{"Earth", kMuEarth, kREarth, Eigen::Vector3d::Zero()};
    world1.addGravBody(earth);
    world100.addGravBody(earth);

    StateVector s0 = makeCircularLEO();
    InertiaProperties inertia;
    inertia.mass_kg = 1000.0;
    inertia.inertia = Eigen::Matrix3d::Identity() * 500.0;

    EntityID id1   = world1.addEntity(s0, inertia);
    EntityID id100 = world100.addEntity(s0, inertia);

    // Propagate 100 seconds at 1x (100 steps of 1 s)
    for (int i = 0; i < 100; ++i)
        world1.step(1.0, 1);

    // Same 100 simulated seconds at 100x warp (1 step of 1 s, warp=100)
    world100.step(1.0, 100);

    StateVector s1   = world1.getState(id1);
    StateVector s100 = world100.getState(id100);

    double pos_diff = (s1.position - s100.position).norm();
    INFO("1x vs 100x warp position diff=" << pos_diff << "m");
    REQUIRE(pos_diff < 10.0);
}

// ─────────────────────────────────────────────────────────────────────────────
// Quaternion kinematics
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("quaternionRate: zero angular velocity gives zero rate", "[orbital_mechanics]") {
    Eigen::Quaterniond q = Eigen::Quaterniond::Identity();
    Eigen::Vector3d omega = Eigen::Vector3d::Zero();
    auto dq = quaternionRate(q, omega);
    REQUIRE_THAT(dq.norm(), WithinAbs(0.0, 1e-15));
}

TEST_CASE("quaternionRate: pure spin around z gives correct rate", "[orbital_mechanics]") {
    Eigen::Quaterniond q = Eigen::Quaterniond::Identity();
    Eigen::Vector3d omega{0.0, 0.0, 1.0}; // 1 rad/s around z
    auto dq = quaternionRate(q, omega);
    // dq/dt = 0.5 * q ⊗ [0,ω] = 0.5 * [0,0,0,1] (pure z rotation)
    REQUIRE_THAT(dq.w(), WithinAbs(0.0,  1e-15));
    REQUIRE_THAT(dq.x(), WithinAbs(0.0,  1e-15));
    REQUIRE_THAT(dq.y(), WithinAbs(0.0,  1e-15));
    REQUIRE_THAT(dq.z(), WithinAbs(0.5,  1e-15));
}
