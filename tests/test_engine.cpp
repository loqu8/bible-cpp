/// @file test_engine.cpp
/// @brief Integration tests for ScriptureEngine (SQLite-backed).
///
/// Creates an in-memory SQLite database with test data, writes it to a temp
/// file, then exercises all engine methods.

#include "bible/bible.hpp"

#include <sqlite3.h>

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

using bible::ScriptureEngine;
using bible::PluginConfig;
using bible::Book;
using bible::ParallelVerse;
using bible::StrongsEntry;
using bible::Reference;

// ---------------------------------------------------------------------------
// Test framework (same micro-framework as test_strongs_parser.cpp)
// ---------------------------------------------------------------------------

#define TEST(name) \
    static void test_##name(); \
    struct Register_##name { Register_##name() { tests.push_back({#name, test_##name}); } } reg_##name; \
    static void test_##name()

struct TestEntry {
    const char* name;
    void (*fn)();
};

static std::vector<TestEntry> tests;

// ---------------------------------------------------------------------------
// Test database setup
// ---------------------------------------------------------------------------

static std::string g_db_path;

static void exec(sqlite3* db, const char* sql) {
    char* err = nullptr;
    int rc = sqlite3_exec(db, sql, nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        std::cerr << "SQL error: " << (err ? err : "unknown") << "\n"
                  << "  SQL: " << sql << "\n";
        sqlite3_free(err);
        std::abort();
    }
}

static void create_test_db() {
    // Create temp file
    auto tmp = std::filesystem::temp_directory_path() / "bible_test.sqlite";
    g_db_path = tmp.string();

    // Remove if exists
    std::filesystem::remove(tmp);

    sqlite3* db = nullptr;
    int rc = sqlite3_open(g_db_path.c_str(), &db);
    assert(rc == SQLITE_OK);

    // Create schema
    exec(db, R"(
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
            text_cuv_simp TEXT,
            text_cuv_trad TEXT,
            text_kjv TEXT,
            text_cuv_simp_plain TEXT,
            text_cuv_trad_plain TEXT,
            text_kjv_plain TEXT,
            versification_note TEXT,
            UNIQUE(book, chapter, verse)
        );

        CREATE TABLE strongs (
            number INTEGER NOT NULL,
            type TEXT NOT NULL,
            language TEXT NOT NULL DEFAULT 'Hebrew',
            lemma TEXT,
            translit TEXT,
            pronunciation TEXT,
            kjv_def TEXT,
            strongs_def TEXT,
            PRIMARY KEY(number, type)
        );

        CREATE INDEX idx_verses_book_ch ON verses(book, chapter);
    )");

    // Insert test books (3 books)
    exec(db, R"(
        INSERT INTO books VALUES (1, 'Genesis', '创世记', '創世記', 'Gen', 'OT', 'protestant', 50);
        INSERT INTO books VALUES (43, 'John', '约翰福音', '約翰福音', 'Jhn', 'NT', 'protestant', 21);
        INSERT INTO books VALUES (66, 'Revelation', '启示录', '啟示錄', 'Rev', 'NT', 'protestant', 22);
    )");

    // Insert test verses (Genesis 1:1-3, John 3:16-17, Revelation 1:1)
    exec(db, R"(
        INSERT INTO verses (book, chapter, verse, text_kjv, text_kjv_plain,
                            text_cuv_simp, text_cuv_simp_plain)
        VALUES
        (1, 1, 1,
         'In the {H7225}beginning {H430}God {H1254}created the heaven and the earth.',
         'In the beginning God created the heaven and the earth.',
         '{H7225}起初 {H430}神 {H1254}创造天地。',
         '起初神创造天地。'),
        (1, 1, 2,
         'And the earth was without form, and void.',
         'And the earth was without form, and void.',
         '地是空虚混沌。',
         '地是空虚混沌。'),
        (1, 1, 3,
         'And {H430}God said, Let there be light.',
         'And God said, Let there be light.',
         '{H430}神说，要有光。',
         '神说，要有光。'),
        (43, 3, 16,
         'For {G2316}God so {G25}loved the world, that he gave his only begotten Son.',
         'For God so loved the world, that he gave his only begotten Son.',
         '神爱世人，甚至将他的独生子赐给他们。',
         '神爱世人，甚至将他的独生子赐给他们。'),
        (43, 3, 17,
         'For {G2316}God sent not his Son into the world to condemn the world.',
         'For God sent not his Son into the world to condemn the world.',
         '因为神差他的儿子降世，不是要定世人的罪。',
         '因为神差他的儿子降世，不是要定世人的罪。'),
        (66, 1, 1,
         'The Revelation of Jesus Christ.',
         'The Revelation of Jesus Christ.',
         '耶稣基督的启示。',
         '耶稣基督的启示。');
    )");

    // Insert Strong's entries
    exec(db, R"(
        INSERT INTO strongs VALUES (430, 'H', 'Hebrew', 'אֱלֹהִים', 'elohim',
            'el-o-heem', 'God, gods', 'plural of H433; gods in the ordinary sense');
        INSERT INTO strongs VALUES (7225, 'H', 'Hebrew', 'רֵאשִׁית', 'reshith',
            'ray-sheeth', 'beginning, chief', 'from the same as H7218; the first');
        INSERT INTO strongs VALUES (2316, 'G', 'Greek', 'θεός', 'theos',
            'theh-os', 'God, god', 'of uncertain affinity; a deity');
    )");

    sqlite3_close(db);
}

