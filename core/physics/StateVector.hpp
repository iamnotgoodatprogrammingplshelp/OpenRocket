#pragma once
#include <Eigen/Dense>
#include <cstdint>
#include <array>

namespace openorbit::physics {

/// Unique identifier for any simulated entity (spacecraft, debris, etc.)
using EntityID = std::uint64_t;
constexpr EntityID INVALID_ENTITY = 0;

/**
 * @brief Full 13-DOF rigid-body state vector.
 *
 * Position and velocity are in the Earth-Centred Inertial (ECI) frame using
 * double precision to maintain sub-metre accuracy at orbital scales.
 * Attitude is represented as a unit quaternion (body → inertial).
 * Angular velocity is expressed in the body frame.
 */
struct StateVector {
    // ── Translational state (ECI, metres / metres·s⁻¹) ──────────────────────
    Eigen::Vector3d position{0, 0, 0};   ///< ECI position  [m]
    Eigen::Vector3d velocity{0, 0, 0};   ///< ECI velocity  [m/s]

    // ── Rotational state ─────────────────────────────────────────────────────
    Eigen::Quaterniond attitude{1, 0, 0, 0}; ///< body→inertial quaternion (unit)
    Eigen::Vector3d    omega{0, 0, 0};       ///< angular velocity, body frame [rad/s]

    // ── Mass state ───────────────────────────────────────────────────────────
    double mass_kg   = 1.0;   ///< total vehicle mass (dry + propellant) [kg]
    double fuel_kg   = 0.0;   ///< remaining propellant mass [kg]

    // ── Convenience ──────────────────────────────────────────────────────────
    [[nodiscard]] double altitude(double planet_radius_m) const noexcept {
        return position.norm() - planet_radius_m;
    }

    [[nodiscard]] double speed() const noexcept {
        return velocity.norm();
    }

    /// Normalise quaternion in-place (guards against numerical drift)
    void normaliseAttitude() noexcept {
        attitude.normalize();
    }

    /// Flat 13-element array representation for integrators
    using Flat = std::array<double, 13>;

    [[nodiscard]] Flat toFlat() const noexcept {
        return {
            position.x(), position.y(), position.z(),
            velocity.x(), velocity.y(), velocity.z(),
            attitude.w(), attitude.x(), attitude.y(), attitude.z(),
            omega.x(),    omega.y(),    omega.z()
        };
    }

    static StateVector fromFlat(const Flat& f, double mass_kg, double fuel_kg) noexcept {
        StateVector s;
        s.position  = {f[0], f[1], f[2]};
        s.velocity  = {f[3], f[4], f[5]};
        s.attitude  = Eigen::Quaterniond{f[6], f[7], f[8], f[9]};
        s.omega     = {f[10], f[11], f[12]};
        s.mass_kg   = mass_kg;
        s.fuel_kg   = fuel_kg;
        s.normaliseAttitude();
        return s;
    }
};

/**
 * @brief Derivative of the state vector, returned by force models.
 *
 * Stores d/dt of each state component for use by Runge-Kutta integrators.
 */
struct StateDerivative {
    Eigen::Vector3d dposition{0, 0, 0};  ///< velocity  [m/s]
    Eigen::Vector3d dvelocity{0, 0, 0};  ///< acceleration [m/s²]
    Eigen::Quaterniond dattitude{0, 0, 0, 0}; ///< quaternion rate
    Eigen::Vector3d domega{0, 0, 0};     ///< angular acceleration [rad/s²]
    double          dmass{0};            ///< mass flow rate (negative = propellant consumption) [kg/s]

    StateDerivative& operator+=(const StateDerivative& rhs) noexcept {
        dposition  += rhs.dposition;
        dvelocity  += rhs.dvelocity;
        dattitude.coeffs() += rhs.dattitude.coeffs();
        domega     += rhs.domega;
        dmass      += rhs.dmass;
        return *this;
    }

    StateDerivative operator*(double scalar) const noexcept {
        StateDerivative r;
        r.dposition  = dposition  * scalar;
        r.dvelocity  = dvelocity  * scalar;
        r.dattitude.coeffs() = dattitude.coeffs() * scalar;
        r.domega     = domega     * scalar;
        r.dmass      = dmass      * scalar;
        return r;
    }
};

/// Apply a derivative to a state over time step dt
[[nodiscard]] inline StateVector applyDerivative(
        const StateVector& s, const StateDerivative& d, double dt) noexcept
{
    StateVector out;
    out.position  = s.position + d.dposition * dt;
    out.velocity  = s.velocity + d.dvelocity * dt;

    // Quaternion integration: q_new = q + 0.5 * q * [0, ω] * dt
    out.attitude.coeffs() = s.attitude.coeffs() + d.dattitude.coeffs() * dt;
    out.omega     = s.omega    + d.domega    * dt;
    out.mass_kg   = s.mass_kg  + d.dmass     * dt;
    out.fuel_kg   = s.fuel_kg  + d.dmass     * dt;  // dmass is negative
    out.normaliseAttitude();
    return out;
}

} // namespace openorbit::physics
