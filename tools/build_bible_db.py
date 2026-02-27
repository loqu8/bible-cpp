#!/usr/bin/env python3
"""build_bible_db.py — Build bible-cpp plugin databases from public sources.

Downloads KJV, CUV (Simplified/Traditional) with Strong's markers from
Bible SuperSearch, and Hebrew/Greek lexicons from STEPBible-Data.

Outputs 4 SQLite databases conforming to the bible-cpp engine schema:
  data/kjv.sqlite       — KJV with Strong's markers
  data/cuv_simp.sqlite  — CUV Simplified with Strong's markers
  data/cuv_trad.sqlite  — CUV Traditional with Strong's markers
  data/strongs.sqlite   — Combined Hebrew + Greek lexicon

Usage:
    python3 tools/build_bible_db.py                           # auto-download + build
    python3 tools/build_bible_db.py --output-dir /path/to/out # custom output
    python3 tools/build_bible_db.py --data-dir /path/to/raw   # pre-downloaded files

All sources are public domain (Bible text) or CC BY 4.0 (STEPBible lexicons).
"""

import argparse
import csv
import html
import io
import os
import re
import sqlite3
import sys
import urllib.request
import zipfile
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
PROJECT_DIR = SCRIPT_DIR.parent

# ---------------------------------------------------------------------------
# Download URLs
# ---------------------------------------------------------------------------

URLS = {
    # Bible SuperSearch modules (GitHub, pipe-separated text with Strong's)
    "kjv_strongs.zip": (
        "https://github.com/aicwebtech/biblesupersearch_api/raw/master/"
        "bibles/modules/kjv_strongs.zip"
    ),
    "chinese_union_simp_s.zip": (
        "https://github.com/aicwebtech/biblesupersearch_api/raw/master/"
        "bibles/modules/chinese_union_simp_s.zip"
    ),
    "chinese_union_trad_s.zip": (
        "https://github.com/aicwebtech/biblesupersearch_api/raw/master/"
        "bibles/modules/chinese_union_trad_s.zip"
    ),
    # STEPBible lexicons (CC BY 4.0)
    "TBESH.txt": (
        "https://raw.githubusercontent.com/STEPBible/STEPBible-Data/master/"
        "Lexicons/TBESH%20-%20Translators%20Brief%20lexicon%20of%20Extended"
        "%20Strongs%20for%20Hebrew%20-%20STEPBible.org%20CC%20BY.txt"
    ),
    "TBESG.txt": (
        "https://raw.githubusercontent.com/STEPBible/STEPBible-Data/master/"
        "Lexicons/TBESG%20-%20Translators%20Brief%20lexicon%20of%20Extended"
        "%20Strongs%20for%20Greek%20-%20STEPBible.org%20CC%20BY.txt"
    ),
    # Bible SuperSearch Strong's definitions (pronunciation data)
    "strongs_definitions.csv": (
        "https://raw.githubusercontent.com/aicwebtech/biblesupersearch_api/"
        "master/database/dumps/strongs_definitions.csv"
    ),
}

# ---------------------------------------------------------------------------
# 66-book metadata (Protestant canon)
# (id, name_en, name_zh, name_zh_trad, abbr_en, testament, chapter_count)
# ---------------------------------------------------------------------------