static void cleanup_test_db() {
    std::filesystem::remove(g_db_path);
}

// ---------------------------------------------------------------------------
// Helper: create engine with test DB registered
// ---------------------------------------------------------------------------

static ScriptureEngine make_engine() {
    ScriptureEngine engine;
    engine.init_strongs(g_db_path);

    // Register KJV plugin (combined-DB pattern)
    engine.register_plugin("kjv", g_db_path, {
        .language = "en",
        .name = "King James Version",
        .has_strongs = true,
        .text_column = "text_kjv",
        .text_plain_column = "text_kjv_plain"
    });

    // Register CUV plugin (same DB, different columns)
    engine.register_plugin("cuv", g_db_path, {
        .language = "zh",
        .name = "Chinese Union Version",
        .name_native = "和合本",
        .has_strongs = true,
        .text_column = "text_cuv_simp",
        .text_plain_column = "text_cuv_simp_plain"
    });

    return engine;
}

// ---------------------------------------------------------------------------
// Tests: Plugin registry
// ---------------------------------------------------------------------------

TEST(register_plugins) {
    auto engine = make_engine();
    auto ids = engine.plugin_ids();
    assert(ids.size() == 2);
    // Map is sorted — cuv before kjv
    assert(ids[0] == "cuv");
    assert(ids[1] == "kjv");
}

TEST(unregister_plugin) {
    auto engine = make_engine();
    engine.unregister_plugin("cuv");
    auto ids = engine.plugin_ids();
    assert(ids.size() == 1);
    assert(ids[0] == "kjv");
}

TEST(duplicate_register_fails) {
    auto engine = make_engine();
    int rc = engine.register_plugin("kjv", g_db_path, {});
    assert(rc < 0);  // should fail
}

// ---------------------------------------------------------------------------
// Tests: Books
// ---------------------------------------------------------------------------

TEST(get_all_books) {
    auto engine = make_engine();
    auto books = engine.get_books();
    assert(books.size() == 3);
    assert(books[0].id == 1);
    assert(books[0].name == "Genesis");
    assert(books[0].name_alt == "创世记");
    assert(books[0].abbr == "Gen");
    assert(books[0].testament == "OT");
    assert(books[0].chapter_count == 50);
}

TEST(get_books_by_testament) {
    auto engine = make_engine();
    auto ot = engine.get_books({}, "OT");
    assert(ot.size() == 1);
    assert(ot[0].name == "Genesis");

    auto nt = engine.get_books({}, "NT");
    assert(nt.size() == 2);
}

TEST(get_single_book) {
    auto engine = make_engine();
    auto book = engine.get_book(43);
    assert(book.id == 43);
    assert(book.name == "John");
    assert(book.name_alt == "约翰福音");

    auto missing = engine.get_book(99);
    assert(missing.id == 0);
}

// ---------------------------------------------------------------------------
// Tests: Chapter/Verse navigation
// ---------------------------------------------------------------------------

