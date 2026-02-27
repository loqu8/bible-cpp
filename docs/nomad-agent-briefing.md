# nomad-builder Agent Briefing: bible-cpp Integration

## What Exists

`loqu8/bible-cpp` (https://github.com/loqu8/bible-cpp) is a new MIT-licensed pure C++20 library — a parallel-text scripture engine with a plugin architecture. It was created following the same pattern as `fsrs-cpp`: standalone Layer 1 library, zero dependencies, designed for nomad-builder consumption.

### Architecture

The core insight: **no hardcoded Bible knowledge**. Each translation (KJV, CUV-Simplified, CUV-Traditional, Russian Synodal, even Tao Te Ching) is a SQLite database plugin implementing a standard schema. The engine provides:

- **Plugin registry**: `register_plugin("kjv", "kjv.sqlite", {.language="en", .has_strongs=true})`
- **Navigation**: `get_books()`, `get_chapter(book, ch)`, `get_verse(book, ch, v)` — returns parallel texts from all plugins
- **Reference parsing**: `"John 3:16"` → `(book=43, chapter=3, verse=16)` — extensible for any locale (Chinese, Russian, etc.)
- **Strong's concordance**: Hebrew (H1-H8674) + Greek (G1-G5624) lexicon with lemma, transliteration, morph, gloss, definition
- **Full-text search**: FTS5 across all registered parallel texts
- **Statistics**: `compute_stats()` — AI/MCP-ready metadata on every query

### Plugin Database Schema

```sql
-- Per-translation database (e.g., kjv.sqlite, cuv_simp.sqlite)
CREATE TABLE books (
    id INTEGER PRIMARY KEY,
    name TEXT NOT NULL,
    name_alt TEXT,
    abbr TEXT NOT NULL,
    testament TEXT CHECK(testament IN ('OT', 'NT', NULL)),
    chapter_count INTEGER NOT NULL
);

CREATE TABLE verses (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    book INTEGER NOT NULL REFERENCES books(id),
    chapter INTEGER NOT NULL,
    verse INTEGER NOT NULL,
    text TEXT,          -- may contain {H/G####} Strong's markers
    text_plain TEXT,    -- stripped
    UNIQUE(book, chapter, verse)
);
```

### Strong's Lexicon Schema

```sql
CREATE TABLE strongs (
    number INTEGER NOT NULL,
    type TEXT NOT NULL CHECK(type IN ('H', 'G')),
    language TEXT NOT NULL DEFAULT 'Hebrew',
    lemma TEXT, translit TEXT, pronunciation TEXT,
    morph TEXT, gloss TEXT, definition TEXT,
    PRIMARY KEY(number, type)
);
```

### Data Sources (all clean licensing)

| Data | Source | License |
|------|--------|---------|
| Library code | loqu8/bible-cpp | MIT |
| Strong's Hebrew/Greek lexicon | STEPBible TBESH/TBESG | CC BY 4.0 |
| KJV text | Public domain (1611) | PD |
| CUV text | Public domain (1919) | PD |

STEPBible data format: TSV with 8 columns (eStrong, dStrong, uStrong, lemma, transliteration, morph, gloss, meaning). Download from `github.com/STEPBible/STEPBible-Data/tree/master/Lexicons`.

CUV with Strong's alignment from Bible SuperSearch (biblesupersearch.com) — software is GPL but Bible text is explicitly public domain.

### What's Implemented

- Full public API in headers (`include/bible/`)
- `ScriptureEngine` class with pimpl pattern
- Strong's text parser (`{H7225}beginning` → ParsedWord with strongs_number=7225)
- Test framework with 7 parser tests
- CMake build system
- Architecture and data-source documentation

### What's NOT Implemented (skeleton/TODO)

- SQLite query layer (all navigation/search methods return empty)
- FTS5 search
- Reference parser (book name tables)
- Statistics computation
- Data build tools (Python scripts referenced in data/README.md)

## Three-Layer Integration

Follow the pattern from `open-source-library-pattern.md` exactly:

| Layer | Location | Status |
|-------|----------|--------|
| 1 | `third_party/bible-cpp/` | Clone from `loqu8/bible-cpp` |
| 2 | `src/libbible/` | Create — wrap in `loqu8::bible` namespace |
| 3 | `src/libbible_capi/` | Create — follow `strokematch_capi` pattern exactly |

### Suggested CAPI Surface (Layer 3)

```c
// Lifecycle
bible_engine_t bible_create(void);
int bible_init_strongs(bible_engine_t, const char* strongs_db_path);
int bible_register_plugin(bible_engine_t, const char* id, const char* db_path,
                          const char* language, const char* name, int has_strongs);
void bible_destroy(bible_engine_t);

// Navigation
int bible_get_books(bible_engine_t, const char* plugin_id, const char* testament,
                    const bible_book_t** out, size_t* count);
int bible_get_chapter(bible_engine_t, int book, int chapter,
                      const bible_parallel_verse_t** out, size_t* count);
int bible_get_verse(bible_engine_t, int book, int chapter, int verse,
                    bible_parallel_verse_t* out);

// Strong's
int bible_get_strongs(bible_engine_t, char type, int number, bible_strongs_t* out);
int bible_get_verses_with_strongs(bible_engine_t, char type, int number, int limit,
                                  const bible_parallel_verse_t** out, size_t* count);
int bible_count_verses_with_strongs(bible_engine_t, char type, int number);

// Search
int bible_search(bible_engine_t, const char* query, int limit,
                 const bible_search_result_t** out, size_t* count);

// Reference parsing
int bible_parse_reference(bible_engine_t, const char* ref, bible_reference_t* out);

// Statistics
int bible_compute_stats(bible_engine_t, bible_corpus_stats_t* out);

// Strong's text parsing (static, no engine needed)
int bible_parse_strongs_text(const char* text,
                             const bible_parsed_word_t** out, size_t* count);
```

### Legacy Compatibility Note

The old Tao/Bible schema from intuition-2019 had `books(bid, book, logos, mla, cms, b5, gb)` + `verses(vid, ch, vn, hb5, hgb, kjv, esv)` — all translations in one table with multiple columns. The new design normalizes this: each translation is a separate plugin database, joined at query time. The build tools should convert the legacy format if the tao_src.db3 is ever recovered.

### Key Design Decisions

1. **Plugin-first extensibility**: A Russian speaker adds `synodal.sqlite` and calls `register_plugin()`. No code changes needed.
2. **Strong's from STEPBible (CC BY 4.0)** instead of OpenScriptures (no license): Clean licensing for MIT.
3. **MCP-ready**: `compute_stats()`, `QueryStats` on every result, structured metadata throughout.
4. **Same threading model as nomad**: NOT thread-safe, one engine per thread.
