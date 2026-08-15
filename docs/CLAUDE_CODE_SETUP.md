# Claude Code setup for this repository

What was configured, why, and what each developer has to install locally.
Written 2026-07-26 against SDK 0.1.1.

---

## What a developer needs on their machine

One thing, and it is not optional:

**A `clangd` shim at `~/.local/bin/clangd`.** The clangd-lsp plugin invokes a bare
`clangd`. A Linux clangd cannot resolve the MSVC STL or the Windows SDK, so it must be
the Windows `clangd.exe` — and `clangd.exe` cannot open WSL paths, so it needs
`--path-mappings`:

```sh
#!/bin/sh
exec "/mnt/c/Users/<you>/AppData/Local/Programs/CLion/bin/clang/win/x64/bin/clangd.exe" \
     --path-mappings=/mnt/c=C:/,/mnt/d=D:/,/mnt/h=H:/ "$@"
```

Both sides of a mapping must be absolute: `C:` is rejected, `C:/` is accepted. One
entry per drive the checkouts live on — they are on `H:` now (`/mnt/h`), and a drive
with no entry makes `clangd.exe` reject every LSP URI for it. Any
`clangd.exe` will do; the CLion-bundled one is just what is already installed here. The
same shim serves Quantiloom-dev — if that repo's code intelligence works, this one will
too, with nothing further to install.

Then generate the compilation database once:

```bash
./scripts/gen_compile_commands.sh        # ~7 s
```

Everything else — Qt 6.10.1, Vulkan SDK, MSVC 18, the Quantiloom SDK — was already
required to build.

---

## What was added

Five commits, `347a90d..c7a7b01`.

### 1. Layered `CLAUDE.md`

There was none before, so every session rediscovered the same handful of facts.

| File | Lines | Loaded |
|---|---|---|
| `CLAUDE.md` | 58 | every session |
| `src/panels/CLAUDE.md` | 29 | on demand |
| `src/config/CLAUDE.md` | 29 | on demand |
| `src/vulkan/CLAUDE.md` | 29 | on demand |
| `src/i18n/CLAUDE.md` | 27 | on demand |

Standing cost is the root file alone: **2,914 bytes, roughly 700 tokens**. The other
5,022 bytes are charged only when work actually touches that directory, so editing a
panel never pays for the renderer's rules.

The root file holds what is true everywhere — the two build commands with measured
times, the cross-repo SDK relationship, the repo map, the commit convention. Anything
that only matters inside one directory went into that directory. Four directories
earned a file; `src/editing/` and `src/dialogs/` did not, because nothing about them is
surprising, and a file that only says "widgets live here" costs more than it returns.

Every command in these files was executed before being written down. The one that
mattered most was `lupdate`: running it the obvious way adds 544 `<location>` elements
per `.ts` file, and the repo's files have none. The documented invocation includes
`-locations none`, verified by restoring the originals and confirming
`quantiloom_zh_CN.ts` came back byte-identical.

### 2. Read denials for checked-in binary assets

`.gitignore` already keeps content search out of the build trees, so nothing was added
for those. What `.gitignore` does not cover is `assets/`, and that is where the weight
is: of **2,587 files visible to an agent, 2,497 are binary assets** — 2,415 of them the
1.3 GB glTF-Sample-Assets submodule.

Denied: `assets/models/**`, `assets/maps/**`, `assets/spectral/*.json`,
`assets/spectral/*.qlbin`, `assets/luts/*.bin`.

Left readable on purpose: `assets/configs/*.toml` (hand-written, and the core CLI's
input format), `assets/spectral/material_summary_*.csv` and `MANIFEST.sha256` (the
readable side of the baked data, and what the drift check compares against), and
`src/i18n/translations/*.ts` (edited by hand often enough that blocking them would cost
more than it saves).

**The denials reach further than reading.** `ls`, `find` and `test` are refused on a
denied path too, so you cannot check whether a model file exists the obvious way. Use
`du -a assets/models/<dir>`, which lists names and sizes. This is recorded in the
`build-and-run` skill next to the scene-load failure it comes up in.

Sessions are expected to start at the repo root, which is why the patterns are
relative and one `settings.json` is enough. Starting a session from a subdirectory
would silently lose them — `settings.json` is not inherited from parent directories the
way `CLAUDE.md` is.

### 3. A working compilation database