TEST(get_chapter_parallel) {
    auto engine = make_engine();
    auto verses = engine.get_chapter(1, 1);
    assert(verses.size() == 3);  // Gen 1:1-3

    // Each verse should have 2 parallel texts (kjv + cuv)
    for (const auto& pv : verses) {
        assert(pv.texts.size() == 2);
        assert(pv.book == 1);
        assert(pv.chapter == 1);
    }

    // Verify verse 1 KJV text
    const auto& v1 = verses[0];
    assert(v1.verse == 1);
    bool found_kjv = false;
    for (const auto& [pid, v] : v1.texts) {
        if (pid == "kjv") {
            assert(v.text.find("{H7225}") != std::string::npos);
            assert(v.text_plain == "In the beginning God created the heaven and the earth.");
            found_kjv = true;
        }
    }
    assert(found_kjv);
}

TEST(get_chapter_single_plugin) {
    auto engine = make_engine();
    auto verses = engine.get_chapter(1, 1, {.plugin_id = "kjv"});
    assert(verses.size() == 3);

    // Each verse should have 1 text (kjv only)
    for (const auto& pv : verses) {
        assert(pv.texts.size() == 1);
        assert(pv.texts[0].first == "kjv");
    }
}

TEST(get_chapter_empty) {
    auto engine = make_engine();
    auto verses = engine.get_chapter(1, 99);  // non-existent chapter
    assert(verses.empty());
}

TEST(get_verse) {
    auto engine = make_engine();
    auto pv = engine.get_verse(43, 3, 16);
    assert(pv.book == 43);
    assert(pv.chapter == 3);
    assert(pv.verse == 16);
    assert(pv.texts.size() == 2);

    // Check KJV
    bool found = false;
    for (const auto& [pid, v] : pv.texts) {
        if (pid == "kjv") {
            assert(v.text_plain.find("God so") != std::string::npos);
            found = true;
        }
    }
    assert(found);
}

// ---------------------------------------------------------------------------
// Tests: Strong's
// ---------------------------------------------------------------------------

TEST(get_strongs_hebrew) {
    auto engine = make_engine();
    auto entry = engine.get_strongs('H', 430);
    assert(entry.number == 430);
    assert(entry.type == 'H');
    assert(entry.language == "Hebrew");
    assert(entry.lemma.find("אֱלֹהִים") != std::string::npos);
    assert(entry.translit == "elohim");
    assert(!entry.gloss.empty());
    assert(!entry.definition.empty());
    assert(entry.label() == "H430");
}

TEST(get_strongs_greek) {
    auto engine = make_engine();
    auto entry = engine.get_strongs('G', 2316);
    assert(entry.number == 2316);
    assert(entry.type == 'G');
    assert(entry.language == "Greek");
    assert(entry.translit == "theos");
}

TEST(get_strongs_missing) {
    auto engine = make_engine();
    auto entry = engine.get_strongs('H', 99999);
    assert(entry.number == 0);
}

TEST(get_verses_with_strongs) {
    auto engine = make_engine();
    // H430 (God) appears in Gen 1:1 and Gen 1:3
    auto verses = engine.get_verses_with_strongs('H', 430);
    assert(verses.size() == 2);
    assert(verses[0].book == 1);
    assert(verses[0].chapter == 1);
    assert(verses[0].verse == 1);
    assert(verses[1].verse == 3);
}

TEST(count_verses_with_strongs) {
    auto engine = make_engine();
    assert(engine.count_verses_with_strongs('H', 430) == 2);
    assert(engine.count_verses_with_strongs('G', 2316) == 2);
    assert(engine.count_verses_with_strongs('H', 99999) == 0);
}

// ---------------------------------------------------------------------------
// Tests: Reference parsing
// ---------------------------------------------------------------------------

TEST(parse_reference_english) {
    auto engine = make_engine();
    auto ref = engine.parse_reference("John 3:16");
    assert(ref.valid);
    assert(ref.book == 43);
    assert(ref.chapter == 3);
    assert(ref.verse == 16);
    assert(ref.verse_end == 0);
}

TEST(parse_reference_range) {
    auto engine = make_engine();
    auto ref = engine.parse_reference("Gen 1:1-3");
    assert(ref.valid);
    assert(ref.book == 1);
    assert(ref.chapter == 1);
    assert(ref.verse == 1);
    assert(ref.verse_end == 3);
}

TEST(parse_reference_abbreviation) {
    auto engine = make_engine();
    auto ref = engine.parse_reference("Jhn 3:16");
    assert(ref.valid);
    assert(ref.book == 43);
}