BOOKS_META = [
    # Old Testament (39 books)
    (1,  "Genesis",         "创世记",       "創世記",       "Gen", "OT", 50),
    (2,  "Exodus",          "出埃及记",     "出埃及記",     "Exo", "OT", 40),
    (3,  "Leviticus",       "利未记",       "利未記",       "Lev", "OT", 27),
    (4,  "Numbers",         "民数记",       "民數記",       "Num", "OT", 36),
    (5,  "Deuteronomy",     "申命记",       "申命記",       "Deu", "OT", 34),
    (6,  "Joshua",          "约书亚记",     "約書亞記",     "Jos", "OT", 24),
    (7,  "Judges",          "士师记",       "士師記",       "Jdg", "OT", 21),
    (8,  "Ruth",            "路得记",       "路得記",       "Rth", "OT", 4),
    (9,  "1 Samuel",        "撒母耳记上",   "撒母耳記上",   "1Sa", "OT", 31),
    (10, "2 Samuel",        "撒母耳记下",   "撒母耳記下",   "2Sa", "OT", 24),
    (11, "1 Kings",         "列王纪上",     "列王紀上",     "1Ki", "OT", 22),
    (12, "2 Kings",         "列王纪下",     "列王紀下",     "2Ki", "OT", 25),
    (13, "1 Chronicles",    "历代志上",     "歷代志上",     "1Ch", "OT", 29),
    (14, "2 Chronicles",    "历代志下",     "歷代志下",     "2Ch", "OT", 36),
    (15, "Ezra",            "以斯拉记",     "以斯拉記",     "Ezr", "OT", 10),
    (16, "Nehemiah",        "尼希米记",     "尼希米記",     "Neh", "OT", 13),
    (17, "Esther",          "以斯帖记",     "以斯帖記",     "Est", "OT", 10),
    (18, "Job",             "约伯记",       "約伯記",       "Job", "OT", 42),
    (19, "Psalms",          "诗篇",         "詩篇",         "Psa", "OT", 150),
    (20, "Proverbs",        "箴言",         "箴言",         "Pro", "OT", 31),
    (21, "Ecclesiastes",    "传道书",       "傳道書",       "Ecc", "OT", 12),
    (22, "Song of Solomon", "雅歌",         "雅歌",         "Sng", "OT", 8),
    (23, "Isaiah",          "以赛亚书",     "以賽亞書",     "Isa", "OT", 66),
    (24, "Jeremiah",        "耶利米书",     "耶利米書",     "Jer", "OT", 52),
    (25, "Lamentations",    "耶利米哀歌",   "耶利米哀歌",   "Lam", "OT", 5),
    (26, "Ezekiel",         "以西结书",     "以西結書",     "Eze", "OT", 48),
    (27, "Daniel",          "但以理书",     "但以理書",     "Dan", "OT", 12),
    (28, "Hosea",           "何西阿书",     "何西阿書",     "Hos", "OT", 14),
    (29, "Joel",            "约珥书",       "約珥書",       "Joe", "OT", 3),
    (30, "Amos",            "阿摩司书",     "阿摩司書",     "Amo", "OT", 9),
    (31, "Obadiah",         "俄巴底亚书",   "俄巴底亞書",   "Oba", "OT", 1),
    (32, "Jonah",           "约拿书",       "約拿書",       "Jon", "OT", 4),
    (33, "Micah",           "弥迦书",       "彌迦書",       "Mic", "OT", 7),
    (34, "Nahum",           "那鸿书",       "那鴻書",       "Nah", "OT", 3),
    (35, "Habakkuk",        "哈巴谷书",     "哈巴谷書",     "Hab", "OT", 3),
    (36, "Zephaniah",       "西番雅书",     "西番雅書",     "Zep", "OT", 3),
    (37, "Haggai",          "哈该书",       "哈該書",       "Hag", "OT", 2),
    (38, "Zechariah",       "撒迦利亚书",   "撒迦利亞書",   "Zec", "OT", 14),
    (39, "Malachi",         "玛拉基书",     "瑪拉基書",     "Mal", "OT", 4),
    # New Testament (27 books)
    (40, "Matthew",         "马太福音",     "馬太福音",     "Mat", "NT", 28),
    (41, "Mark",            "马可福音",     "馬可福音",     "Mrk", "NT", 16),
    (42, "Luke",            "路加福音",     "路加福音",     "Luk", "NT", 24),
    (43, "John",            "约翰福音",     "約翰福音",     "Jhn", "NT", 21),
    (44, "Acts",            "使徒行传",     "使徒行傳",     "Act", "NT", 28),
    (45, "Romans",          "罗马书",       "羅馬書",       "Rom", "NT", 16),
    (46, "1 Corinthians",   "哥林多前书",   "哥林多前書",   "1Co", "NT", 16),
    (47, "2 Corinthians",   "哥林多后书",   "哥林多後書",   "2Co", "NT", 13),
    (48, "Galatians",       "加拉太书",     "加拉太書",     "Gal", "NT", 6),
    (49, "Ephesians",       "以弗所书",     "以弗所書",     "Eph", "NT", 6),
    (50, "Philippians",     "腓立比书",     "腓立比書",     "Php", "NT", 4),
    (51, "Colossians",      "歌罗西书",     "歌羅西書",     "Col", "NT", 4),
    (52, "1 Thessalonians", "帖撒罗尼迦前书","帖撒羅尼迦前書","1Th", "NT", 5),
    (53, "2 Thessalonians", "帖撒罗尼迦后书","帖撒羅尼迦後書","2Th", "NT", 3),
    (54, "1 Timothy",       "提摩太前书",   "提摩太前書",   "1Ti", "NT", 6),
    (55, "2 Timothy",       "提摩太后书",   "提摩太後書",   "2Ti", "NT", 4),
    (56, "Titus",           "提多书",       "提多書",       "Tit", "NT", 3),
    (57, "Philemon",        "腓利门书",     "腓利門書",     "Phm", "NT", 1),
    (58, "Hebrews",         "希伯来书",     "希伯來書",     "Heb", "NT", 13),
    (59, "James",           "雅各书",       "雅各書",       "Jas", "NT", 5),
    (60, "1 Peter",         "彼得前书",     "彼得前書",     "1Pe", "NT", 5),
    (61, "2 Peter",         "彼得后书",     "彼得後書",     "2Pe", "NT", 3),
    (62, "1 John",          "约翰一书",     "約翰一書",     "1Jn", "NT", 5),
    (63, "2 John",          "约翰二书",     "約翰二書",     "2Jn", "NT", 1),
    (64, "3 John",          "约翰三书",     "約翰三書",     "3Jn", "NT", 1),
    (65, "Jude",            "犹大书",       "猶大書",       "Jud", "NT", 1),
    (66, "Revelation",      "启示录",       "啟示錄",       "Rev", "NT", 22),
]

