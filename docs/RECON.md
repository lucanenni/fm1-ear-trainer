# RECON — Phase 0

Verified against the source code cloned into `vendor/Felucca` (current commit of `main`, with
submodules `firmware/bsp` and `firmware/engines`) and `vendor/fm1_open` (README `V14`). Where my
conclusions correct the spec (`fm1-ear-trainer-spec.md`), I flag it explicitly with
**[DISCREPANCY]**.

## 1. Felucca vs FM1 Open comparison and choice

| | Felucca | FM1 Open |
|---|---|---|
| License | GPL-3.0-only (app) + MPL-2.0 (`firmware/bsp`) + MIT (`firmware/engines`) | GPLv3 **with a linking exception for the FM1 SDK** (`LICENSE.md`); the sound engine is a **binary SDK** with its own terms (`THIRD_PARTY.md`) |
| Hardware build | macOS: Docker (`linux/amd64` debian) + JieLi Linux toolchain + Apache-2.0 AC79 SDK. Linux x86-64: native | **Requires Windows x64 + PowerShell**, JieLi Windows toolchain 2.5.3; no documented macOS/Linux path for building the firmware |
| Development without hardware | Yes: `tests/run_tests.sh` (unit/regression tests on the host) and `web/emu` (Emscripten → WebAssembly, browser demo) | Web preview (`Build-Web.ps1`), but built with PowerShell/WASI SDK tooling; I didn't find an equivalent host-side unit test suite |
| Audio engine | Full source (FM6 derived from Dexed/msfa, plus DRUM, PHYS, GRAIN, SLICE, SAMPLE, FORMANT, ADPCM, DIGITAL) | Closed binary (SDK); only the app layer is open |
| UI architecture | Pages per family (`FAM_*`) + a "quick layer" (HOLD/SET) on 7 buttons; "unity build" style code (many `.c` files `#include` each other) | Not verified in depth (see §7, risk) |
| Documentation | Detailed `BUILDING.md`; **no** top-level `ARCHITECTURE.md`, `PORTING.md`, `PLUGINS.md`, `COMPATIBILITY.md` as cited by the spec | `README.md` + `app/instrument.h` (tuning API) |

**[DISCREPANCY]** The spec (section 2) describes Felucca as having a host layer with
`platforms/host`, `felucca_host.h`, and a `./build.sh gen|web|sdl|host|test` build. None of that
exists in the current repo: there's no `platforms/` folder, no `felucca_host.h`, `build.sh` takes
no subcommands (it always does the full firmware build via `tools/build.py`), and there's no SDL2
or libretro target (verified with an exhaustive search, zero results). What does exist instead:
- `tests/run_tests.sh` — unit/regression tests on the host (what the spec called "build.sh test");
- `web/emu/` — browser demo via Emscripten/WebAssembly (what the spec called "build.sh web"),
  already published at `https://hugelton.github.io/Felucca/webapp/try/`.

No native desktop SDL2 equivalent: the "no hardware" development the spec expects for Phase 2 has
to happen either in the browser (`web/emu`) or by extending the host tests (`tests/`), not in a
separate SDL build.

**Choice: Felucca**, confirmed. Reasons, in order of weight:
1. The sound engine and BSP are entirely open (GPL/MPL/MIT): we can read and, if needed, extend
   any layer, which is impossible with FM1 Open's binary SDK.
2. Reproducible build on macOS (Docker) without Windows.
3. A no-hardware development/test path already exists (`tests/run_tests.sh` + `web/emu`), even if
   under different names and tools than the spec assumed.
4. Code and commit messages are extremely well documented line by line (every file explains the
   "why"), which reduces the risk of breaking something for the Trainer.

## 2. How a new mode/screen is added

There's no plugin-like "screens" system. The real architecture (`firmware/src/`):

- **Main loop** (`main.c`, `fm1_main`): after boot and init, runs forever calling, each frame
  (~60/s), `ui_input()` → `ui_leds()` → `ui_draw()`. There are already overlays that "steal" the
  main loop (e.g. the UBOOT countdown in `main.c`, drawn directly on top of everything), so there's
  precedent for a mode that temporarily replaces normal input/draw.
- **Normal UI**: pages organized by family (`FAM_HOME`, `FAM_SCL`, `FAM_SEQ`, ...) defined as
  tables (`page_t`), plus 7 "quick layers" activated by holding a page button (`ui_layer.c`, table
  `LAYERS[]`): FX, GLO, SCL, EDIT, SEQ, REC. Every page button already has a fixed role.
- **Code style**: many `.c` files include each other (e.g. `main.c` does `#include "timer5.c"`,
  `ui_layer.c` does `#include "ui_tools.c"`) and functions are almost all `static` — it's a "unity
  build", not modules with public headers. The note on/off functions (`trk_note_on`,
  `trk_note_off`, `trk_all_off` in `voice.c`) are `static`, not exposed.

**Consequence for the required architecture (spec section 4):**
- The **exercise core** (`trainer/`) can be written in plain C, with no Felucca headers, exactly
  as required: it doesn't need to know anything about the hardware or the unity-build style.
