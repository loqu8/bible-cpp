#pragma once

/// @file bible.hpp
/// @brief Main entry point for the bible-cpp scripture engine.
///
/// ScriptureEngine is the central class. Register one or more translation
/// plugins, then navigate, search, and cross-reference across all of them.
///
/// @code
///   bible::ScriptureEngine engine;
///   engine.init("/path/to/strongs.sqlite");  // Strong's lexicon (optional)
///   engine.register_plugin("kjv", "/path/to/kjv.sqlite", {
///       .language = "en", .name = "King James Version", .has_strongs = true
///   });
///   engine.register_plugin("cuv_simp", "/path/to/cuv_simp.sqlite", {
///       .language = "zh", .name = "Chinese Union Version (Simplified)",
///       .name_native = "和合本简体", .has_strongs = true
///   });
///
///   auto books = engine.get_books();          // all books across plugins
///   auto verses = engine.get_chapter(43, 3);  // John 3, all translations
///   auto ref = engine.parse_reference("John 3:16");
///   auto results = engine.search("love");
///   auto strongs = engine.get_strongs('G', 26);  // agape
///   auto stats = engine.compute_stats();
/// @endcode

#include "scripture_plugin.hpp"
#include "types.hpp"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace bible {

/// Options for search queries.
struct SearchOptions {
    int limit = 50;
    int offset = 0;
    std::string plugin_id;    ///< Empty = search all plugins
};

/// Options for verse retrieval.
struct GetChapterOptions {
    std::string plugin_id;    ///< Empty = get from all plugins (parallel)
};

/// The core scripture engine.
///
/// Thread-safety: NOT thread-safe. Create one instance per thread, or
/// synchronize externally. (Follows the same pattern as nomad DictEngine.)
class ScriptureEngine {
public:
    ScriptureEngine();
    ~ScriptureEngine();

    // Non-copyable, movable
    ScriptureEngine(const ScriptureEngine&) = delete;
    ScriptureEngine& operator=(const ScriptureEngine&) = delete;
    ScriptureEngine(ScriptureEngine&&) noexcept;
    ScriptureEngine& operator=(ScriptureEngine&&) noexcept;

    // -----------------------------------------------------------------------
    // Initialization
    // -----------------------------------------------------------------------

    /// Initialize the Strong's concordance lexicon.
    /// @param strongs_db_path Path to strongs.sqlite (TBESH + TBESG data).
    /// @return 0 on success, negative error code on failure.
    int init_strongs(std::string_view strongs_db_path);

    /// Register a scripture plugin (translation database).
    /// @param id Unique identifier (e.g., "kjv", "cuv_simp").
    /// @param db_path Path to the plugin's SQLite database.
    /// @param config Plugin configuration.
    /// @return 0 on success, negative error code on failure.
    int register_plugin(std::string_view id, std::string_view db_path,
                        const PluginConfig& config);

    /// Unregister a plugin by id.
    void unregister_plugin(std::string_view id);

    /// List registered plugin IDs.
    [[nodiscard]] std::vector<std::string> plugin_ids() const;

    // -----------------------------------------------------------------------
    // Navigation
    // -----------------------------------------------------------------------

    /// Get all books (merged across all plugins, or from a specific plugin).
    [[nodiscard]] std::vector<Book> get_books(
        std::string_view plugin_id = {},
        std::string_view testament = {}) const;

    /// Get a single book by ID.
    [[nodiscard]] Book get_book(int book_id,
                                std::string_view plugin_id = {}) const;

    /// Get all verses for a chapter, with parallel texts from all plugins.
    [[nodiscard]] std::vector<ParallelVerse> get_chapter(
        int book, int chapter,
        const GetChapterOptions& options = {}) const;

    /// Get a single verse with parallel texts.
    [[nodiscard]] ParallelVerse get_verse(int book, int chapter,
                                          int verse) const;

    // -----------------------------------------------------------------------
    // Reference parsing
    // -----------------------------------------------------------------------

    /// Parse a scripture reference string.
    /// Supports: "John 3:16", "Gen 1:1-3", "约翰福音 3:16", "Быт 1:1"
    [[nodiscard]] Reference parse_reference(std::string_view ref) const;

    /// Format a reference as a string.
    /// @param lang Language code for book names (e.g., "en", "zh").
    [[nodiscard]] std::string format_reference(const Reference& ref,
                                                std::string_view lang = "en") const;

    // -----------------------------------------------------------------------
    // Strong's Concordance
    // -----------------------------------------------------------------------

    /// Look up a Strong's entry by type and number.
    [[nodiscard]] StrongsEntry get_strongs(char type, int number) const;

    /// Get all verses containing a Strong's number.
    [[nodiscard]] std::vector<ParallelVerse> get_verses_with_strongs(
        char type, int number, int limit = 50) const;

    /// Count verses containing a Strong's number.
    [[nodiscard]] int count_verses_with_strongs(char type, int number) const;

    /// Parse Strong's markers from tagged text.
    /// Input: "In the {H7225}beginning {H430}God {H1254}created"
    /// Output: vector of ParsedWord with text + strongs info.
    static std::vector<ParsedWord> parse_strongs_text(std::string_view text);

    // -----------------------------------------------------------------------
    // Search
    // -----------------------------------------------------------------------

    /// Full-text search across all registered plugins.
    [[nodiscard]] std::pair<std::vector<SearchResult>, QueryStats>
    search(std::string_view query, const SearchOptions& options = {}) const;

    // -----------------------------------------------------------------------
    // Statistics (AI/MCP-ready)
    // -----------------------------------------------------------------------

    /// Compute corpus-wide statistics.
    [[nodiscard]] CorpusStats compute_stats() const;

    /// Get usage statistics for a Strong's number.
    [[nodiscard]] StrongsStats strongs_stats(char type, int number) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace bible