Symbol lookup across 10k lines of C++ was grep. `CMAKE_EXPORT_COMPILE_COMMANDS` is ON
in `CMakeLists.txt` but the Visual Studio generator ignores it, so it had been inert
since the day it was added.

`scripts/gen_compile_commands.sh` configures a second Ninja tree, `build-cdb/`, purely
to emit the database — it is never built. 7 s, 23 entries covering all 21 tracked
sources plus AUTOMOC's `mocs_compilation.cpp` and the qrc. Qt's AUTOMOC and AUTORCC
work under Ninja with no special handling.

`--check` answers whether the database is stale, which nothing else will: clangd falls
back to heuristic flags for a file it cannot find and reports nothing.

`.clangd` points at that tree and adds the MSVC system include paths by hand — those
come from `vcvars64.bat`'s `INCLUDE`, never appear in the database, and cannot be
inferred. Qt, Vulkan and the SDK need no entry; CMake passes them as `-external:I` and
clangd reads them from the database.

Verified with `clangd --check` on the two heaviest translation units: zero diagnostics
on both.

### 4. A staleness hook

Adding a source file is the one change that degrades clangd silently. `PostToolUse` on
`Write|Edit` checks `.cpp`/`.c`/`.cmake`/`CMakeLists.txt` paths only and stays quiet
otherwise. Exercised on real hook payloads across seven cases, including malformed
stdin — a hook that throws on unexpected input would fire on every edit in the session.

### 5. `.gitignore`

`build-cdb/`, `.claude/settings.local.json` and `.claude/SURVEY.md` are per-developer.
`.claude/settings.json` and `.claude/skills/` are shared and now tracked.

Its line endings were also normalised back to LF. The working copy had been converted
to CRLF at some point, which made every commit touching it show as a whole-file
rewrite.

### 6. clang-tidy

Not copied from Quantiloom-dev — its exclusions are justified with measurements taken
there. Measured here instead: dev's check set produces **488 warnings** over the 21
tracked sources, of which five exclusions remove 469. Two of those five differ from
dev's, because a Qt GUI trips different checks than a rendering library:
`misc-include-cleaner` treats `emit`, `QOverload` and `Qt::Checked` as missing includes
(265 warnings, 54%), and `performance-enum-size` wants `uint8_t` for four GUI enums.

What remains is 15 warnings at 14 sites, and five of them are the reason to keep the
file: `DisplayEnhancementPanel::setEnabled`, `RenderSettingsPanel::width` and
`::height`, `SensorPanel::setEnabled` and `::isEnabled` each shadow a `QWidget` method
of the same name. None are virtual, so which one runs depends on the static type of the
pointer — and `SensorPanel::setEnabled(bool)` ticks the sensor box and activates four
parameter groups, while the same call through a `QWidget*` merely makes the widget
interactive. `src/panels/CLAUDE.md` now warns against adding a sixth.

Measured with standalone clang-tidy. **Whether clangd surfaces these while editing is
not verified**: `clangd --check` emits no tidy diagnostics here or in Quantiloom-dev, so
that is a `--check` limitation rather than a misconfiguration — but the in-editor half
rests on dev's claim, not on a test run here.

---

## What was deliberately not done

- **Worktree `sparsePaths`.** Not applicable. 79 tracked files and one CMake target
  leave nothing worth not checking out, and the 1.3 GB that dominates the repo is a
  submodule, which sparse checkout does not govern.
- **A `PostToolUse` linter or type-checker.** There is no lint command here; the only
  check is a full build at 48 s, too expensive to hang on every edit.
- **`SessionStart` and `Stop` hooks.** One codebase, one team — there is no "this area
  belongs to X" to announce, and monitoring a configuration for rot on the day it is
  written is premature. Revisit `Stop` in six months.

---

## Keeping it accurate

- **Review `CLAUDE.md` changes like code.** They rot the same way and for the same
  reasons.
- **Line references are load-bearing.** These files cite `MainWindow.cpp:216`,
  `:895`, `:979`, `ConfigManager.cpp:234`, `QuantiloomVulkanRenderer.cpp:205` and
  `:293`. All six were correct when written. A stale line number is worse than none —
  it sends the reader somewhere confidently wrong.
- **After a major model release, reread these files** and delete anything that exists
  only to work around a previous generation's weaknesses.
- **For a change spanning the SDK boundary**, hand the core-side change and every call
  site to one session so the decisions stay consistent — and have it write the plan to
  a markdown file in the repo before editing. Long sessions compact their context; a
  saved plan survives, a conversation may not.
