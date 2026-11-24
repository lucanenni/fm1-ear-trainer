# FM-1 Ear Trainer: development spec

Reference document for developing, with Claude Code, a custom ear-training firmware for the M-VAVE FM-1 synthesizer.

Project working name: **FM-1 Ear Trainer** (negotiable).

---

## 1. Goal

Turn the FM-1 into a standalone device for training musical ear. The synth plays a question (interval, chord, scale degree…), the user answers on the keybed or with the controls, and the display shows the result and statistics. No computer or phone needed during use.

Baseline requirements:
- the synth's sound engine and base functions stay available (ideally returning to "play" mode with one key);
- exercises must be usable hands-on, on the device, without deep menus;
- it must work for a beginner student as well as for an experienced musician (configurable levels).

---

## 2. Technical context (to be verified against the code)

The information below comes from public sources. **Claude Code must verify it against the source code before building on it** and flag any differences.

### FM-1 hardware
- JieLi AC791N / WL82 SoC, pi32v2 CPU.
- 6-operator FM engine derived from Dexed/msfa, 32 algorithms, up to 12 voices in the stock firmware.
- 27-key silicone keybed; the 16 white keys are used as sequencer steps.
- 1.54" color TFT display.
- Rotary encoders (4 in the parameter section, per the manual), OCT−/OCT+ buttons, about 12 mode buttons.
- Built-in speaker, 3.5 mm jack output, battery.
- MIDI over USB-C, BLE and 3.5 mm TRS (IN only). Over USB it's a composite audio + MIDI device.

### Available software bases
| Base | License | Notes |
|---|---|---|
| **Felucca** (`hugelton/Felucca`) | GPL-3.0-only | Very mature multi-engine firmware. Has a host layer (`platforms/host`, `felucca_host.h` API) with a web build (`platforms/web`), SDL2 desktop and libretro, so it can be developed and tested without hardware. `./build.sh gen` generates the headers without the FM-1 toolchain; `./build.sh web|sdl|host|test` for the other targets. Hardware layer separated into `felucca-bsp` (submodule). Documentation in `docs/` (ARCHITECTURE.md, PORTING.md, PLUGINS.md, COMPATIBILITY.md, BUILDING.md). Tools: `tools/fm1_install.py`, web installer, web editor. |
| **FM1 Open** (`m-vave/fm1_open`) | GPLv3 (verify details) | M-VAVE's official open-source firmware, with an SDK and web Workbench (`open.m-vave.com/fm1-workbench`). 8-track sequencer, 6 engines. |
| **Hortator**, **JIANT** | GPL-3.0 | Felucca forks: useful examples of turning Felucca into an application with a different interface. |

**Recommended choice: fork Felucca**, because the web/SDL build lets almost everything be developed without flashing the device, and there are already forks that completely repurpose it. Claude Code must still compare the two bases in Phase 0 and justify the choice.

### Installation and recovery
- The installable package is a `.fwsc` file; every firmware has an "identity" (e.g. `FM-1_910` for Felucca). **The project must use its own identity** that doesn't conflict with other firmware (verify how it's chosen).
- Installation via web installer (Chrome/Edge, WebUSB) or `tools/fm1_install.py`.
- To go back to the official firmware there's M-VAVE's M-Updater.
- If an update is interrupted, the chip stays in boot mode (USB device `WL80UBOOT`, ID `4C4A:8057`). Recovery is documented by Felucca (FM-1 Transporter with a Seeed XIAO RP2040).

---

## 3. Features

### 3.1 Exercise types (MVP)

1. **Intervals**
   - Modes: ascending, descending, harmonic (simultaneous), mixed.
   - Selectable set: from minor 2nd to octave, then optionally up to the 15th.
   - Random starting note within a configurable range.
2. **Chord qualities**
   - Triads: major, minor, diminished, augmented, sus2, sus4.
   - Sevenths: maj7, 7, m7, m7b5, dim7 (advanced level).
   - Optional: root position / inversions; block or arpeggiated playback.
3. **Scale degrees in context**
   - The synth establishes the key (I–IV–V–I cadence or a drone on the tonic), then plays a note; the user must say which degree it is.
   - Major first, then minor and chromaticism.

### 3.2 Later exercises (post-MVP)
- Short melodic dictation (3–5 notes, to be repeated on the keybed).
- Harmonic progressions (recognizing I–IV–V–vi etc.).
- Scale and mode recognition.
- Optional: pitch reference ("play an A").

