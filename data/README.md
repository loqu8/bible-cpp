# Data Sources

This directory contains the generated scripture databases and the build tool.

## Quick Start

```bash
# Auto-download all sources and build all 4 databases
python3 tools/build_bible_db.py

# Custom output directory
python3 tools/build_bible_db.py --output-dir /path/to/output

# Use pre-downloaded raw files (cached in data/raw/ by default)
python3 tools/build_bible_db.py --data-dir /path/to/cached/raw
```

## Output Databases

| File | Content | Verses | Strong's |
|------|---------|--------|----------|
| `kjv.sqlite` | King James Version (English) | 31,102 | Yes |
| `cuv_simp.sqlite` | Chinese Union Version — Simplified | 31,100 | Yes |
| `cuv_trad.sqlite` | Chinese Union Version — Traditional | 31,100 | Yes |
| `strongs.sqlite` | Hebrew + Greek Strong's lexicon | — | 19,570 entries |

All databases conform to the bible-cpp plugin schema (see [../README.md](../README.md)).

## Sources

| Dataset | Source | License |
|---------|--------|---------|
| KJV + Strong's | [Bible SuperSearch](https://github.com/aicwebtech/biblesupersearch_api) | Public Domain |
| CUV Simplified + Strong's | [Bible SuperSearch](https://github.com/aicwebtech/biblesupersearch_api) | Public Domain |
| CUV Traditional + Strong's | [Bible SuperSearch](https://github.com/aicwebtech/biblesupersearch_api) | Public Domain |
| Hebrew Strong's (TBESH) | [STEPBible-Data](https://github.com/STEPBible/STEPBible-Data) | CC BY 4.0 |
| Greek Strong's (TBESG) | [STEPBible-Data](https://github.com/STEPBible/STEPBible-Data) | CC BY 4.0 |
| Pronunciation data | [Bible SuperSearch](https://github.com/aicwebtech/biblesupersearch_api) | Public Domain |

## Adding Your Own Translation

See the plugin schema in [../README.md](../README.md). Any SQLite database
with `books` and `verses` tables conforming to the schema can be registered
as a plugin.

Example: to add the Russian Synodal Bible, create `synodal.sqlite` with:

```sql
CREATE TABLE books (
    id INTEGER PRIMARY KEY,
    name_en TEXT NOT NULL,
    name_zh TEXT NOT NULL,
    name_zh_trad TEXT NOT NULL,
    abbr_en TEXT NOT NULL,
    testament TEXT NOT NULL,
    canon TEXT NOT NULL DEFAULT 'protestant',
    chapter_count INTEGER NOT NULL
);

CREATE TABLE verses (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    book INTEGER NOT NULL REFERENCES books(id),
    chapter INTEGER NOT NULL,
    verse INTEGER NOT NULL,
    text TEXT,
    text_plain TEXT,
    UNIQUE(book, chapter, verse)
);
```

Populate with your translation data, then register the plugin:

```cpp
engine.register_plugin("synodal", "/path/to/synodal.sqlite", {
    .language = "ru",
    .name = "Synodal Translation",
    .has_strongs = false
});
```
