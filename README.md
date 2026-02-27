# bible-cpp

**Pure C++20 parallel-text scripture engine with plugin architecture.**

A zero-dependency, cross-platform library for navigating, searching, and cross-referencing parallel scripture texts. Each translation (KJV, CUV, Synodal, Tao Te Ching, etc.) is a database plugin — the core engine has no hardcoded knowledge of any specific text.

## Features

- **Plugin architecture** — register any parallel text as a `ScripturePlugin`. Ship KJV, CUV, Russian Synodal, or Tao Te Ching. Add your own.
- **Reference parsing** — `"John 3:16"` → `(book=43, chapter=3, verse=16)`. Supports English, Chinese, and extensible locale abbreviations.
- **Strong's concordance** — Hebrew (H1-H8674) and Greek (G1-G5624) lexicon with lemma, transliteration, morphology, gloss, and full definition. Data from [STEPBible](https://github.com/STEPBible/STEPBible-Data) (CC BY 4.0).
- **Full-text search** — FTS5 across all registered parallel texts simultaneously.
- **Cross-referencing** — tap a Strong's number to find every verse using that word across all translations.
- **Statistics** — word frequency, Strong's usage counts, per-book/chapter verse counts. AI/MCP-ready metadata on every query result.
- **Zero dependencies** — pure C++20, no Boost, no nlohmann-json, no std::filesystem. Compiles on 14 platforms via Guix cross-compilation.

## Architecture

```
┌─────────────────────────────────────────┐
│           ScriptureEngine               │
│  navigate, search, reference parsing    │
├─────────────────────────────────────────┤
│         IScripturePlugin                │
│  register text databases as plugins     │
├──────────┬──────────┬───────────────────┤
│   KJV    │  CUV-S   │  CUV-T  │  ...   │
│ (plugin) │ (plugin) │ (plugin)│(plugin) │
└──────────┴──────────┴─────────┴─────────┘
         ↕               ↕
┌─────────────────────────────────────────┐
│         Strong's Lexicon                │
│  Hebrew + Greek concordance data        │
│  (STEPBible TBESG/TBESH, CC BY 4.0)    │
└─────────────────────────────────────────┘
```

Each plugin is a SQLite database conforming to a standard schema. The engine queries across all registered plugins transparently.

### Three-Layer Integration (nomad-builder)

