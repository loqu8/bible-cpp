/// @file test_strongs_parser.cpp
/// @brief Tests for Strong's text parser.
///
/// Minimal test framework (no external dependency). Returns 0 on success.

#include "bible/bible.hpp"
#include <cassert>
#include <iostream>
#include <string>

using bible::ScriptureEngine;
using bible::ParsedWord;

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
// Tests
// ---------------------------------------------------------------------------

TEST(empty_input) {
    auto words = ScriptureEngine::parse_strongs_text("");
    assert(words.empty());
}

TEST(plain_text_no_markers) {
    auto words = ScriptureEngine::parse_strongs_text("In the beginning God created");
    assert(words.size() == 5);
    assert(words[0].text == "In");
    assert(!words[0].has_strongs);
    assert(words[3].text == "God");
    assert(!words[3].has_strongs);
}

TEST(leading_marker_format) {
    // Bible SuperSearch format: marker before word
    auto words = ScriptureEngine::parse_strongs_text("{H7225}beginning {H430}God");
    assert(words.size() == 2);
    assert(words[0].text == "beginning");
    assert(words[0].has_strongs);
    assert(words[0].strongs_type == 'H');
    assert(words[0].strongs_number == 7225);
    assert(words[1].text == "God");
    assert(words[1].strongs_number == 430);
}

TEST(greek_markers) {
    auto words = ScriptureEngine::parse_strongs_text("{G3056}word {G2316}God");
    assert(words.size() == 2);
    assert(words[0].strongs_type == 'G');
    assert(words[0].strongs_number == 3056);
    assert(words[0].text == "word");
    assert(words[1].strongs_number == 2316);
}

TEST(mixed_tagged_and_plain) {
    auto words = ScriptureEngine::parse_strongs_text(
        "In the {H7225}beginning {H430}God {H1254}created the heavens");
    // "In" "the" are plain, "beginning" "God" "created" tagged, "the" "heavens" plain
    bool found_beginning = false;
    bool found_the = false;
    for (const auto& w : words) {
        if (w.text == "beginning") {
            assert(w.has_strongs);
            assert(w.strongs_number == 7225);
            found_beginning = true;
        }
        if (w.text == "the" && !w.has_strongs) {
            found_the = true;
        }
    }
    assert(found_beginning);
    assert(found_the);
}

TEST(strongs_label) {
    ParsedWord pw;
    pw.has_strongs = true;
    pw.strongs_type = 'H';
    pw.strongs_number = 430;
    assert(pw.strongs_label() == "H430");

    ParsedWord plain;
    plain.has_strongs = false;
    assert(plain.strongs_label().empty());
}

TEST(strongs_entry_label) {
    bible::StrongsEntry entry;
    entry.type = 'G';
    entry.number = 3056;
    assert(entry.label() == "G3056");
}

// ---------------------------------------------------------------------------
// Runner
// ---------------------------------------------------------------------------

int main() {
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

    std::cout << "\n" << passed << " passed, " << failed << " failed\n";
    return failed > 0 ? 1 : 0;
}
