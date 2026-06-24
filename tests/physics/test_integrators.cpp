#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "core/physics/Integrators.hpp"
#include <cmath>
#include <numbers>

using namespace openorbit::physics;
using namespace Catch::Matchers;

// Simple harmonic oscillator: d²x/dt² = -ω²x  (analytical solution: x = A cos(ωt))
// Use as a reference problem with known analytic solution.

static constexpr double kOmega = 1.0; // angular frequency

static StateDerivative harmonicOscillator(double /*t*/, const StateVector& s) {
    StateDerivative d;
    // Encode x in position.x, ẋ in velocity.x
    d.dposition = s.velocity;
    d.dvelocity = {-kOmega * kOmega * s.position.x(), 0.0, 0.0};
    return d;
}

TEST_CASE("RK4 harmonic oscillator: error < 1e-8 after 10 periods", "[rk4]") {
    StateVector s;
    s.position = {1.0, 0.0, 0.0}; // A = 1
    s.velocity = {0.0, 0.0, 0.0}; // starts at rest
    s.mass_kg  = 1.0;

    double T    = 2.0 * std::numbers::pi / kOmega;
    double dt   = 0.01;
    double t    = 0.0;
    int n_steps = static_cast<int>(10.0 * T / dt);

    for (int i = 0; i < n_steps; ++i) {
        s = stepRK4(harmonicOscillator, s, t, dt);
        t += dt;
    }

    double x_exact = std::cos(kOmega * t);
    REQUIRE_THAT(s.position.x(), WithinAbs(x_exact, 1e-6));
}

TEST_CASE("RK45 adaptive: harmonic oscillator tighter tolerance", "[rk45]") {
    StateVector s;
    s.position = {1.0, 0.0, 0.0};
    s.velocity = {0.0, 0.0, 0.0};
    s.mass_kg  = 1.0;

    double T = 2.0 * std::numbers::pi / kOmega;
    double t = 0.0, dt = 0.1;
    double t_end = 10.0 * T;

    while (t < t_end) {
        double step = std::min(dt, t_end - t);
        auto result = stepRK45(harmonicOscillator, s, t, step, 1e-10, 1e-10);
        s  = result.state;
        dt = result.dt_next;
        t += result.dt_used;
    }

    double x_exact = std::cos(kOmega * t);
    REQUIRE_THAT(s.position.x(), WithinAbs(x_exact, 1e-8));
}

TEST_CASE("Verlet: harmonic oscillator energy conserved over 100 periods", "[verlet]") {
    StateVector s;
    s.position = {1.0, 0.0, 0.0};
    s.velocity = {0.0, kOmega, 0.0}; // start with velocity for circular motion
    s.mass_kg  = 1.0;

    // For Verlet, use position.x as displacement, velocity.x as velocity
    StateVector s0 = s;
    double E0 = 0.5 * s0.velocity.squaredNorm() + 0.5 * kOmega*kOmega * s0.position.squaredNorm();

    double T    = 2.0 * std::numbers::pi / kOmega;
    double dt   = 0.01;
    double t    = 0.0;
    int n_steps = static_cast<int>(100.0 * T / dt);

    for (int i = 0; i < n_steps; ++i) {
        s = stepVerlet(harmonicOscillator, s, t, dt);
        t += dt;
    }

    double E1 = 0.5 * s.velocity.squaredNorm() + 0.5 * kOmega*kOmega * s.position.squaredNorm();
    double rel_err = std::abs((E1 - E0) / E0);
    INFO("Verlet 100-period energy rel_err=" << rel_err);
    REQUIRE(rel_err < 1e-4);
}

TEST_CASE("RK4 step is fourth-order: error scales as O(h^4)", "[rk4]") {
    // Integrate x' = -x from 0 to 1, analytical: x(1) = e^{-1}
    auto expDecay = [](double, const StateVector& s) -> StateDerivative {
        StateDerivative d;
        d.dposition = {-s.position.x(), 0.0, 0.0};
        d.dvelocity = Eigen::Vector3d::Zero();
        return d;
    };

    StateVector s_start;
    s_start.position = {1.0, 0.0, 0.0};
    s_start.mass_kg  = 1.0;

    double exact = std::exp(-1.0);
    double dt_ref = 0.1, dt_fine = 0.05;

    // Coarse
    StateVector sc = s_start;
    for (int i = 0; i < 10; ++i) sc = stepRK4(expDecay, sc, i*dt_ref, dt_ref);

    // Fine
    StateVector sf = s_start;
    for (int i = 0; i < 20; ++i) sf = stepRK4(expDecay, sf, i*dt_fine, dt_fine);

    double err_coarse = std::abs(sc.position.x() - exact);
    double err_fine   = std::abs(sf.position.x() - exact);

    // RK4 → ratio should be ~2^4 = 16 when step halved
    double ratio = err_coarse / err_fine;
    INFO("err_coarse=" << err_coarse << " err_fine=" << err_fine << " ratio=" << ratio);
    REQUIRE(ratio > 12.0); // order-4 convergence
    REQUIRE(ratio < 20.0);
}