When consumed by [nomad-builder](https://github.com/nicholasq/nomad-builder), bible-cpp follows the standard three-layer pattern:

| Layer | Location | What |
|-------|----------|------|
| 1 | `third_party/bible-cpp/` | This repo — standalone MIT library |
| 2 | `src/libbible/` | Nomad wrapper in `loqu8::bible` namespace |
| 3 | `src/libbible_capi/` | C API for FFI (Flutter, .NET, Electron, Python, WASM) |

## Data Sources

| Dataset | Source | License | Notes |
|---------|--------|---------|-------|
| KJV text | Public domain (1611) | **Public Domain** | King James Version |
| CUV text | Public domain (1919) | **Public Domain** | Chinese Union Version (simplified + traditional) |
| Strong's Concordance | James Strong (1890) | **Public Domain** | Original concordance |
| Strong's Hebrew Lexicon (TBESH) | [STEPBible](https://github.com/STEPBible/STEPBible-Data) | **CC BY 4.0** | Tyndale House, Cambridge |
| Strong's Greek Lexicon (TBESG) | [STEPBible](https://github.com/STEPBible/STEPBible-Data) | **CC BY 4.0** | Tyndale House, Cambridge |
| CUV Strong's alignment | [Bible SuperSearch](https://biblesupersearch.com) | **Public Domain** (text) | Alignment-based tagging |

**Attribution**: Strong's lexicon data from "Tyndale House, Cambridge" ([tyndaleHouse.com](https://www.tyndaleHouse.com)) and "STEP Bible" ([STEPBible.org](https://www.STEPBible.org)). Source: [STEPBible-Data](https://github.com/STEPBible/STEPBible-Data).

## Database Schema

### Plugin Schema (per-translation database)

```sql
CREATE TABLE books (
    id INTEGER PRIMARY KEY,           -- 1-66 for Protestant Bible
    name TEXT NOT NULL,                -- primary name (language of this plugin)
    name_alt TEXT,                     -- alternate name
    abbr TEXT NOT NULL,                -- abbreviation for reference parsing
    testament TEXT CHECK(testament IN ('OT', 'NT', NULL)),
    chapter_count INTEGER NOT NULL
);

CREATE TABLE verses (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    book INTEGER NOT NULL REFERENCES books(id),
    chapter INTEGER NOT NULL,
    verse INTEGER NOT NULL,
    text TEXT,                         -- verse text (may contain Strong's markers)
    text_plain TEXT,                   -- stripped text (no markers)
    UNIQUE(book, chapter, verse)
);

CREATE INDEX idx_verses_book_ch ON verses(book, chapter);
```

### Strong's Lexicon Schema

```sql
CREATE TABLE strongs (
    number INTEGER NOT NULL,
    type TEXT NOT NULL CHECK(type IN ('H', 'G')),
    language TEXT NOT NULL DEFAULT 'Hebrew'
        CHECK(language IN ('Hebrew', 'Aramaic', 'Greek')),
    lemma TEXT,                        -- original word (אֱלֹהִים / λόγος)
    translit TEXT,                     -- transliteration
    pronunciation TEXT,
    morph TEXT,                        -- morphological classification
    gloss TEXT,                        -- one-word gloss
    definition TEXT,                   -- full lexical definition
    PRIMARY KEY(number, type)
);

CREATE INDEX idx_strongs_type ON strongs(type);
```

## Adding a New Translation

To add a Russian Synodal Bible (or any other text):

1. Create a SQLite database conforming to the plugin schema above
2. Populate `books` with Russian book names and abbreviations
3. Populate `verses` with the text (optionally with `{H1234}` Strong's markers)
4. Register the plugin:

```cpp
#include <bible/bible.hpp>

bible::ScriptureEngine engine;
engine.register_plugin("synodal", "/path/to/synodal.sqlite", {
    .language = "ru",
    .name = "Synodal Translation",
    .has_strongs = false
});
```

The engine handles navigation, search, and cross-referencing automatically.

## Building

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
ctest
```

### Requirements

- C++20 compiler (GCC 12+, Clang 15+, MSVC 2022+)
- CMake 3.20+
- No external dependencies (SQLite is vendored or expected via find_package)

## MCP / AI Agent Integration

bible-cpp is designed for AI agent consumption. Every query returns structured metadata:

```cpp
auto result = engine.search("love", {.limit = 10});
// result.stats.query_time_ms, result.stats.total_matches
// result.stats.books_matched, result.stats.translations_searched

auto stats = engine.compute_stats();
// stats.total_verses, stats.total_books, stats.plugins_registered
// stats.strongs_entries, stats.search_index_size
```

When wrapped in an MCP server (Layer 3 CAPI → MCP tools), this enables:

| MCP Tool | What It Does |
|----------|-------------|
| `bible_get_books` | List all books with chapter counts |
| `bible_get_chapter` | Get full chapter across all translations |
| `bible_get_verse` | Get single verse with all translations + Strong's |
| `bible_search` | Full-text search across all registered texts |
| `bible_get_strongs` | Look up Strong's concordance entry |
| `bible_get_stats` | Corpus statistics, word frequency, coverage |
| `bible_navigate` | Navigate UI to specific passage |
| `bible_parse_ref` | Parse "John 3:16" → structured coordinates |

## Project Status

**Phase**: Engine complete, data pipeline operational. Next: nomad-builder integration.

- [x] Architecture design (plugin model, schema, CAPI pattern)
- [x] Core types and interfaces (`types.hpp`, `scripture_plugin.hpp`)
- [x] Reference parser (`reference.hpp/cpp`) — English, Chinese, abbreviations
- [x] SQLite query layer (navigation, search, parallel text)
- [x] Strong's lexicon integration (STEPBible TBESG/TBESH)
- [x] Build tools (`tools/build_bible_db.py` — auto-downloads + builds 4 databases)
- [x] Statistics / analytics (AI/MCP-ready corpus metadata)
- [x] Tests (37 passing — strongs parser + full engine integration)
- [ ] nomad-builder integration (Layer 2 + Layer 3 CAPI)

## License

**Code**: MIT License — see [LICENSE](LICENSE).

**Data**: Strong's lexicon data is [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/) from [STEPBible-Data](https://github.com/STEPBible/STEPBible-Data) by Tyndale House, Cambridge. Bible texts (KJV, CUV) are public domain.
