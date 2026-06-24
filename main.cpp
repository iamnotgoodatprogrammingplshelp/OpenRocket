/**
 * @file main.cpp
 * @brief OpenOrbit — Phase 2 main entry point.
 *
 * Demonstrates the full SFML + OpenGL pipeline:
 *   - SFML RenderWindow with OpenGL 4.6 core-profile context
 *   - PlanetRenderer (UV-sphere, Cook-Torrance shading)
 *   - SFMLOpenGLBridge (HDR FBO → tone-map → SFML texture blit)
 *   - AltimeterGauge + TelemetryGraph composited as SFML 2D HUD
 *   - PhysicsWorld running a LEO spacecraft in a background thread
 *
 * Controls:
 *   Z / X   — throttle up / down
 *   Space   — stage
 *   M       — toggle orbital map
 *   H       — toggle HUD
 *   Escape  — quit
 */

#include "renderer/window/AppWindow.hpp"
#include "renderer/sfml_opengl_bridge/SFMLOpenGLBridge.hpp"
#include "renderer/gl3d/PlanetRenderer.hpp"
#include "renderer/hud/instruments/AltimeterGauge.hpp"
#include "renderer/hud/telemetry/TelemetryGraph.hpp"
#include "renderer/hud/map/OrbitalMap.hpp"
#include "core/physics/PhysicsWorld.hpp"
#include "core/world/CelestialBody.hpp"
#include "core/propulsion/Engine.hpp"

#include <SFML/Graphics.hpp>
#include <GL/gl.h>
#include <Eigen/Dense>
#include <cmath>
#include <numbers>
#include <print>
#include <atomic>
#include <thread>
#include <mutex>

using namespace openorbit;

// ─────────────────────────────────────────────────────────────────────────────
// Camera (simple spherical / chase camera)
// ─────────────────────────────────────────────────────────────────────────────

struct Camera {
    Eigen::Vector3f target{0, 0, 0};
    float           distance = 1.5e7f;   // 15 000 km
    float           azimuth  = 0.0f;
    float           elevation = 0.3f;

    Eigen::Vector3f position() const {
        float ce = std::cos(elevation), se = std::sin(elevation);
        float ca = std::cos(azimuth),   sa = std::sin(azimuth);
        return target + Eigen::Vector3f{
            distance * ce * ca,
            distance * se,
            distance * ce * sa
        };
    }

    // Row-major view matrix
    void viewMatrix(float out[16]) const {
        Eigen::Vector3f eye = position();
        Eigen::Vector3f up{0, 1, 0};
        Eigen::Vector3f f = (target - eye).normalized();
        Eigen::Vector3f r = f.cross(up).normalized();
        Eigen::Vector3f u = r.cross(f);

        out[0]=r.x(); out[1]=u.x(); out[2]=-f.x(); out[3]=0;
        out[4]=r.y(); out[5]=u.y(); out[6]=-f.y(); out[7]=0;
        out[8]=r.z(); out[9]=u.z(); out[10]=-f.z();out[11]=0;
        out[12]=-r.dot(eye); out[13]=-u.dot(eye); out[14]=f.dot(eye); out[15]=1;
    }

