# Orbital Mechanics — Derivations

## State Vector

The 13-DOF state vector `StateVector` encodes:

| Symbol | Description | Units |
|--------|-------------|-------|
| **r** = (x, y, z) | ECI position | m |
| **v** = (vx, vy, vz) | ECI velocity | m/s |
| **q** = (qw, qx, qy, qz) | body→inertial attitude quaternion (unit) | — |
| **ω** = (ωx, ωy, ωz) | angular velocity, body frame | rad/s |
| m | total mass (dry + propellant) | kg |

All orbital state uses `double` (64-bit) precision to maintain sub-metre
accuracy across the solar system. The renderer converts to `float` after
applying a camera-relative offset (floating-origin technique).

## N-body Gravity

The gravitational acceleration from N bodies is:

$$\mathbf{a} = \sum_{i=1}^{N} \frac{\mu_i \,(\mathbf{r}_i - \mathbf{r})}
               {|\mathbf{r}_i - \mathbf{r}|^3 + \varepsilon^2}$$

where ε = 1 m is a softening length that prevents singularities at
planetary surfaces (the collision subsystem handles those separately).

**Source:** Bate, Mueller & White, *Fundamentals of Astrodynamics*, Dover 1971, §2.2.

## Orbital Elements

Classical Keplerian elements (a, e, i, Ω, ω, ν):

| Symbol | Name | Range |
|--------|------|-------|
| a | semi-major axis [m] | > 0 (< 0 for hyperbola) |
| e | eccentricity | ≥ 0 |
| i | inclination [rad] | [0, π] |
| Ω | right ascension of ascending node [rad] | [0, 2π) |
| ω | argument of periapsis [rad] | [0, 2π) |
| ν | true anomaly [rad] | [0, 2π) |

### Cartesian → Elements

1. Specific angular momentum: **h** = **r** × **v**
2. Node vector: **N** = ẑ × **h**
3. Eccentricity vector: **e** = (**v** × **h**)/μ − **r̂**
4. Specific orbital energy: ε = ½v² − μ/r → a = −μ/(2ε)
5. Inclination: i = arccos(hₙ/|**h**|)
6. RAAN: Ω = arccos(Nₓ/|**N**|), adjusted for Ny < 0
7. AoP: ω = arccos(**N**·**e**/(|**N**||e|)), adjusted for eᵤ < 0
8. True anomaly: ν = arccos(**e**·**r**/(e·r)), adjusted for **r**·**v** < 0

### Elements → Cartesian

Convert to perifocal frame (P, Q, W):

$$\mathbf{r}_{peri} = \frac{a(1-e^2)}{1+e\cos\nu}
    \begin{pmatrix}\cos\nu\\\sin\nu\\0\end{pmatrix}, \quad
\mathbf{v}_{peri} = \sqrt{\frac{\mu}{p}}
    \begin{pmatrix}-\sin\nu\\e+\cos\nu\\0\end{pmatrix}$$

Then rotate by: Rz(−Ω) · Rx(−i) · Rz(−ω)

## Kepler's Equation

For an elliptic orbit: M = E − e sin E (mean anomaly M, eccentric anomaly E).

Solved by Newton-Raphson:

$$E_{n+1} = E_n + \frac{M - E_n + e\sin E_n}{1 - e\cos E_n}$$

Starting from E₀ = M + e sin M (1 + e cos M) (Mikkola 1987 approximation).

**Convergence:** typically 5–8 iterations for e < 0.9, ≤ 15 for e < 0.999.

## Integrators

### RK4 (default)

$$\mathbf{y}_{n+1} = \mathbf{y}_n + \frac{h}{6}(k_1 + 2k_2 + 2k_3 + k_4)$$

Error: O(h⁵) per step, O(h⁴) global. At h = 1 s in LEO, position error
per orbit ≈ 0.06 m (validated by `test_orbital_mechanics.cpp`).

### RK45 Dormand-Prince (precision mode)

7-stage embedded method with adaptive step control. Uses the step doubling
error estimator with safety factor 0.9 and exponents (1/5, 1/4).

**Reference:** Dormand & Prince (1980), J. Comput. Appl. Math. 6(1):19–26.

### Verlet (symplectic)

$$\mathbf{v}_{n+1/2} = \mathbf{v}_n + \frac{h}{2}\mathbf{a}_n, \quad
  \mathbf{r}_{n+1} = \mathbf{r}_n + h\mathbf{v}_{n+1/2}, \quad
  \mathbf{v}_{n+1} = \mathbf{v}_{n+1/2} + \frac{h}{2}\mathbf{a}_{n+1}$$

Conserves a modified Hamiltonian exactly — energy oscillates but does not
drift secularly. Preferred for long coast phases.

## US Standard Atmosphere 1976

Temperature-altitude profile in 7 layers from 0–86 km.
Above 86 km: exponential decay with scale height H = 8500 m.

Pressure in isothermal layers:
$$p = p_0 \exp\!\left(-\frac{g_0 M (h - h_0)}{R^* T}\right)$$

Pressure in gradient layers:
$$p = p_0 \left(\frac{T}{T_0}\right)^{-g_0 M / (R^* L)}$$

where L = dT/dh is the lapse rate.

**Source:** NOAA/NASA/USAF, *US Standard Atmosphere 1976*, NASA-TM-X-74335.
