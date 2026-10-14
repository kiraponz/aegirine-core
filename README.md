# aegirine-core

The PlayStation (PS1) emulation core of Aegirine, an Android PS1 emulator.

This is a fork of [DuckStation](https://github.com/stenzek/duckstation) by Connor McLaughlin
and contributors, taken at commit `14ab642ff3d5d489d388c8062530aa2f7c9a1372` (2024-09-01),
the last commit before DuckStation moved to a non-free license. It is not affiliated with or
endorsed by the DuckStation project.

## License

GNU General Public License, version 3 ([LICENSE](LICENSE)).

Most source files carry the header `SPDX-License-Identifier: (GPL-3.0 OR CC-BY-NC-ND-4.0)`
(a few `(GPL-3.0 OR PolyForm-Strict-1.0.0)`); this fork takes them under GPL-3.0. Files under
`dep/` keep their own licenses.

Nothing from DuckStation after the fork point is merged into this repository.

## Changes from upstream

The Android frontend lives in Aegirine. This repository holds the changes the core needs for
it, such as the Android EGL context and leaving out the parts Aegirine does not use.

PlayStation BIOS images and game images are not included and must not be added.

"PlayStation" and "PSX" are registered trademarks of Sony Interactive Entertainment Europe
Limited. This project is not affiliated in any way with Sony Interactive Entertainment.