TEST(parse_reference_chinese) {
    auto engine = make_engine();
    auto ref = engine.parse_reference("约翰福音 3:16");
    assert(ref.valid);
    assert(ref.book == 43);
    assert(ref.chapter == 3);
    assert(ref.verse == 16);
}

TEST(parse_reference_chapter_only) {
    auto engine = make_engine();
    auto ref = engine.parse_reference("Genesis 1");
    assert(ref.valid);
    assert(ref.book == 1);
    assert(ref.chapter == 1);
    assert(ref.verse == 0);
}

TEST(parse_reference_invalid) {
    auto engine = make_engine();
    auto ref = engine.parse_reference("Nonexistent 1:1");
    assert(!ref.valid);

    auto empty = engine.parse_reference("");
    assert(!empty.valid);
}

TEST(format_reference_english) {
    auto engine = make_engine();
    Reference ref{.book = 43, .chapter = 3, .verse = 16, .valid = true};
    auto str = engine.format_reference(ref, "en");
    assert(str == "John 3:16");
}

TEST(format_reference_chinese) {
    auto engine = make_engine();
    Reference ref{.book = 43, .chapter = 3, .verse = 16, .valid = true};
    auto str = engine.format_reference(ref, "zh");
    assert(str == "约翰福音 3:16");
}

TEST(format_reference_range) {
    auto engine = make_engine();
    Reference ref{.book = 1, .chapter = 1, .verse = 1, .verse_end = 3, .valid = true};
    auto str = engine.format_reference(ref);
    assert(str == "Genesis 1:1-3");
}

// ---------------------------------------------------------------------------
// Tests: Search
// ---------------------------------------------------------------------------

TEST(search_english) {
    auto engine = make_engine();
    auto [results, stats] = engine.search("beginning");
    assert(!results.empty());
    assert(results[0].verse.plugin_id == "cuv" || results[0].verse.plugin_id == "kjv");
    assert(stats.total_matches > 0);
    assert(stats.translations_searched == 2);
}

TEST(search_chinese) {
    auto engine = make_engine();
    auto [results, stats] = engine.search("创造");
    assert(!results.empty());
    // Should find CUV match (创造天地)
    bool found_cuv = false;
    for (const auto& sr : results) {
        if (sr.verse.plugin_id == "cuv") found_cuv = true;
    }
    assert(found_cuv);
}

TEST(search_no_results) {
    auto engine = make_engine();
    auto [results, stats] = engine.search("zzzznonexistent");
    assert(results.empty());
    assert(stats.total_matches == 0);
}

TEST(search_single_plugin) {
    auto engine = make_engine();
    auto [results, stats] = engine.search("God", {.plugin_id = "kjv"});
    assert(stats.translations_searched == 1);
    for (const auto& sr : results) {
        assert(sr.verse.plugin_id == "kjv");
    }
}

// ---------------------------------------------------------------------------
// Tests: Statistics
// ---------------------------------------------------------------------------

TEST(compute_stats) {
    auto engine = make_engine();
    auto stats = engine.compute_stats();
    assert(stats.plugins_registered == 2);
    assert(stats.total_books == 3);
    assert(stats.total_chapters == 50 + 21 + 22);
    assert(stats.total_verses == 12);  // 6 verses × 2 plugins
    assert(stats.strongs_hebrew_entries == 2);
    assert(stats.strongs_greek_entries == 1);
    assert(stats.verses_per_plugin.size() == 2);
}

TEST(strongs_stats) {
    auto engine = make_engine();
    auto ss = engine.strongs_stats('H', 430);
    assert(ss.number == 430);
    assert(ss.type == 'H');
    assert(ss.verse_count > 0);
    assert(ss.plugin_count > 0);
}

// ---------------------------------------------------------------------------
// Runner
// ---------------------------------------------------------------------------

int main() {
    create_test_db();

    int passed = 0;
    int failed = 0;

    for (const auto& test : tests) {
        try {
            test.fn();
            std::cout << "  PASS: " << test.name << "\n";
            ++passed;
        } catch (const std::exception& e) {
            std::cerr << "  FAIL: " << test.name << " — " << e.what() << "\n";
            ++failed;
        } catch (...) {
            std::cerr << "  FAIL: " << test.name << " — assertion failed\n";
            ++failed;
        }
    }

    cleanup_test_db();

    std::cout << "\n" << passed << " passed, " << failed << " failed\n";
    return failed > 0 ? 1 : 0;
}
