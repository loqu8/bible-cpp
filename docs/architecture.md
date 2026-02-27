# Architecture

## Design Principles

1. **Plugin-first**: No hardcoded knowledge of any specific text. KJV, CUV, Synodal, Tao Te Ching — all are plugins.
2. **Zero dependencies**: Pure C++20. No Boost, no nlohmann-json, no std::filesystem. SQLite is the only external dependency (vendored or system).
3. **AI/MCP-ready**: Every query returns structured metadata (timing, counts, statistics). Designed for agent consumption from day one.
4. **Cross-platform**: Compiles on Linux, macOS, Windows, iOS, Android, WASM, and 8 more platforms via Guix cross-compilation.
5. **Extensible**: Adding a new translation requires only creating a SQLite database and calling `register_plugin()`.

## Plugin Architecture

```
ScriptureEngine
├── Plugin Registry
│   ├── "kjv" → KJV SQLite (en, has_strongs=true)
│   ├── "cuv_simp" → CUV Simplified SQLite (zh, has_strongs=true)
│   ├── "cuv_trad" → CUV Traditional SQLite (zh, has_strongs=true)
│   ├── "synodal" → Russian Synodal SQLite (ru, has_strongs=false)
│   └── "tao" → Tao Te Ching SQLite (zh+en, has_strongs=false)
├── Strong's Lexicon
│   └── strongs.sqlite (TBESH Hebrew + TBESG Greek)
├── Reference Parser
│   └── Book name tables built from registered plugins
└── FTS5 Search Engine
    └── Virtual tables spanning all plugin texts
```

## Data Flow

### Registration
1. User calls `register_plugin("kjv", "kjv.sqlite", config)`
2. Engine opens SQLite, verifies schema
3. Adds book names/abbreviations to the reference parser's lookup table
4. Creates FTS5 virtual table over the plugin's verses

### Navigation
1. `get_chapter(43, 3)` → queries all registered plugins for John 3
2. Returns `vector<ParallelVerse>` — each verse has parallel texts from all plugins
3. QueryStats attached: timing, verse count, translations included

### Search
1. `search("love")` → FTS5 query across all plugin FTS tables
2. Results ranked by relevance, deduplicated by (book, chapter, verse)
3. Each result includes the matching plugin, verse text, and relevance score

### Strong's Cross-Reference
1. `get_verses_with_strongs('G', 26)` → finds all verses with agape
2. Scans tagged text in all plugins with `has_strongs=true`
3. Returns parallel verses across translations for that Strong's number

## Legacy Schema Compatibility

The legacy Tao/Bible schema from intuition-2019 used:
- `books(bid, book, logos, mla, cms, cms_s, sbl, b5, gb)` — multiple citation styles
- `verses(vid, ch, vn, hb5, hgb, kjv, esv)` — all translations in one table

bible-cpp normalizes this: each translation is a separate plugin database,
joined at query time by (book, chapter, verse) coordinates. This is more
extensible (adding a translation doesn't require schema changes) and cleaner
for cross-platform distribution (ship only the translations you need).

## Threading Model

ScriptureEngine is NOT thread-safe. This follows the nomad-builder convention:
- Create one engine per thread
- Or synchronize externally with a mutex

SQLite itself handles concurrent readers safely via WAL mode, so multiple
engines can share the same database files.

## Memory Management

- Plugin databases: opened on `register_plugin()`, closed on `unregister_plugin()` or engine destruction
- Query results: returned by value (move semantics)
- No global state, no singletons
- RAII throughout via unique_ptr pimpl

## Integration with nomad-builder

When consumed by nomad-builder, bible-cpp sits at Layer 1:

| Layer | What | Owns |
|-------|------|------|
| Layer 1 (`third_party/bible-cpp/`) | This library | Algorithm, types, SQLite queries |
| Layer 2 (`src/libbible/`) | Nomad wrapper | `loqu8::bible` namespace, nomad conventions |
| Layer 3 (`src/libbible_capi/`) | C API | Opaque handles, FFI for Flutter/.NET/Python/WASM |

The CAPI follows the strokematch_capi pattern:
- `BIBLE_API` export macro
- Opaque handle: `typedef struct bible_engine* bible_engine_t;`
- Error codes: `BIBLE_SUCCESS 0`, `BIBLE_ERROR_INVALID -1`, etc.
- All strings are `const char*` (caller-owned)
- Timestamps as `int64_t`