    // Column-major perspective projection
    static void perspectiveMatrix(float fov_deg, float aspect, float znear, float zfar,
                                   float out[16]) {
        float f = 1.0f / std::tan(fov_deg * std::numbers::pi_v<float> / 360.0f);
        float range = znear - zfar;
        out[0]=f/aspect; out[1]=0; out[2]=0;                    out[3]=0;
        out[4]=0;        out[5]=f; out[6]=0;                    out[7]=0;
        out[8]=0;        out[9]=0; out[10]=(zfar+znear)/range;  out[11]=-1;
        out[12]=0;      out[13]=0; out[14]=2*zfar*znear/range;  out[15]=0;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Application
// ─────────────────────────────────────────────────────────────────────────────

int main() {
    std::print("OpenOrbit starting...\n");

    // ── Window ───────────────────────────────────────────────────────────────
    renderer::AppWindow win(1920, 1080, "OpenOrbit — Phase 2", false);

    // ── OpenGL–SFML bridge ───────────────────────────────────────────────────
    renderer::SFMLOpenGLBridge bridge(win.sfml(), win.width(), win.height());

    // ── 3D renderer ──────────────────────────────────────────────────────────
    win.activateGL();
    renderer::PlanetRenderer planets;
    win.deactivateGL();

    // ── HUD font ─────────────────────────────────────────────────────────────
    sf::Font font;
    // Try to load a system monospace font; fall back silently
    const std::vector<std::string> fontPaths = {
        "assets/fonts/RobotoMono-Regular.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf",
        "/System/Library/Fonts/Menlo.ttc"
    };
    bool fontLoaded = false;
    for (const auto& p : fontPaths) {
        if (font.loadFromFile(p)) { fontLoaded = true; break; }
    }
    if (!fontLoaded) std::print(stderr, "Warning: no monospace font found; HUD text disabled\n");

    // ── HUD instruments ──────────────────────────────────────────────────────
    hud::AltimeterGauge altimeter(font, 90.0f);
    hud::ThrustGauge    thrustGauge(font, 220.0f, 38.0f);
    hud::TelemetryGraph telemetry(font, 420.0f, 160.0f, 120.0f, 10.0f);
    telemetry.setTitle("Telemetry");
    int altSeries = telemetry.addSeries("Altitude (km)", sf::Color(0x00, 0x88, 0xFF, 0xFF));
    int spdSeries = telemetry.addSeries("Speed (km/s)",  sf::Color(0x00, 0xFF, 0x88, 0xFF));

    hud::OrbitalMap mapPanel(font, 280.0f, 280.0f);
    bool showMap = true;
    bool showHUD = true;

    // ── Physics world ─────────────────────────────────────────────────────────
    physics::PhysicsWorld world(physics::IntegratorType::RK4);
    auto earthDef = world::makeEarth();
    world.addGravBody(earthDef.toGravBody());
    world.setPrimaryBodyName("Earth");

    // Spacecraft: circular LEO at 400 km
    constexpr double kMuEarth  = 3.986004418e14;
    constexpr double kREarth   = 6.3781e6;
    constexpr double kAlt      = 400e3;
    double r = kREarth + kAlt;
    double v = physics::circularOrbitSpeed(r, kMuEarth);

    physics::StateVector initState;
    initState.position = {r, 0.0, 0.0};
    initState.velocity = {0.0, v, 0.0};
    initState.mass_kg  = 5000.0;
    initState.fuel_kg  = 2000.0;

    physics::InertiaProperties inertia;
    inertia.mass_kg = 5000.0;
    inertia.inertia = Eigen::Matrix3d::Identity() * 2500.0;

    auto scID = world.addEntity(initState, inertia);

    // ── Input callbacks ───────────────────────────────────────────────────────
    std::atomic<float> throttleCmd{0.0f};
    renderer::InputCallbacks cbs;
    cbs.onThrottleUp   = [&] { throttleCmd = std::min(throttleCmd.load() + 0.1f, 1.0f); };
    cbs.onThrottleDown = [&] { throttleCmd = std::max(throttleCmd.load() - 0.1f, 0.0f); };
    cbs.onToggleMap    = [&] { showMap = !showMap; };
    cbs.onToggleHUD    = [&] { showHUD = !showHUD; };
    win.setInputCallbacks(cbs);

    // ── Camera ────────────────────────────────────────────────────────────────
    Camera camera;
    camera.distance = static_cast<float>(r * 2.5);

    // ── Sun direction (fixed for Phase 2) ─────────────────────────────────────
    Eigen::Vector3f sunDir = Eigen::Vector3f{1.0f, 0.3f, 0.2f}.normalized();

    // ── Main loop ─────────────────────────────────────────────────────────────
    double accumTime = 0.0;
    constexpr double kPhysicsStep = 0.1; // 10 Hz physics

    while (win.isOpen()) {
        win.pollEvents();
        double dt = static_cast<double>(win.deltaTime());

        // ── Physics step ──────────────────────────────────────────────────────
        accumTime += dt;
        while (accumTime >= kPhysicsStep) {
            world.step(kPhysicsStep, 1);
            accumTime -= kPhysicsStep;
        }

        physics::StateVector sc = world.getState(scID);
        double altitude_m  = world.getAltitude(scID);
        double speed_ms    = sc.velocity.norm();

        // Slowly rotate camera around the planet
        camera.azimuth += static_cast<float>(dt) * 0.05f;
        Eigen::Vector3f scPosF = sc.position.cast<float>();
        camera.target = scPosF * 0.0f; // orbit around planet centre

        // ── Telemetry push ────────────────────────────────────────────────────
        telemetry.push(altSeries, static_cast<float>(altitude_m / 1000.0));
        telemetry.push(spdSeries, static_cast<float>(speed_ms   / 1000.0));

        // ── Update HUD ────────────────────────────────────────────────────────
        altimeter.update(altitude_m, sc.velocity.dot(sc.position.normalized()));

        // ── 3D render pass ────────────────────────────────────────────────────
        bridge.beginGL();
        {
            float view[16], proj[16];
            camera.viewMatrix(view);
            Camera::perspectiveMatrix(60.0f,
                                       static_cast<float>(win.width()) / win.height(),
                                       1000.0f, 2e8f, proj);
            Eigen::Vector3f camPos = camera.position();

            // Draw Earth centred at origin (spacecraft position offset handled in camera)
            planets.draw(earthDef, view, proj, camPos, sunDir);
        }
        bridge.endGL(); // → tone-map + blit + pushGLStates

        // ── SFML 2D HUD pass ─────────────────────────────────────────────────
        if (showHUD) {
            float W = static_cast<float>(win.width());
            float H = static_cast<float>(win.height());

            altimeter.draw(win.sfml(), {100.0f, H - 220.0f});
            thrustGauge.update(throttleCmd.load(), 0.0, 0.0);
            thrustGauge.draw(win.sfml(), {20.0f, H - 60.0f});
            telemetry.draw(win.sfml(), {20.0f, 20.0f});
        }

        if (showMap && showHUD) {
            float W = static_cast<float>(win.width());
            mapPanel.setPrimarySOI(earthDef.soi_m, "Earth");
            mapPanel.setViewRadius(2e7);
            mapPanel.clearTracks();

            // Current orbit track
            hud::OrbitalMap::OrbitTrack current;
            current.elements = world.getOrbitalElements(scID);
            current.mu       = kMuEarth;
            current.color    = sf::Color(0xFF, 0xFF, 0xFF, 0xCC);
            current.label    = "SC";
            mapPanel.addOrbit(current);

            hud::OrbitalMap::SpacecraftIcon icon;
            icon.position = sc.position;
            icon.velocity = sc.velocity;
            mapPanel.setSpacecraft(icon);

            mapPanel.draw(win.sfml(), {W - 300.0f, 20.0f});
        }

        // FPS counter
        if (fontLoaded) {
            sf::Text fps(font);
            fps.setCharacterSize(14);
            fps.setFillColor(sf::Color(0x44, 0xFF, 0x88, 0xFF));
            fps.setString(std::format("FPS:{:.0f}  T={:.1f}s  Alt={:.1f}km",
                                       win.fps(), world.getSimTime(), altitude_m / 1000.0));
            fps.setPosition({static_cast<float>(win.width()) - 400.0f,
                              static_cast<float>(win.height()) - 30.0f});
            win.sfml().draw(fps);
        }

        bridge.present(); // popGLStates
        win.display();
    }

    std::print("OpenOrbit exiting normally.\n");
    return 0;
}