# ---------------------------------------------------------------------------
# SQL schemas
# ---------------------------------------------------------------------------

SCHEMA_PLUGIN_SQL = """\
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

CREATE INDEX idx_verses_book_ch ON verses(book, chapter);
"""

SCHEMA_STRONGS_SQL = """\
CREATE TABLE strongs (
    number INTEGER NOT NULL,
    type TEXT NOT NULL CHECK(type IN ('H', 'G')),
    language TEXT NOT NULL DEFAULT 'Hebrew'
        CHECK(language IN ('Hebrew', 'Aramaic', 'Greek')),
    lemma TEXT,
    translit TEXT,
    pronunciation TEXT,
    kjv_def TEXT,
    strongs_def TEXT,
    PRIMARY KEY(number, type)
);

CREATE INDEX idx_strongs_type ON strongs(type);
"""

# ---------------------------------------------------------------------------
# Download helpers
# ---------------------------------------------------------------------------

def download_if_missing(data_dir, filename):
    """Download a file if not already cached in data_dir."""
    path = data_dir / filename
    if path.exists():
        print(f"  cached: {filename}")
        return path

    url = URLS.get(filename)
    if not url:
        print(f"  ERROR: no URL for {filename}", file=sys.stderr)
        sys.exit(1)

    print(f"  downloading: {filename} ...")
    try:
        req = urllib.request.Request(url, headers={"User-Agent": "bible-cpp/1.0"})
        with urllib.request.urlopen(req, timeout=120) as resp:
            data = resp.read()
        path.write_bytes(data)
        size_kb = len(data) / 1024
        print(f"  saved: {filename} ({size_kb:.0f} KB)")
    except Exception as e:
        print(f"  ERROR downloading {filename}: {e}", file=sys.stderr)
        print(f"  Download manually from:\n    {url}", file=sys.stderr)
        print(f"  Place in: {data_dir}/", file=sys.stderr)
        sys.exit(1)

    return path


def extract_verses_txt(zip_path):
    """Extract verses.txt from a Bible SuperSearch module ZIP."""
    with zipfile.ZipFile(zip_path, "r") as zf:
        for name in zf.namelist():
            if name.endswith("verses.txt"):
                return zf.read(name).decode("utf-8")
    raise ValueError(f"No verses.txt found in {zip_path}")

