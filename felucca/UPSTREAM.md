# Upstream

This directory is a vendored, modified copy of [Felucca](https://github.com/hugelton/Felucca)
(GPL-3.0-only), forked for the FM-1 ear-trainer project. The base firmware's copyright,
license texts and attributions (`LICENSE`, `LICENSING.md`, `LICENSES/`, `CREDITS.md`) are kept
as they are upstream.

Forked from:

| Component | Upstream repo | Commit |
|---|---|---|
| Felucca (app) | https://github.com/hugelton/Felucca | `e4b1380b11b389a24dfada97c233ef93ac65893f` (2026-11-23) |
| firmware/bsp | https://github.com/hugelton/felucca-bsp | `9670b5d34082700d0a7413eb6264abe1756c0ece` |
| firmware/engines | https://github.com/hugelton/Felucca-Engines | `1fa016b9702a76e2767b2bd9f108333f7c46e592` |

New, project-specific code lives in files named `trainer_*` (e.g. `firmware/src/trainer_bridge.c`)
and in `../trainer/` (the hardware-independent core, outside this tree entirely). Touch-points in
the base files themselves are kept minimal and are listed in `../docs/` as they are added, to make
diffing against a future upstream Felucca version easier.
