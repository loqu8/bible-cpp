/// @file bible.cpp
/// @brief ScriptureEngine implementation.

#include "bible/bible.hpp"

namespace bible {

// ---------------------------------------------------------------------------
// Impl (pimpl)
// ---------------------------------------------------------------------------

struct ScriptureEngine::Impl {
    // TODO: SQLite connections, plugin registry, Strong's DB
};

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

ScriptureEngine::ScriptureEngine() : impl_(std::make_unique<Impl>()) {}
ScriptureEngine::~ScriptureEngine() = default;
ScriptureEngine::ScriptureEngine(ScriptureEngine&&) noexcept = default;
ScriptureEngine& ScriptureEngine::operator=(ScriptureEngine&&) noexcept = default;

int ScriptureEngine::init_strongs(std::string_view /*strongs_db_path*/) {
    // TODO: Open strongs.sqlite, verify schema
    return 0;
}

int ScriptureEngine::register_plugin(std::string_view /*id*/,
                                     std::string_view /*db_path*/,
                                     const PluginConfig& /*config*/) {
    // TODO: Open plugin database, verify schema, add to registry
    return 0;
}

void ScriptureEngine::unregister_plugin(std::string_view /*id*/) {
    // TODO
}

std::vector<std::string> ScriptureEngine::plugin_ids() const {
    return {};  // TODO
}

// ---------------------------------------------------------------------------
// Navigation
// ---------------------------------------------------------------------------

std::vector<Book> ScriptureEngine::get_books(std::string_view /*plugin_id*/,
                                             std::string_view /*testament*/) const {
    return {};  // TODO
}

Book ScriptureEngine::get_book(int /*book_id*/,
                               std::string_view /*plugin_id*/) const {
    return {};  // TODO
}

std::vector<ParallelVerse> ScriptureEngine::get_chapter(
    int /*book*/, int /*chapter*/, const GetChapterOptions& /*options*/) const {
    return {};  // TODO
}

ParallelVerse ScriptureEngine::get_verse(int /*book*/, int /*chapter*/,
                                         int /*verse*/) const {
    return {};  // TODO
}

// ---------------------------------------------------------------------------
// Reference parsing
// ---------------------------------------------------------------------------

Reference ScriptureEngine::parse_reference(std::string_view /*ref*/) const {
    return {};  // TODO: Implemented in reference.cpp
}

std::string ScriptureEngine::format_reference(const Reference& /*ref*/,
                                               std::string_view /*lang*/) const {
    return {};  // TODO
}

// ---------------------------------------------------------------------------
// Strong's Concordance
// ---------------------------------------------------------------------------

StrongsEntry ScriptureEngine::get_strongs(char /*type*/, int /*number*/) const {
    return {};  // TODO
}

std::vector<ParallelVerse> ScriptureEngine::get_verses_with_strongs(
    char /*type*/, int /*number*/, int /*limit*/) const {
    return {};  // TODO
}

int ScriptureEngine::count_verses_with_strongs(char /*type*/,
                                                int /*number*/) const {
    return 0;  // TODO
}

// ---------------------------------------------------------------------------
// Search
// ---------------------------------------------------------------------------

std::pair<std::vector<SearchResult>, QueryStats>
ScriptureEngine::search(std::string_view /*query*/,
                         const SearchOptions& /*options*/) const {
    return {{}, {}};  // TODO
}

// ---------------------------------------------------------------------------
// Statistics
// ---------------------------------------------------------------------------

CorpusStats ScriptureEngine::compute_stats() const {
    return {};  // TODO
}

StrongsStats ScriptureEngine::strongs_stats(char /*type*/, int /*number*/) const {
    return {};  // TODO
}

}  // namespace bible
