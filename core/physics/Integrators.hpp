#pragma once
#include "StateVector.hpp"
#include <functional>
#include <expected>
#include <string>

namespace openorbit::physics {

/// Signature of the derivative function: f(t, state) → StateDerivative
using DerivativeFn = std::function<StateDerivative(double t, const StateVector& s)>;

/**
 * @brief 4th-order Runge-Kutta integrator (fixed step).
 *
 * Error order O(h⁵). Suitable for all normal flight phases when
 * h ≤ 1 s. Use RK45 for adaptive error control.
 *
 * @param fn    Derivative function
 * @param s     Current state
 * @param t     Current time [s]
 * @param dt    Step size [s]
 * @return      Integrated state at t + dt
 */
[[nodiscard]] StateVector stepRK4(
        const DerivativeFn& fn,
        const StateVector&  s,
        double              t,
        double              dt);

/**
 * @brief Dormand-Prince RK45 adaptive integrator.
 *
 * Uses the embedded 5th-order solution to estimate local truncation error and
 * automatically adjusts the step size to keep the error below the tolerance.
 *
 * Reference: Dormand & Prince (1980), J. Computational and Applied Mathematics.
 *
 * @param fn        Derivative function
 * @param s         Current state
 * @param t         Current time [s]
 * @param dt_in     Initial trial step size [s]
 * @param rtol      Relative error tolerance (default 1e-9)
 * @param atol      Absolute error tolerance (default 1e-9)
 * @param dt_min    Minimum allowed step size [s]
 * @param dt_max    Maximum allowed step size [s]
 * @return          Pair of (new state at t+dt_actual, dt_actual)
 */
struct RK45Result {
    StateVector state;
    double      dt_used;
    double      dt_next;    ///< Suggested next step size
    int         steps;      ///< Internal sub-steps taken
};

[[nodiscard]] RK45Result stepRK45(
        const DerivativeFn& fn,
        const StateVector&  s,
        double              t,
        double              dt_in,
        double              rtol    = 1e-9,
        double              atol    = 1e-9,
        double              dt_min  = 1e-6,
        double              dt_max  = 60.0);

/**
 * @brief Velocity Verlet integrator (symplectic, 2nd order).
 *
 * Conserves energy exactly for conservative (Hamiltonian) systems.
 * Preferred for long orbital propagations at low-precision requirements.
 * Only valid when the derivative function has no velocity-dependent forces
 * (i.e., ignores drag, thrust). Use RK4/RK45 for powered or atmospheric flight.
 *
 * @param fn    Derivative function (only dvelocity / acceleration used)
 * @param s     Current state
 * @param t     Current time [s]
 * @param dt    Step size [s]
 * @return      Integrated state at t + dt
 */
[[nodiscard]] StateVector stepVerlet(
        const DerivativeFn& fn,
        const StateVector&  s,
        double              t,
        double              dt);

/**
 * @brief Integrator type selector for PhysicsWorld.
 */
enum class IntegratorType {
    RK4,     ///< Fixed-step 4th-order Runge-Kutta (default)
    RK45,    ///< Dormand-Prince adaptive (precision mode)
    Verlet   ///< Symplectic Verlet (performance/long-coast mode)
};

} // namespace openorbit::physics
