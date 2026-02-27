#pragma once

/// @file types.hpp
/// @brief Core data types for bible-cpp.

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace bible {

// ---------------------------------------------------------------------------
// Book
// ---------------------------------------------------------------------------

struct Book {
    int id = 0;               ///< 1-66 for Protestant Bible, extensible
    std::string name;         ///< Primary name in plugin's language
    std::string name_alt;     ///< Alternate name (e.g., traditional Chinese)
    std::string abbr;         ///< Abbreviation for reference parsing
    std::string testament;    ///< "OT", "NT", or empty for non-Biblical texts
    int chapter_count = 0;
};

// ---------------------------------------------------------------------------
// Verse
// ---------------------------------------------------------------------------

struct Verse {
    int book = 0;
    int chapter = 0;
    int verse = 0;
    std::string text;         ///< Raw text (may contain Strong's markers)
    std::string text_plain;   ///< Stripped text (no markers)
    std::string plugin_id;    ///< Which plugin this came from
};

/// A verse with parallel texts from multiple plugins.
struct ParallelVerse {
    int book = 0;
    int chapter = 0;
    int verse = 0;
    /// Parallel texts keyed by plugin_id.
    /// Use find() to get a specific translation.
    std::vector<std::pair<std::string, Verse>> texts;
};

// ---------------------------------------------------------------------------
// Strong's Concordance
// ---------------------------------------------------------------------------

struct StrongsEntry {
    int number = 0;
    char type = 'H';          ///< 'H' = Hebrew, 'G' = Greek
    std::string language;     ///< "Hebrew", "Aramaic", or "Greek"
    std::string lemma;        ///< Original word (אֱלֹהִים / λόγος)
    std::string translit;     ///< Transliteration
    std::string pronunciation;
    std::string morph;        ///< Morphological classification
    std::string gloss;        ///< One-word gloss
    std::string definition;   ///< Full lexical definition

    /// Convenience: "H430" or "G3056"
    [[nodiscard]] std::string label() const;
};

// ---------------------------------------------------------------------------
// Parsed word (from Strong's-tagged text)
// ---------------------------------------------------------------------------

struct ParsedWord {
    std::string text;              ///< The word as displayed
    bool has_strongs = false;
    char strongs_type = 0;         ///< 'H' or 'G'
    int strongs_number = 0;

    /// Convenience: "H430" or empty
    [[nodiscard]] std::string strongs_label() const;
};

// ---------------------------------------------------------------------------
// Reference (parsed scripture reference)
// ---------------------------------------------------------------------------

struct Reference {
    int book = 0;
    int chapter = 0;
    int verse = 0;                 ///< 0 = whole chapter
    int verse_end = 0;             ///< For ranges (e.g., John 3:16-18)
    bool valid = false;
};

// ---------------------------------------------------------------------------
// Search result
// ---------------------------------------------------------------------------

struct SearchResult {
    Verse verse;
    float relevance = 0.0f;       ///< FTS5 rank score
};

// ---------------------------------------------------------------------------
// Statistics (AI/MCP-ready metadata)
// ---------------------------------------------------------------------------

struct QueryStats {
    double query_time_ms = 0.0;
    int total_matches = 0;
    int books_matched = 0;
    int translations_searched = 0;
};

struct CorpusStats {
    int plugins_registered = 0;
    int total_books = 0;
    int total_verses = 0;
    int total_chapters = 0;
    int strongs_hebrew_entries = 0;
    int strongs_greek_entries = 0;
    int64_t search_index_size_bytes = 0;
    std::vector<std::pair<std::string, int>> verses_per_plugin;
};

struct StrongsStats {
    int number = 0;
    char type = 0;
    int verse_count = 0;           ///< How many verses contain this Strong's number
    int plugin_count = 0;          ///< How many plugins have tagged text with this number
};

}  // namespace bible