# ---------------------------------------------------------------------------
# Text processing
# ---------------------------------------------------------------------------

def strip_morphology(text):
    """Remove morphology codes {(H####)} from Strong's-tagged text."""
    return re.sub(r'\{\([HG]\d+\)\}', '', text)


def strip_strongs(text):
    """Remove all Strong's markers {H####}/{G####} to produce plain text."""
    text = re.sub(r'\{[HG]\d+\}', '', text)
    text = re.sub(r'\{\([HG]\d+\)\}', '', text)
    # Collapse multiple spaces
    text = re.sub(r'  +', ' ', text)
    return text.strip()


def clean_cuv(text):
    """Clean CUV text: strip paragraph markers and extra whitespace."""
    # Remove pilcrow (paragraph marker)
    text = text.replace('¶', '')
    # Remove ideographic space (U+3000) used as honorific spacing
    text = text.replace('\u3000', '')
    # Strip leading/trailing whitespace
    return text.strip()


def strip_html(text):
    """Strip HTML tags and decode entities."""
    text = re.sub(r'<[^>]+>', '', text)
    text = html.unescape(text)
    # Normalize whitespace
    text = re.sub(r'\s+', ' ', text)
    return text.strip()

# ---------------------------------------------------------------------------
# Bible SuperSearch module parser
# ---------------------------------------------------------------------------

def parse_bss_module(verses_txt):
    """Parse Bible SuperSearch pipe-separated verses.txt.

    Format: book|chapter|verse|text|italics|strongs
    Returns list of (book, chapter, verse, text, text_plain).
    """
    verses = []
    for line in verses_txt.splitlines():
        line = line.strip()
        if not line or line.startswith('#'):
            continue
        parts = line.split('|')
        if len(parts) < 4:
            continue
        try:
            book = int(parts[0])
            chapter = int(parts[1])
            verse = int(parts[2])
        except ValueError:
            continue

        raw_text = parts[3]
        # Strip morphology codes, keep lexical Strong's
        text = strip_morphology(raw_text)
        text_plain = strip_strongs(raw_text)
        verses.append((book, chapter, verse, text, text_plain))

    return verses

# ---------------------------------------------------------------------------
# STEPBible lexicon parser
# ---------------------------------------------------------------------------

def parse_stepbible_lexicon(path, type_char):
    """Parse STEPBible TBESH/TBESG lexicon file.

    Returns dict of {number: (lemma, translit, morph, gloss, definition, language)}.

    Entry selection: col1 contains '=' marker. Some entries have bare '='
    (primary meaning), others have '= a Name of' or '= in Aramaic of'.
    We prefer bare '=' entries but fall back to qualified ones.
    First entry per base number wins within each priority level.
    """
    # Two-pass: collect bare '=' entries first, then fill gaps with qualified
    entries = {}
    qualified = {}

    with open(path, encoding='utf-8-sig') as f:
        for line in f:
            line = line.rstrip('\n\r')
            if not line or line.startswith('#') or line.startswith('$'):
                continue
            parts = line.split('\t')
            if len(parts) < 7:
                continue

            # Column 0: base Strong's number (e.g., H0001)
            col0 = parts[0].strip()
            if not col0.startswith(type_char):
                continue
            m = re.match(r'[HG](\d+)', col0)
            if not m:
                continue
            number = int(m.group(1))

            # Column 1: extended number + qualifier
            col1 = parts[1].rstrip()
            if '=' not in col1:
                continue

            lemma = parts[3].strip() if len(parts) > 3 else ''
            translit = parts[4].strip() if len(parts) > 4 else ''
            morph = parts[5].strip() if len(parts) > 5 else ''
            gloss = parts[6].strip() if len(parts) > 6 else ''
            definition = strip_html(parts[7]) if len(parts) > 7 else ''

            # Detect language from morphology prefix
            if type_char == 'H':
                language = 'Aramaic' if morph.startswith('A:') else 'Hebrew'
            else:
                language = 'Greek'

            entry = (lemma, translit, morph, gloss, definition, language)

            # Bare '=' (primary) vs qualified '= a Name of' etc.
            is_bare = col1.endswith('=')
            if is_bare:
                if number not in entries:
                    entries[number] = entry
            else:
                if number not in qualified:
                    qualified[number] = entry

    # Fill gaps: use qualified entries for numbers missing bare primaries
    for number, entry in qualified.items():
        if number not in entries:
            entries[number] = entry

    return entries