- The **audio/UI layer** inside Felucca, however, can't just "call existing APIs": those APIs
  (note on/off) aren't public today. A few new, non-`static` bridge functions will need to be
  added (new, minimal, isolated in their own file, e.g. `trainer_bridge.c`) that call
  `trk_note_on`/`trk_note_off`/`trk_all_off` on the `track_t` chosen for the Trainer. This is still
  a small, localized change, consistent with the "new code kept separate from the base" constraint
  (section 4), but it's a risk/cost the spec didn't mention: worth watching in upstream Felucca
  merges.
- **Trainer mode** as a "screen" will follow the same pattern as the existing overlays: a flag
  (e.g. `ui.trainer`) checked at the top of `ui_input()`/`ui_draw()`/`ui_leds()` that, when active,
  skips the normal UI and uses its own input/draw routines (consistent with how UBOOT already
  behaves).

## 3. Display, input, audio, storage APIs (code references)

- **Input**: `firmware/bsp/boards/fm1/fm1_input.h` — 11-column × 6-row matrix, 14 buttons
  (ids 0..13) + 27 note keys (ids 14..40, note 0 = F3 … note 26 = G5) + 7 quadrature encoders with
  "detent" detection. Main APIs: `fm1_input_scan()`/`fm1_input_tick()` (polling/IRQ),
  `fm1_input_edges()` (button edges), `fm1_input_note_edges()` (key edges),
  `fm1_enc_take(role)` (encoder steps since the last read), `fm1_led_key(id, on)`.
  `panel.c` maps the matrix ids to the 14 buttons' **real names** and to the 7 encoders' roles
  (see §4) — this mapping is calibrated on real hardware (OCT- + OCT+ at boot), so the numbers are
  a "best guess" until verified on a real FM-1.
- **Display**: `lcd.c` (TFT driver), `gfx.c`/`ui_draw.c`/`ui_graph.c` (drawing primitives),
  `ui.c`/`ui_menu.c`/`ui_help.c` (menu and help). Color TFT, confirmed 240×240 as the usable area
  (see `lcd_fill(0, 0, 240, 240, ...)` used throughout `main.c`) — the spec says "1.54"" but
  doesn't give the pixel resolution; 240×240 is consistent with a common 1.54" panel.
- **Audio**: `audio.c` (start/stop, `fm1_audio_stop()`), `voice.c` (per-track voice management,
  `NVOICE` = shared budget), `engines.c`/`eng_fm6.c`/`fm6_core.c` (FM engine, 32 algorithm tables,
  derived from msfa/Dexed — confirmed). Note on/off are currently `static` in `voice.c`
  (`trk_note_on`, `trk_note_off`, `trk_all_off`): need to be exposed with a bridge (see §2).
- **Storage**: `storage.c`/`storage_hw.c` (flash), `settings_persist.c` (settings records with a
  "magic" value + version-to-version migration, e.g. `PERSIST_MAGIC`), `project.c`
  (projects/presets). The mechanism is already designed to survive sudden power loss and firmware
  updates (old formats are migrated, not discarded) — suitable for hosting a Trainer data block
  (settings + statistics) following the same versioned "magic number" scheme. A deeper look at
  `storage.c`/`storage_hw.c` is deferred to Phase 1, when the Trainer's data format gets designed.

## 4. Real controls and proposed mapping (to be approved)

**[DISCREPANCY]** The spec (section 2) talks about "about 12 mode buttons" and "4 encoders in the
parameter section". The code (`panel.c`, `fm1_input.h`) gives precise, different numbers:

- **14 real buttons** (matrix ids 0..13, names from `panel.c`):
  `FX`, `SCL`, `ENV`, `LFO`, `EDIT`, `GLO`, `HOME`, `SAVE`, `ARP`, `SEQ`, `PLAY`, `REC`, `OCT-`, `OCT+`.
- **7 real encoders** (not 4): `SELECT`, `ALGORITHM`, `PRESETS`, `KNOB 1`, `KNOB 2`, `KNOB 3`, `KNOB 4`.
- **27 note keys** (F3..G5), of which **16 are white keys** (F3 G3 A3 B3 C4 D4 E4 F4 G4 A4 B4 C5 D5
  E5 F5 G5) — this number **matches** the spec ("16 white keys... used as steps").

All 14 buttons already have a role in Felucca's normal UI (pages or quick layers); **none is
free**. To enter/exit the Trainer without stealing an already-used button, I propose (to be
discussed and approved before implementing, as the spec requires):

- **Entry**: a menu item (`HOME` → `MENU` → new `TRAINER` entry), plus possibly a "hold"
  combination consistent with the existing style (e.g. `HOME` held long, following the other
  quick-layer pattern), to be checked against the long-press `HOME` already uses in the normal UI.
- **Exit**: the same path (`MENU` → `EXIT TRAINER`), or `HOME` again if Trainer intercepts it
  before the normal UI does.
