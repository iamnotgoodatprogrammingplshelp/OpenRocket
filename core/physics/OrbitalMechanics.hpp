#pragma once
#include "StateVector.hpp"
#include <Eigen/Dense>
#include <vector>
#include <span>

namespace openorbit::physics {

/**
 * @brief Gravitational body for N-body computation.
 *
 * Positions are provided by the world model each timestep (JPL ephemeris or
 * simplified Keplerian propagation for non-primary bodies).
 */
struct GravBody {
    std::string     name;
    double          mu;         ///< gravitational parameter  GM  [m³/s²]
    double          radius_m;   ///< mean equatorial radius   [m]
    Eigen::Vector3d position;   ///< ECI position at current epoch [m]
};

/**
 * @brief Computes gravitational acceleration from N bodies at a given position.
 *
 * Uses the direct-summation formulation:
 *   a = Σ  GM_i * (r_i - r) / |r_i - r|³
 *
 * Softening ε prevents singularities when r → r_i (planet surface collisions
 * are handled separately by the collision subsystem).
 *
 * @param position   Query position in ECI [m]
 * @param bodies     All gravitational bodies active in the simulation
 * @param softening  Softening length [m], default = 1 m (effectively disabled for
 *                   orbital-scale distances)
 * @return           Gravitational acceleration [m/s²]
 */
[[nodiscard]] Eigen::Vector3d computeGravity(
        const Eigen::Vector3d&      position,
        std::span<const GravBody>   bodies,
        double                      softening = 1.0);

/**
 * @brief Classical Keplerian orbital elements.
 *
 * Valid for elliptic (e < 1) and hyperbolic (e > 1) orbits.
 * Uses radians throughout.
 */
struct OrbitalElements {
    double a;    ///< semi-major axis [m]  (negative for hyperbola)
    double e;    ///< eccentricity []
    double i;    ///< inclination [rad]
    double raan; ///< right ascension of ascending node Ω [rad]
    double aop;  ///< argument of periapsis ω [rad]
    double ta;   ///< true anomaly ν [rad]
};

/**
 * @brief Convert position/velocity (ECI) to Keplerian orbital elements.
 *
 * @param position  ECI position [m]
 * @param velocity  ECI velocity [m/s]
 * @param mu        Central body GM [m³/s²]
 */
[[nodiscard]] OrbitalElements stateToElements(
        const Eigen::Vector3d& position,
        const Eigen::Vector3d& velocity,
        double                 mu);

/**
 * @brief Convert Keplerian elements back to position/velocity (ECI).
 *
 * @param elements  Orbital elements (a, e, i, Ω, ω, ν)
 * @param mu        Central body GM [m³/s²]
 * @param position  [out] ECI position [m]
 * @param velocity  [out] ECI velocity [m/s]
 */
void elementsToState(
        const OrbitalElements& elements,
        double                 mu,
        Eigen::Vector3d&       position,
        Eigen::Vector3d&       velocity);

/**
 * @brief Orbital period of an elliptic orbit.
 * @param a   Semi-major axis [m]
 * @param mu  Central body GM [m³/s²]
 * @return    Period [s]
 */
[[nodiscard]] double orbitalPeriod(double a, double mu) noexcept;

/**
 * @brief Circular orbit speed at a given radius.
 * @param r   Orbital radius [m]
 * @param mu  Central body GM [m³/s²]
 * @return    Speed [m/s]
 */
[[nodiscard]] double circularOrbitSpeed(double r, double mu) noexcept;

/**
 * @brief Solve Kepler's equation M = E - e·sin(E) for eccentric anomaly E.
 *
 * Uses Newton-Raphson iteration; converges in ≤15 iterations for e < 0.999.
 *
 * @param M  Mean anomaly [rad]
 * @param e  Eccentricity
 * @return   Eccentric anomaly E [rad]
 */
[[nodiscard]] double solveKepler(double M, double e, double tol = 1e-12);

/**
 * @brief True anomaly from eccentric anomaly.
 */
[[nodiscard]] double trueFromEccentric(double E, double e) noexcept;

/**
 * @brief Compute the quaternion rate from the current attitude and angular velocity.
 *
 * dq/dt = 0.5 * q ⊗ [0, ω_body]
 *
 * @param q      Current unit quaternion (body→inertial)
 * @param omega  Angular velocity in body frame [rad/s]
 */
[[nodiscard]] Eigen::Quaterniond quaternionRate(
        const Eigen::Quaterniond& q,
        const Eigen::Vector3d&    omega) noexcept;

} // namespace openorbit::physics
