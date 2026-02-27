# Data Sources

This directory contains tools and instructions for building the scripture databases.

## Strong's Lexicon

Source: [STEPBible-Data](https://github.com/STEPBible/STEPBible-Data) (CC BY 4.0)

1. Download TBESG (Greek) and TBESH (Hebrew) from the Lexicons directory
2. Run `tools/build_strongs_db.py` to convert TSV → SQLite

```bash
python3 tools/build_strongs_db.py \
    --hebrew data/raw/TBESH.txt \
    --greek data/raw/TBESG.txt \
    --output data/strongs.sqlite
```

## KJV Plugin

Source: Bible SuperSearch (public domain text)

The KJV text with Strong's markers is available from biblesupersearch.com
in SQLite format. Run the conversion tool to produce a plugin database:

```bash
python3 tools/build_kjv_plugin.py \
    --source data/raw/kjv_strongs.sqlite \
    --output data/kjv.sqlite
```

## CUV Plugin (Chinese Union Version)

Source: Bible SuperSearch (public domain text, 1919)

Available in simplified and traditional variants:

```bash
python3 tools/build_cuv_plugin.py \
    --source-simp data/raw/chinese_union_simp_s.sqlite \
    --source-trad data/raw/chinese_union_trad_s.sqlite \
    --output-simp data/cuv_simp.sqlite \
    --output-trad data/cuv_trad.sqlite
```

## Adding Your Own Translation

See the plugin schema in [../README.md](../README.md). Any SQLite database
with `books` and `verses` tables conforming to the schema can be registered
as a plugin.

Example: to add the Russian Synodal Bible, create `synodal.sqlite` with:

```sql
CREATE TABLE books (
    id INTEGER PRIMARY KEY,
    name TEXT NOT NULL,        -- "Бытие", "Исход", etc.
    name_alt TEXT,
    abbr TEXT NOT NULL,        -- "Быт", "Исх", etc.
    testament TEXT,
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

Populate with your translation data, then register the plugin in your application.