### 3.3 Answer modes
- **Playing**: the user plays the answer on the keybed (e.g. the interval's second note). Option to accept the note in any octave.
- **Choosing**: the possible answers are assigned to keys (e.g. 12 keys = 12 intervals, labeled on the display) or chosen from a list with an encoder.
- Each exercise declares which modes it supports.

### 3.4 Question flow
1. The synth plays the question.
2. The user can replay it (dedicated button) as many times as they want; replays are counted.
3. The user answers.
4. Immediate feedback: right/wrong, on the display and with an optional brief sound cue; if wrong, the correct answer is shown and played, with the option to compare it against the one given.
5. Move on to the next question (automatically or with a key, configurable).

### 3.5 Sessions, levels, statistics
- Session of N questions (10/20/50/infinite) with a final summary: percentage, average response time, most frequent mistakes.
- Persistent per-item statistics (e.g. "descending minor 6th: 14/20").
- **Adaptive selection**: items missed more often come up more frequently (simple, documented weighting).
- Predefined levels per exercise (e.g. Intervals L1 = M3, P5, octave; L2 = adds P4, M6…) plus a "custom" level.
- Reset statistics from the menu, with confirmation.

### 3.6 Sound
- Choice of timbre among a few FM presets suited for listening (piano, electric piano, organ, pure sine).
- Adjustable note duration and tempo.
- Feedback-signal volume separate from the question's.
- Fixed velocity for questions, so as not to give hints.

### 3.7 Persistent settings
Last exercise, level, answer mode, range, timbre, tempo, statistics. Saving must be robust to sudden power loss (battery): verify how the chosen base handles this.

### 3.8 Language
Display text in English by default, with a structure that allows adding Italian (string table). Selectable note names: C D E… or Do Re Mi…

---

## 4. Required architecture

- **Separate exercise core** (`trainer/` or similar), in plain C, with no hardware dependencies:
  - question generator (for each exercise type);
  - answer evaluation;
  - session state machine;
  - statistics and adaptive selection;
  - pseudo-random generator with settable seed (reproducible tests).
- **Interface layer**: display rendering and control mapping, on top of the graphics and input APIs already present in the base.
- **Audio layer**: uses the existing FM engine through its note on/off APIs; no new sound engine.
- The core's APIs must be documented in the headers.
- Keep the new code separate from the base's as much as possible, to make it easier to update to upstream code (merging new Felucca/FM1 Open versions).

---

## 5. Control mapping

**Don't invent button names.** In Phase 0, Claude Code must derive the real list of keys, buttons and encoders from the code (and from Felucca's wiki Controls/Layout pages), then propose a mapping table for Trainer mode, to be approved before implementing it.

Functions the mapping must cover:
- entering/exiting Trainer mode;
- choosing exercise and level;
- replaying the question;
- confirming / moving to the next question;
- showing the answer;
- opening settings and statistics;
- scrolling through options (encoder).

---

## 6. Phased work plan

At the end of each phase, Claude Code stops, summarizes what it did, and waits for confirmation before proceeding.

### Phase 0: groundwork (no code)
- Clone Felucca (`git clone --recurse-submodules`) and FM1 Open.
- Read README, BUILDING.md, docs/ARCHITECTURE.md, PORTING.md, licenses.
- Produce `docs/RECON.md` with:
  - comparison of the two bases and a justified choice;
  - how a new mode/screen is added;
  - display, input, audio, storage APIs;
  - the real list of controls and a proposed mapping;
  - the toolchain needed for the hardware build and how to get it;
  - how the firmware identity is assigned;
  - identified risks.

### Phase 1: exercise core + tests
- Implement the core (section 4) for the three MVP exercises.
- Unit tests runnable on the host (`./build.sh test` or equivalent): question generation, evaluation, adaptive selection, statistics serialization.

### Phase 2: emulator interface
- Integrate Trainer mode into the web or SDL build.
- The full flow (section 3.4) must be usable in the browser or on the desktop.

### Phase 3: hardware build and installation
- Build the `.fwsc` with its own identity.
- Documented installation procedure, **with a device backup before flashing**.
- Manual test checklist on the device.

### Phase 4: polish
- Advanced statistics, Italian strings, later exercises (section 3.2), web editor/installer if useful.

---

## 7. Acceptance criteria (MVP)

- The three MVP exercises work on the device with both planned answer modes.
- Normal synth mode is still usable.
- Settings and statistics survive power-off and power-on.
- No crash or audio lockup in a 100-question session.
- Unit tests pass on the host.
- Returning to the official firmware with M-Updater (verified and documented procedure).
- README with installation, controls, exercises and recovery procedure.

---

## 8. Constraints and rules

- **License**: the derived project stays GPL-3.0; keep the base's copyright and attributions.
- **Device safety**: never run flash or write commands on the device without explicit confirmation. Remember to back up before every flash.
- Do not modify the bootloader or flash areas outside those the chosen base normally uses.
- Small, descriptive commits; one branch per phase.
- When information in this document turns out to be wrong relative to the code, the code wins: flag it and update this file.

---

## 9. References

- Felucca: https://github.com/hugelton/Felucca (wiki: Installation, Controls, Layout, Troubleshooting)
- Felucca-Engines (plugin ABI, MIT): Felucca submodule
- FM1 Open: https://github.com/m-vave/fm1_open
- FM-1 Workbench: https://open.m-vave.com/fm1-workbench
- Hortator (Felucca fork, reuse example): https://github.com/DeadActive/hortator
- JIANT (Felucca fork): https://github.com/juanjiant-bit/JIANT
- Community firmware list: https://fm1-editor.com/firmware/
- Official product/firmware page: https://www.m-vave.com/product?id=fm-1
