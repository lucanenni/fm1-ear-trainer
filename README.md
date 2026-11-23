# FM-1 Ear Trainer

An ear-training mode (intervals, chord qualities, scale degrees) built on top of
[Felucca](https://github.com/hugelton/Felucca), the open-source multi-engine synth firmware for
the M-VAVE FM-1. See [`fm1-ear-trainer-spec.md`](fm1-ear-trainer-spec.md) for the full spec this
project follows.

> **Status: work in progress, Fase 2 of 4 — not yet installed or tested on real hardware.**
> Everything below has only been run on the host (unit tests) and in the browser emulator. There
> is no firmware release yet, no `.fwsc` package, and no installation instructions for a real
> FM-1. Treat this as an early preview, not something to flash onto your device. See
> [Project status](#project-status) below for what's done and what isn't.
>
> This is an unofficial, community derivative. It is **not affiliated with, endorsed by, or
> supported by M-VAVE** or Hügelton Instruments. "FM-1" is a trademark of its respective owner.

## Try it in the browser

No FM-1 needed — a browser build of the (unmodified) Felucca firmware, now with the Trainer mode
wired in, runs entirely client-side via WebAssembly:

**→ [lucanenni.github.io/fm1-ear-trainer](https://lucanenni.github.io/fm1-ear-trainer/)**

Click "Switch on", hold **HOME** to open the settings menu, turn the **ALGORITHM** knob to the
**SYSTEM** tab, turn **PRESETS** down to **TRAINER**, press **OCT+** to enter. Controls once
inside: **PLAY** replays the question, **REC** plays/reveals the correct answer, **OCT-/OCT+**
change the level, the **SELECT** encoder switches exercise (interval / chord / degree), **HOME**
exits. Answer by playing the note(s) on the keybed.

This demo build (`felucca/build/emu/`) is compiled from the `main` branch; the live page may lag
behind the latest commit until it's rebuilt and redeployed to the `gh-pages` branch.

## Project status

- **Fase 0 — ricognizione**: done. [`docs/RECON.md`](docs/RECON.md): why Felucca over FM1 Open,
  the real controls (14 buttons, 7 encoders, 27 keys), the proposed Trainer control mapping.
- **Fase 1 — nucleo didattico + test**: done. [`trainer/`](trainer/): the exercise logic (pure
  C99, no hardware or Felucca dependency, host-testable — `trainer/tests/run_tests.sh`).
- **Fase 2 — interfaccia nell'emulatore**: done. The Trainer is reachable from `MENU > TRAINER`
  in Felucca's own browser emulator (`felucca/web/emu/`); see above.
- **Fase 3 — build e installazione sull'hardware**: *not started*. No `.fwsc` package has been
  built, and nothing has been flashed to or tested on a real FM-1. The real-hardware build
  (`felucca/build.sh`) needs a few more wiring changes first (see the note in the Fase 2 commit
  message) and, obviously, a device to test on.
- **Fase 4 — rifinitura**: not started (advanced stats, Italian strings, more exercise types).

## Building it yourself

```bash
cd felucca
./build.sh                 # firmware build (needs the JieLi toolchain + AC79 SDK: see BUILDING.md)
sh web/emu/build.sh         # browser build (needs Emscripten + Python/Pillow/fontTools for ./build.sh's gen step)
sh ../trainer/tests/run_tests.sh   # the trainer core's own tests (needs only a C99 compiler)
```

`felucca/BUILDING.md` has the full prerequisites.

## License

Felucca itself (vendored in [`felucca/`](felucca/), forked from
[hugelton/Felucca](https://github.com/hugelton/Felucca); see
[`felucca/UPSTREAM.md`](felucca/UPSTREAM.md) for the exact commit) is GPL-3.0-only for the app,
MPL-2.0 for `firmware/bsp`, MIT for `firmware/engines` — see `felucca/LICENSE`,
`felucca/LICENSING.md` and `felucca/LICENSES/`. The Trainer additions
([`trainer/`](trainer/), `felucca/firmware/src/trainer_*.c`) are released under the same terms as
the app they extend, GPL-3.0-only.
