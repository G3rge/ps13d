# PS1-3D

**A PS1-flavored software 3D engine and scene editor, written in C.**

Fixed-point math end to end, no GPU, no floats inside the engine. The classic
PlayStation look — vertex jitter, affine texture warping, RGB555 color — comes
straight out of a hand-written scanline rasterizer, wrapped in a real-time
editing tool.

[![language](https://img.shields.io/badge/language-C-555555?style=flat-square&logo=c&logoColor=white)](#)
[![platform](https://img.shields.io/badge/platform-Windows%20%2F%20SDL2-1e88e5?style=flat-square)](#)
[![math](https://img.shields.io/badge/math-fixed--point%2C%20no%20float-43a047?style=flat-square)](#)
[![license](https://img.shields.io/badge/license-MIT-6a1b9a?style=flat-square)](#)

---

## Preview

A frame rendered entirely by the software pipeline (headless `make selftest`):
![Rendered frame] <img width="512" alt="sh" src="https://github.com/user-attachments/assets/0cc270d3-a31f-43c5-be3d-791cb8a33b83" />

## Highlights

- **Software PS1-style renderer** — scanline rasterizer, affine UVs, GTE-style
  vertex quantization, 16-bit `1/z` depth buffer or painter sort, back-face
  culling and near-plane clipping. 240x160 RGB555 framebuffer.
- **Real-time editor** — Unity-style top toolbar with hover menus:
  - `FILE` — SAVE / OPEN scenes (`.ps13d`), HOME resets the camera.
  - `CREATE` — CUBE / SPHERE / PLANE / CYLINDER / CAPSULE.
  - `LOAD` — MODEL imports a Wavefront `.obj` (auto-centered and auto-scaled).
  - `SL / MV / RT / SC` — select, move, rotate, scale tools.
  - `PLAY / STOP` — run the little demo simulation, or stop it.
- **Gizmo drag** — with the MOVE tool, left-click an axis handle and drag to
  move an object in that direction.
- **Per-object textures** — every object owns its material. Assign a PNG to the
  selected object via `INSPECTOR > MATERIAL > ALBEDO > BROWSE`, or drop PNGs
  into the assets dock. Loaded `.obj`s pick up their `map_Kd` from the MTL
  automatically. Up to 32 texture slots, 128x128 max.
- **Fixed-point audit** — `make verify-no-float` proves the engine has no
  `float`/`double` outside the PC/sys layer and LUT generation.
- **Fidelity knobs** — `PS1_RATIO_CLEAR`, `PS1_SCREEN_CLEAR`,
  `PS1_UV_CLEAR` in `src/core/ps1_config.h` tune the aliasing look.

## Quick start

Requires MSYS2 `ucrt64` (gcc) and SDL2 (incl. SDL_image). On Windows,
`run.bat` puts MSYS2's SDL2 dlls on the PATH.

```sh
make                 # Q16.16 build -> ps13d.exe
make q88             # Q8.8 build
make q1616           # Q16.16 build
make selftest        # headless: 120-frame render -> selftest.png
make verify-no-float # audit: no float tokens outside PC/sys layer
./run.bat            # open the editor, 967x640 window
```

Headless / quick-render usage:

```sh
ps13d --obj model.obj        # open editor, pre-loaded with a model
ps13d --tex texture.png      # open editor with a default albedo
ps13d --selftest 120         # render 120 frames, export ps13d_frame.png
ps13d --png shot.png --frames 60   # headless render, no window
ps13d --scale 2              # window scale
```

## Controls

| Input | Action |
| --- | --- |
| Left-click | select an object / drag a MOVE gizmo axis |
| Right-drag | orbit camera |
| `W A S D` | fly (hold `Shift` to sprint) |
| `Q / E` | move camera up / down |
| `← ↑ ↓ →` | rotate camera yaw / pitch |
| `Z` | toggle depth mode (painter sort vs 16-bit z-buffer) |
| `F` | toggle textures |
| `L` | toggle lighting |
| `C` | cycle cull mode |
| `TAB` | profiler overlay |
| `ENTER` / `BACKSPACE` | play simulation / stop |
| `P` | save current frame as `ps1_frame.png` |
| `ESC` | quit |

## Editor layout

- **Top toolbar** — FILE / CREATE / LOAD menus, transform tools, PLAY/STOP.
- **Left panel** — CAM (add a camera) + scene hierarchy; click to select.
- **Center** — 3D viewport with grid and selection gizmo.
- **Right panel (inspector)** — name, kind, position X/Y/Z (10-step nudge,
  direct entry), material albedo swatch + BROWSE, ambient / diffuse controls.
- **Bottom bar** — OBJ / FPS / TRI stats.

## Architecture

```
src/
  core/        config, types, fixed-point math, LUTs (sin/cos/recip), timing
  graphics/    RGB555 color, 240x160 framebuffer, depth buffer, clipping,
               rasterizer, textures, 5x7 font
  renderer3d/  vertex, camera, transforms, material, lighting, mesh, pipeline
  scene/       entity + scene (static array pools)
  assets/      procedural mesh builder, Wavefront OBJ loader (v/vt/vn/f),
               MTL loader with map_Kd, texture loader
  editor/      editor shell: toolbar, hierarchy, inspector, viewport, gizmos
  sys/         SDL2 window (keyboard + mouse), PNG import (SDL_image),
               native file dialogs (comdlg32), profiler, font
  main.c       arg parsing, headless/selftest mode, main loop
```

## Pipeline

1. Model + world transform, combined `view * model` matrix.
2. Clip against the near plane (Sutherland-Hodgman, fan output).
3. Perspective divide into fixed-point screen Q12 — deliberately quantized
   for PS1-style vertex jitter.
4. Back-face culling by projected winding.
5. Painter sort (default) or 16-bit z-buffer.
6. Scanline rasterization with incremental edge functions and affine UVs.

## Verification

- `--selftest` runs the renderer headless and exports a PNG; it asserts the
  expected pixel/primitive counts.
- `make verify-no-float` greps the engine for floating-point tokens.

## License

MIT — see [LICENSE](LICENSE).
