#include "core/physics/Integrators.hpp"
#include <cmath>
#include <algorithm>

namespace openorbit::physics {

// ─────────────────────────────────────────────────────────────────────────────
// Helper: weighted sum of derivatives
// ─────────────────────────────────────────────────────────────────────────────

static StateVector addDerivative(const StateVector& s, const StateDerivative& d, double dt) noexcept {
    return applyDerivative(s, d, dt);
}

// ─────────────────────────────────────────────────────────────────────────────
// RK4 (classical 4th-order)
// ─────────────────────────────────────────────────────────────────────────────
// Coefficients (Butcher tableau):
//   k1 = f(t,       y)
//   k2 = f(t+h/2,   y + h/2*k1)
//   k3 = f(t+h/2,   y + h/2*k2)
//   k4 = f(t+h,     y + h*k3)
//   y_new = y + h/6*(k1 + 2*k2 + 2*k3 + k4)

StateVector stepRK4(
        const DerivativeFn& fn,
        const StateVector&  s,
        double              t,
        double              dt)
{
    StateDerivative k1 = fn(t,          s);
    StateDerivative k2 = fn(t + dt*0.5, addDerivative(s, k1, dt*0.5));
    StateDerivative k3 = fn(t + dt*0.5, addDerivative(s, k2, dt*0.5));
    StateDerivative k4 = fn(t + dt,     addDerivative(s, k3, dt));

    StateDerivative combined;
    combined.dposition  = (k1.dposition  + 2.0*k2.dposition  + 2.0*k3.dposition  + k4.dposition)  / 6.0;
    combined.dvelocity  = (k1.dvelocity  + 2.0*k2.dvelocity  + 2.0*k3.dvelocity  + k4.dvelocity)  / 6.0;
    combined.dattitude.coeffs() =
                         (k1.dattitude.coeffs() + 2.0*k2.dattitude.coeffs()
                        + 2.0*k3.dattitude.coeffs() + k4.dattitude.coeffs()) / 6.0;
    combined.domega     = (k1.domega     + 2.0*k2.domega     + 2.0*k3.domega     + k4.domega)     / 6.0;
    combined.dmass      = (k1.dmass      + 2.0*k2.dmass      + 2.0*k3.dmass      + k4.dmass)      / 6.0;

    return applyDerivative(s, combined, dt);
}

// ─────────────────────────────────────────────────────────────────────────────
// Dormand-Prince RK45 (adaptive)
// ─────────────────────────────────────────────────────────────────────────────
// Butcher tableau — Dormand & Prince (1980), Table 1
// 7 function evaluations per step (FSAL: k7 = k1 of next step)

static constexpr double DP_C2  = 1.0/5.0;
static constexpr double DP_C3  = 3.0/10.0;
static constexpr double DP_C4  = 4.0/5.0;
static constexpr double DP_C5  = 8.0/9.0;

static constexpr double DP_A21 = 1.0/5.0;
static constexpr double DP_A31 = 3.0/40.0,    DP_A32 = 9.0/40.0;
static constexpr double DP_A41 = 44.0/45.0,   DP_A42 = -56.0/15.0,  DP_A43 = 32.0/9.0;
static constexpr double DP_A51 = 19372.0/6561.0, DP_A52 = -25360.0/2187.0, DP_A53 = 64448.0/6561.0, DP_A54 = -212.0/729.0;
static constexpr double DP_A61 = 9017.0/3168.0,  DP_A62 = -355.0/33.0,    DP_A63 = 46732.0/5247.0, DP_A64 = 49.0/176.0,    DP_A65 = -5103.0/18656.0;

// 5th-order weights (b)
static constexpr double DP_B1 = 35.0/384.0,  DP_B3 = 500.0/1113.0, DP_B4 = 125.0/192.0,
                         DP_B5 = -2187.0/6784.0, DP_B6 = 11.0/84.0;

// 4th-order weights (b*) for error estimation
static constexpr double DP_E1 =  71.0/57600.0, DP_E3 = -71.0/16695.0, DP_E4 =  71.0/1920.0,
                         DP_E5 = -17253.0/339200.0, DP_E6 = 22.0/525.0, DP_E7 = -1.0/40.0;

// Weighted sum helper for RK45 stages
static StateDerivative weightedSum6(
        const StateDerivative& k1,
        const StateDerivative& k2,
        const StateDerivative& k3,
        const StateDerivative& k4,
        const StateDerivative& k5,
        const StateDerivative& k6,
        double w1, double w2, double w3, double w4, double w5, double w6)
{
    StateDerivative r;
    r.dposition  = w1*k1.dposition  + w2*k2.dposition  + w3*k3.dposition
                 + w4*k4.dposition  + w5*k5.dposition  + w6*k6.dposition;
    r.dvelocity  = w1*k1.dvelocity  + w2*k2.dvelocity  + w3*k3.dvelocity
                 + w4*k4.dvelocity  + w5*k5.dvelocity  + w6*k6.dvelocity;
    r.dattitude.coeffs() = w1*k1.dattitude.coeffs() + w2*k2.dattitude.coeffs()
                         + w3*k3.dattitude.coeffs() + w4*k4.dattitude.coeffs()
                         + w5*k5.dattitude.coeffs() + w6*k6.dattitude.coeffs();
    r.domega    = w1*k1.domega + w2*k2.domega + w3*k3.domega
                + w4*k4.domega + w5*k5.domega + w6*k6.domega;
    r.dmass     = w1*k1.dmass  + w2*k2.dmass  + w3*k3.dmass
                + w4*k4.dmass  + w5*k5.dmass  + w6*k6.dmass;
    return r;
}

static double errorNorm(const StateDerivative& err, const StateVector& s, double dt,
                         double rtol, double atol)
{
    // RMS norm over position and velocity components (most sensitive)
    double sum = 0.0;
    int n = 0;

    auto addComp = [&](double e_val, double y_val) {
        double scale = atol + rtol * std::abs(y_val);
        sum += (e_val * dt / scale) * (e_val * dt / scale);
        ++n;
    };

    for (int i = 0; i < 3; ++i) {
        addComp(err.dposition[i], s.position[i]);
        addComp(err.dvelocity[i], s.velocity[i]);
    }
    return std::sqrt(sum / static_cast<double>(n));
}

RK45Result stepRK45(
        const DerivativeFn& fn,
        const StateVector&  s0,
        double              t,
        double              dt_in,
        double              rtol,
        double              atol,
        double              dt_min,
        double              dt_max)
{
    double dt = dt_in;
    StateVector s = s0;
    int total_steps = 0;
    double t_cur = t;

    while (true) {
        dt = std::clamp(dt, dt_min, dt_max);

        StateDerivative k1 = fn(t_cur, s);
        StateDerivative k2 = fn(t_cur + DP_C2*dt, addDerivative(s, k1 * (DP_A21*dt), 1.0));
        StateDerivative k3 = fn(t_cur + DP_C3*dt, addDerivative(s,
            weightedSum6(k1,k2,k2,k2,k2,k2, DP_A31,DP_A32,0,0,0,0) * dt, 1.0));
        StateDerivative k4 = fn(t_cur + DP_C4*dt, addDerivative(s,
            weightedSum6(k1,k2,k3,k3,k3,k3, DP_A41,DP_A42,DP_A43,0,0,0) * dt, 1.0));
        StateDerivative k5 = fn(t_cur + DP_C5*dt, addDerivative(s,
            weightedSum6(k1,k2,k3,k4,k4,k4, DP_A51,DP_A52,DP_A53,DP_A54,0,0) * dt, 1.0));
        StateDerivative k6 = fn(t_cur + dt, addDerivative(s,
            weightedSum6(k1,k2,k3,k4,k5,k5, DP_A61,DP_A62,DP_A63,DP_A64,DP_A65,0) * dt, 1.0));

        // 5th-order solution
        StateDerivative d5 = weightedSum6(k1,k2,k3,k4,k5,k6, DP_B1,0,DP_B3,DP_B4,DP_B5,DP_B6);
        StateVector s_new = applyDerivative(s, d5, dt);

        // Error estimate (difference between 4th and 5th order)
        StateDerivative k7 = fn(t_cur + dt, s_new); // FSAL
        StateDerivative err;
        err.dvelocity  = (DP_E1*k1.dvelocity + DP_E3*k3.dvelocity + DP_E4*k4.dvelocity
                        + DP_E5*k5.dvelocity + DP_E6*k6.dvelocity + DP_E7*k7.dvelocity);
        err.dposition  = err.dvelocity; // rough approximation for position component

        double err_norm = errorNorm(err, s, dt, rtol, atol);
        ++total_steps;

        if (err_norm <= 1.0 || dt <= dt_min) {
            // Accept step
            double factor = (err_norm > 1e-10)
                ? std::min(5.0, 0.9 * std::pow(1.0 / err_norm, 0.2))
                : 5.0;
            double dt_next = std::clamp(dt * factor, dt_min, dt_max);
            return RK45Result{s_new, dt, dt_next, total_steps};
        }

        // Reject: reduce step
        double factor = std::max(0.1, 0.9 * std::pow(1.0 / err_norm, 0.25));
        dt *= factor;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Velocity Verlet (symplectic)
// ─────────────────────────────────────────────────────────────────────────────
// v(t+h/2) = v(t) + h/2 * a(t)
// r(t+h)   = r(t) + h * v(t+h/2)
// a(t+h)   = f(r(t+h))
// v(t+h)   = v(t+h/2) + h/2 * a(t+h)

StateVector stepVerlet(
        const DerivativeFn& fn,
        const StateVector&  s,
        double              t,
        double              dt)
{
    StateDerivative d0 = fn(t, s);

    StateVector half;
    half.position = s.position + s.velocity * (dt * 0.5);
    half.velocity = s.velocity + d0.dvelocity * (dt * 0.5);
    half.attitude = s.attitude;
    half.omega    = s.omega;
    half.mass_kg  = s.mass_kg;
    half.fuel_kg  = s.fuel_kg;

    StateVector mid;
    mid.position  = s.position + half.velocity * dt;
    mid.velocity  = half.velocity;
    mid.attitude  = s.attitude;
    mid.omega     = s.omega;
    mid.mass_kg   = s.mass_kg;
    mid.fuel_kg   = s.fuel_kg;

    StateDerivative d1 = fn(t + dt, mid);

    StateVector out;
    out.position  = mid.position;
    out.velocity  = half.velocity + d1.dvelocity * (dt * 0.5);
    out.attitude  = s.attitude;   // Verlet doesn't integrate rotation
    out.omega     = s.omega;
    out.mass_kg   = s.mass_kg;
    out.fuel_kg   = s.fuel_kg;
    out.normaliseAttitude();
    return out;
}

} // namespace openorbit::physics