def parse_bss_strongs_csv(path):
    """Parse Bible SuperSearch strongs_definitions.csv for pronunciation.

    Returns dict of {(type, number): pronunciation}.
    """
    pronunciations = {}
    with open(path, encoding='utf-8') as f:
        reader = csv.DictReader(f)
        for row in reader:
            num_str = row.get('number', '').strip()
            if not num_str:
                continue
            m = re.match(r'([HG])(\d+)', num_str)
            if not m:
                continue
            type_char = m.group(1)
            number = int(m.group(2))
            pronunciation = row.get('pronunciation', '').strip()
            if pronunciation:
                pronunciations[(type_char, number)] = pronunciation
    return pronunciations

# ---------------------------------------------------------------------------
# Database builders
# ---------------------------------------------------------------------------

def insert_books(conn):
    """Insert all 66 books into the books table."""
    conn.executemany(
        "INSERT INTO books (id, name_en, name_zh, name_zh_trad, abbr_en, "
        "testament, canon, chapter_count) VALUES (?, ?, ?, ?, ?, ?, 'protestant', ?)",
        BOOKS_META
    )


def build_plugin_db(output_path, verses, is_cuv=False):
    """Build a plugin database (books + verses)."""
    if output_path.exists():
        output_path.unlink()

    conn = sqlite3.connect(str(output_path))
    conn.executescript(SCHEMA_PLUGIN_SQL)
    insert_books(conn)

    for book, chapter, verse, text, text_plain in verses:
        if is_cuv:
            text = clean_cuv(text)
            text_plain = clean_cuv(text_plain)
        conn.execute(
            "INSERT OR IGNORE INTO verses (book, chapter, verse, text, text_plain) "
            "VALUES (?, ?, ?, ?, ?)",
            (book, chapter, verse, text, text_plain)
        )

    conn.commit()

    # Report
    cur = conn.execute("SELECT COUNT(*) FROM verses")
    n_verses = cur.fetchone()[0]
    cur = conn.execute("SELECT COUNT(DISTINCT book) FROM verses")
    n_books = cur.fetchone()[0]
    conn.close()

    print(f"  {output_path.name}: {n_books} books, {n_verses} verses")
    return n_verses


