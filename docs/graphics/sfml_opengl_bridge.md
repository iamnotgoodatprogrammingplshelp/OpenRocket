# SFML–OpenGL Bridge

## Problem

SFML and raw OpenGL share one GPU context but maintain separate internal state
machines. SFML caches VAO/VBO/shader bindings; calling `glBindVertexArray()` or
`glUseProgram()` from application code can silently corrupt those caches, causing
invisible bugs (wrong textures, broken text, etc.).

## Architecture

```
┌──────────────────────────────────────────────────────────────────────┐
│  Per-frame render order                                               │
│                                                                      │
│  bridge.beginGL()            ← setActive(true), bind HDR FBO         │
│    ├─ glClear(COLOR|DEPTH)                                            │
│    ├─ PlanetRenderer::draw() ← raw OpenGL 4.6 draw calls              │
│    ├─ SpacecraftRenderer::draw()                                      │
│    └─ ParticleFX::draw()                                             │
│                                                                      │
│  bridge.endGL()              ← tone-map HDR→LDR, pushGLStates        │
│    ├─ Run ACES fullscreen-triangle pass into tonemapFBO               │
│    ├─ glReadPixels into pixel staging buffer                          │
│    ├─ Flip rows (OpenGL = bottom-left; SFML = top-left)               │
│    ├─ sfmlTexture.update(pixels)                                      │
│    ├─ window.pushGLStates()   ← save GL state machine                 │
│    └─ window.draw(sfmlSprite) ← draw 3D scene as SFML sprite          │
│                                                                      │
│  [HUD drawing — all SFML 2D calls]                                   │
│    ├─ AltimeterGauge::draw()                                          │
│    ├─ TelemetryGraph::draw()                                          │
│    └─ OrbitalMap::draw()                                             │
│                                                                      │
│  bridge.present()            ← window.popGLStates()                  │
│  win.display()               ← swap buffers                          │
└──────────────────────────────────────────────────────────────────────┘
```

## FBO Layout

| Buffer | Format | Purpose |
|--------|--------|---------|
| `m_fbo` (HDR colour) | RGBA16F | Scene render target — wide colour gamut |
| `m_depthRBO` | D24S8 | Depth test + stencil |
| `m_tonemapFBO` | RGBA8 | LDR output after ACES tone-map |

## Tone-mapping

ACES filmic approximation (Narkowicz 2015):

```glsl
vec3 aces(vec3 x) {
    const float a=2.51, b=0.03, c=2.43, d=0.59, e=0.14;
    return clamp((x*(a*x+b))/(x*(c*x+d)+e), 0.0, 1.0);
}
```

Followed by sRGB gamma: `pow(ldr, vec3(1/2.2))`.

## Row Flip

OpenGL origins at bottom-left; SFML origins at top-left. The bridge flips
rows when copying from `m_pixelBuffer` to `m_sfmlTexture`:

```cpp
const sf::Uint8* src = buffer.data() + (height - 1 - y) * rowBytes;
sf::Uint8*       dst = flipped.data() + y * rowBytes;
```

This is O(width × height × 4) — measured at < 0.3 ms at 1080p.

## GL State Leak Prevention

`window.pushGLStates()` / `popGLStates()` save and restore the full SFML
GL state snapshot. The bridge calls these precisely once per frame:
push before any SFML draw, pop after the last SFML draw (in `present()`).

Never call `glBindFramebuffer`, `glUseProgram`, or any other raw GL function
between `endGL()` and `present()`. If you need a custom shader in the HUD,
use `sf::Shader` instead.