- **Inside the Trainer** (functions from spec section 5), proposed reuse of already "natural"
  roles: `PLAY` = replay/confirm, `REC` = show answer, `SAVE` = (unused, or reset stats with
  confirmation), `OCT-`/`OCT+` = level down/up, `KNOB 1..4` = scroll through options/settings,
  `SELECT`/`PRESETS` = choose exercise/level. None of these assignments is final: they need to be
  confirmed against the real `ui_input.c` code (which buttons fire "hold" vs "tap" events) and,
  ideally, on real hardware, before Phase 2.

## 5. Toolchain for the hardware build

- **macOS** (the user's environment): Python 3 + Pillow + fontTools; Docker Desktop (the JieLi
  toolchain is Linux x86-64 only, run in a `linux/amd64 debian:bookworm-slim` container, via
  Rosetta on Apple Silicon); JieLi toolchain (`tools/get_toolchain.sh` → `~/.jieli/toolchain`);
  JieLi AC79 SDK Apache-2.0 (`git clone --depth 1 --branch AC79NN_SDK_V1.2.1_2023-12-13
  https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK.git ~/fw-AC79_AIoT_SDK`); Node.js optional (web
  tests); Emscripten optional (`web/emu`).
- Build: `./build.sh` (debug) or `./build.sh --release X.Y` (release, also assigns the identity).
- None of these tools are installed in this session yet: they'll be downloaded when reaching
  Phase 3 (or earlier, to verify the real build already in Phase 1/2). No downloads or
  installations were performed at this stage.

## 6. Firmware identity

The real mechanism (`tools/build.py`, `tools/fm1pkg_make.py`, `firmware/src/felucca.c`,
`tools/fm1_install.py`):
- The identity is an 8-character string like `FM-1_9XY`, embedded both in the binary
  (`FELUCCA_ID` in `felucca.c`, default `"FM-1_900"`) and in the `.fwsc` package (`PRODUCT` in
  `build.py`, same default; with `--release X.Y` it becomes `FM-1_9` + the two digits). It's
  written as a repeated marker in the package's first 20 blocks (`fm1pkg_make.py`).
- The installer (`fm1_install.py`) reads the connected device's identity and, after installing,
  **verifies** that the written identity matches the package's (exit code 6 = "wrong model", a
  different code = "another identity after install"); it's a consistency check, not a
  firmware-side security gate.
- The official stock firmware is recognized by an exact SHA-256 hash (two constants in the
  script), not by identity — going back to the official M-Updater firmware doesn't depend on our
  identity.

**Proposal for the project**: use our own identity, distinct both from Felucca's default
(`FM-1_9XY`, reserved for upstream updates) and from the official firmware's, so as not to be
mistaken for a Felucca update or the factory firmware. It needs to be exactly 8 characters; I
propose something like `FM-1_EA1` (EA = Ear trainer) — **to be confirmed with the user** before
setting it, because it determines what the web installer and `fm1_install.py` accept as "the same
firmware" for the project's future incremental updates.

## 7. Identified risks

1. **Non-public audio APIs**: the note on/off functions are `static` in the monolithic
   `voice.c` file. Requires a small new bridge (low risk, but not zero for upstream merges).
2. **No native SDL/desktop build**: the "no hardware" development expected for Phase 2 will have
   to use `web/emu` (requires Emscripten) or the host tests (`tests/run_tests.sh`), not a desktop
   executable as the spec assumed. Need to verify that `web/emu` is comfortable for iterating on an
   interactive UI (the browser demo, per the docs, doesn't emulate USB/MIDI/TRS/user samples —
   acceptable for the Trainer, which doesn't need any of that).
3. **All physical buttons are already assigned**: entering/exiting the Trainer necessarily
   requires a new gesture or a menu entry; no choice is "obvious" and needs user confirmation
   (section 4 above) before implementing, as the spec itself requires.
4. **Non-trivial hardware toolchain on macOS**: Docker + Linux toolchain + third-party Apache-2.0
   SDK to download from Gitee; need to verify the download works from the user's network once
   Phase 3 is reached (not yet tried).
5. **FM1 Open build unverified**: being Windows-only, I couldn't verify its build; the information
   about the binary SDK comes from the README, not from an actual build attempt.
6. **Spec/code divergence on the controls**: the spec undercounts encoders (4 vs. 7 real) and
   doesn't list the 14 buttons' real names; corrected here, but spec section 2 should be updated
   (see final note).
7. **Incomplete verification**: this recon read the main documentation and the code parts most
   relevant to the architecture (input, panel, main loop, voice, settings, identity), but not the
   whole tree (76 files in `firmware/src/`, plus BSP and engines). Details on `project.c`,
   `storage_hw.c` and the menu UI (`ui_menu.c`) will need a closer look at the start of Phase 1,
   when the exact data format for persistent statistics/settings is needed.

## 8. Note on the spec

As required by spec section 8 ("when information... turns out to be wrong relative to the code,
the code wins"): section 2 of `fm1-ear-trainer-spec.md` (software-bases table, specifically the
Felucca row) contains claims not verified in the current code (`platforms/host`,
`felucca_host.h`, `build.sh` targets `web|sdl|host|test`, "up to 12 voices"). I haven't changed it
at this stage so as not to alter a reference document without explicit confirmation; I'm flagging
the corrections here and waiting for guidance on whether to update it.