def build_strongs_db(output_path, hebrew_entries, greek_entries, pronunciations):
    """Build the Strong's lexicon database."""
    if output_path.exists():
        output_path.unlink()

    conn = sqlite3.connect(str(output_path))
    conn.executescript(SCHEMA_STRONGS_SQL)

    count = 0
    for type_char, entries in [('H', hebrew_entries), ('G', greek_entries)]:
        for number, (lemma, translit, morph, gloss, definition, language) in entries.items():
            pronunciation = pronunciations.get((type_char, number), '')
            conn.execute(
                "INSERT OR IGNORE INTO strongs "
                "(number, type, language, lemma, translit, pronunciation, "
                "kjv_def, strongs_def) VALUES (?, ?, ?, ?, ?, ?, ?, ?)",
                (number, type_char, language, lemma, translit,
                 pronunciation, gloss, definition)
            )
            count += 1

    conn.commit()

    # Report
    cur = conn.execute("SELECT COUNT(*) FROM strongs WHERE type='H'")
    n_h = cur.fetchone()[0]
    cur = conn.execute("SELECT COUNT(*) FROM strongs WHERE type='G'")
    n_g = cur.fetchone()[0]
    conn.close()

    print(f"  strongs.sqlite: {n_h} Hebrew + {n_g} Greek = {n_h + n_g} entries")
    return count

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description="Build bible-cpp plugin databases from public sources."
    )
    parser.add_argument(
        "--output-dir", type=Path, default=PROJECT_DIR / "data",
        help="Output directory for .sqlite files (default: data/)"
    )
    parser.add_argument(
        "--data-dir", type=Path, default=PROJECT_DIR / "data" / "raw",
        help="Directory for cached raw downloads (default: data/raw/)"
    )
    args = parser.parse_args()

    output_dir = args.output_dir
    data_dir = args.data_dir

    output_dir.mkdir(parents=True, exist_ok=True)
    data_dir.mkdir(parents=True, exist_ok=True)

    # -------------------------------------------------------------------
    # Step 1: Download sources
    # -------------------------------------------------------------------
    print("Downloading sources...")
    kjv_zip = download_if_missing(data_dir, "kjv_strongs.zip")
    cuv_simp_zip = download_if_missing(data_dir, "chinese_union_simp_s.zip")
    cuv_trad_zip = download_if_missing(data_dir, "chinese_union_trad_s.zip")
    tbesh_txt = download_if_missing(data_dir, "TBESH.txt")
    tbesg_txt = download_if_missing(data_dir, "TBESG.txt")
    strongs_csv = download_if_missing(data_dir, "strongs_definitions.csv")

    # -------------------------------------------------------------------
    # Step 2: Parse Bible modules
    # -------------------------------------------------------------------
    print("\nParsing Bible modules...")

    print("  KJV with Strong's...")
    kjv_raw = extract_verses_txt(kjv_zip)
    kjv_verses = parse_bss_module(kjv_raw)
    print(f"    {len(kjv_verses)} verses parsed")

    print("  CUV Simplified with Strong's...")
    cuv_simp_raw = extract_verses_txt(cuv_simp_zip)
    cuv_simp_verses = parse_bss_module(cuv_simp_raw)
    print(f"    {len(cuv_simp_verses)} verses parsed")

    print("  CUV Traditional with Strong's...")
    cuv_trad_raw = extract_verses_txt(cuv_trad_zip)
    cuv_trad_verses = parse_bss_module(cuv_trad_raw)
    print(f"    {len(cuv_trad_verses)} verses parsed")

    # -------------------------------------------------------------------
    # Step 3: Parse Strong's lexicons
    # -------------------------------------------------------------------
    print("\nParsing Strong's lexicons...")

    print("  Hebrew (TBESH)...")
    hebrew_entries = parse_stepbible_lexicon(tbesh_txt, 'H')
    print(f"    {len(hebrew_entries)} entries")

    print("  Greek (TBESG)...")
    greek_entries = parse_stepbible_lexicon(tbesg_txt, 'G')
    print(f"    {len(greek_entries)} entries")

    print("  Pronunciation data...")
    pronunciations = parse_bss_strongs_csv(strongs_csv)
    print(f"    {len(pronunciations)} pronunciations")

    # -------------------------------------------------------------------
    # Step 4: Build output databases
    # -------------------------------------------------------------------
    print("\nBuilding databases...")

    build_plugin_db(output_dir / "kjv.sqlite", kjv_verses)
    build_plugin_db(output_dir / "cuv_simp.sqlite", cuv_simp_verses, is_cuv=True)
    build_plugin_db(output_dir / "cuv_trad.sqlite", cuv_trad_verses, is_cuv=True)
    build_strongs_db(
        output_dir / "strongs.sqlite",
        hebrew_entries, greek_entries, pronunciations
    )

    # -------------------------------------------------------------------
    # Step 5: Summary
    # -------------------------------------------------------------------
    print("\nDone! Databases written to:", output_dir)
    print("\nUsage with bible-cpp:")
    print('  engine.init_strongs("data/strongs.sqlite");')
    print('  engine.register_plugin("kjv", "data/kjv.sqlite", {')
    print('      .language = "en", .name = "King James Version", .has_strongs = true')
    print('  });')
    print('  engine.register_plugin("cuv_simp", "data/cuv_simp.sqlite", {')
    print('      .language = "zh", .name = "Chinese Union Version (Simplified)",')
    print('      .name_native = "和合本简体", .has_strongs = true')
    print('  });')


if __name__ == "__main__":
    main()
