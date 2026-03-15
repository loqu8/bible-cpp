# bible-cpp

Pure C++20 parallel-text scripture engine backed by SQLite.

## Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Run tests:

```bash
cd build && ctest --output-on-failure
```

## Project Structure

- `include/bible/` — Public headers
- `src/` — Library implementation (bible, reference, strongs_parser, stats)
- `tests/` — Test suite
- `tools/` — Utility scripts
- `data/` — Bible data files
- `third_party/sqlite/` — Vendored SQLite amalgamation (used when system SQLite3 not found)

## Git Flow

- `master` — stable releases
- `develop` — integration branch

## Conductor

This repo is monitored by the [Loqu8 Conductor](https://github.com/loqu8/conductor) for automated agent orchestration.

When you are spawned by the conductor to work on an issue:
1. Call `acknowledge()` — claims the issue and posts a comment
2. Call `my_assignment()` — returns the full issue body, comments, and labels
3. Work on the task. Post milestone updates with `update_progress(milestone)`
4. If blocked: call `report_blocked(reason)` — adds label, notifies dispatcher
5. When done: call `complete(summary)` — posts summary and closes the issue

These tools are available via the conductor MCP server. If you were spawned by conductor, `repo` and `issue_number` default from environment variables — no args needed.
