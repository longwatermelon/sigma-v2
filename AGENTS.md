# Repository Guidelines

## Project Structure & Module Organization
Core sources live in `src/`: `main.cpp` orchestrates clip generation, `video.*` handles timeline rendering, `source.h` enumerates event builders, and `util.h` centralizes math/macros (`vec`, `sz`, frame converters). Vendor headers (e.g., nlohmann/json) sit under `deps/`. Runtime assets reside in `res/` (`audio/`, `video/`, `font.ttf`), and renders drop into `out/`. Auxiliary build metadata lives in `compile_commands.json`, while `run.sh` sequences YouTube/Instagram exports.

## Build, Test, and Development Commands
`make target` compiles all C++ sources with OpenCV4, curl, and the bundled headers; ensure `pkg-config` can locate `opencv4`. Run a format by executing `./a.out <format_id> [variant] [prompt]`, or use `./run.sh "caption"` to batch-generate YouTube/Instagram outputs. Clean stale artifacts manually by removing `a.out`, `out/*.mp4`, and temporary WAV files if needed.

## Coding Style & Naming Conventions
Follow the existing C++17 style: 4-space indentation, brace-on-new-line for functions, and snake_case for helpers (`evt_bg`, `draw_glowing_text`). Reuse the macro/util palette in `util.h` (`W`, `H`, `FPS`, `frm2t`). Prefer `vec<T>` and STL algorithms over raw loops where practical. Keep headers self-contained with `#pragma once`, and place project includes before system headers.

## Testing Guidelines
No automated suite exists; lean on targeted renders. Before proposing changes, run `make target` and validate representative timelines (e.g., `./a.out 7` for standard edits) to confirm ETA logging, caption placement, and audio extraction. Inspect `out/` outputs and console traces for timing regressions or asset copy failures. Update or regenerate affected assets when formats or IDs shift.

## Commit & Pull Request Guidelines
Commits should stay short, imperative, and scoped (history favors concise bodies like `rm speed parameter`). Reference asset or format IDs in the subject when relevant. Pull requests must describe the scenario covered, list new commands or flags, acknowledge dependencies on external asset drops, and attach sample output paths or thumbnails so reviewers can verify visuals quickly.

## Assets & Configuration Notes
Download the full `res` tree before building; OpenCV freetype loads `res/font.ttf`, and SFX copying assumes writable `out/`. Keep `out/` out of version control by default and avoid committing third-party media. Document any new environment variables or API keys in `README.md` and provide sanitized examples under `res/` when feasible.
