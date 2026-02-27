/// @file bible.cpp
/// @brief ScriptureEngine implementation — SQLite-backed plugin registry.

#include "bible/bible.hpp"

#include <sqlite3.h>

#include <algorithm>
#include <charconv>
#include <chrono>
#include <map>
#include <set>
#include <string>
#include <tuple>

namespace bible {

// ---------------------------------------------------------------------------
// RAII statement wrapper
// ---------------------------------------------------------------------------

class StmtGuard {
public:
    StmtGuard(sqlite3* db, const char* sql) {
        sqlite3_prepare_v2(db, sql, -1, &stmt_, nullptr);
    }
    ~StmtGuard() { if (stmt_) sqlite3_finalize(stmt_); }
    StmtGuard(const StmtGuard&) = delete;
    StmtGuard& operator=(const StmtGuard&) = delete;

    explicit operator bool() const { return stmt_ != nullptr; }
    sqlite3_stmt* get() const { return stmt_; }

private:
    sqlite3_stmt* stmt_ = nullptr;
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static std::string col_text(sqlite3_stmt* s, int col) {
    auto p = sqlite3_column_text(s, col);
    return p ? std::string(reinterpret_cast<const char*>(p)) : std::string{};
}

static int col_int(sqlite3_stmt* s, int col) {
    return sqlite3_column_int(s, col);
}

// ---------------------------------------------------------------------------
// Impl (pimpl)
// ---------------------------------------------------------------------------

struct ScriptureEngine::Impl {
    struct PluginInfo {
        std::string id;
        std::string db_path;
        PluginConfig config;
        sqlite3* db = nullptr;  // borrowed from db_connections
    };

    std::map<std::string, PluginInfo> plugins;        // id → plugin
    std::map<std::string, sqlite3*> db_connections;   // path → sqlite3*
    std::map<std::string, int> db_refcount;           // path → refcount
    sqlite3* strongs_db = nullptr;
    std::string strongs_db_path;

    // Book cache (populated from first plugin registered)
    std::vector<Book> book_cache;
    // Book name → book_id index (lowercase)
    std::map<std::string, int> book_name_index;

    ~Impl() { close_all(); }

    sqlite3* open_or_reuse(const std::string& path) {
        auto it = db_connections.find(path);
        if (it != db_connections.end()) {
            db_refcount[path]++;
            return it->second;
        }
        sqlite3* db = nullptr;
        int rc = sqlite3_open_v2(path.c_str(), &db,
                                  SQLITE_OPEN_READONLY, nullptr);
        if (rc != SQLITE_OK) {
            if (db) sqlite3_close(db);
            return nullptr;
        }
        db_connections[path] = db;
        db_refcount[path] = 1;
        return db;
    }

    void release_db(const std::string& path) {
        auto rc_it = db_refcount.find(path);
        if (rc_it == db_refcount.end()) return;
        rc_it->second--;
        if (rc_it->second <= 0) {
            auto db_it = db_connections.find(path);
            if (db_it != db_connections.end()) {
                sqlite3_close(db_it->second);
                db_connections.erase(db_it);
            }
            db_refcount.erase(rc_it);
        }
    }

    void close_all() {
        for (auto& [path, db] : db_connections) {
            sqlite3_close(db);
        }
        db_connections.clear();
        db_refcount.clear();
        plugins.clear();
        strongs_db = nullptr;
    }

    void populate_book_cache(sqlite3* db) {
        if (!book_cache.empty()) return;

        StmtGuard stmt(db, "SELECT id, name_en, name_zh, abbr_en, "
                           "testament, chapter_count FROM books ORDER BY id");
        if (!stmt) return;

        while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
            Book b;
            b.id = col_int(stmt.get(), 0);
            b.name = col_text(stmt.get(), 1);
            b.name_alt = col_text(stmt.get(), 2);
            b.abbr = col_text(stmt.get(), 3);
            b.testament = col_text(stmt.get(), 4);
            b.chapter_count = col_int(stmt.get(), 5);
            book_cache.push_back(std::move(b));
        }

        build_name_index(db);
    }

    void build_name_index(sqlite3* db) {
        book_name_index.clear();

        StmtGuard stmt(db, "SELECT id, name_en, name_zh, name_zh_trad, "
                           "abbr_en FROM books");
        if (!stmt) return;

        while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
            int id = col_int(stmt.get(), 0);
            auto add = [&](const std::string& name) {
                if (name.empty()) return;
                // Store lowercase for case-insensitive lookup
                std::string lower = name;
                std::transform(lower.begin(), lower.end(), lower.begin(),
                               [](unsigned char c) { return std::tolower(c); });
                book_name_index[lower] = id;
            };
            add(col_text(stmt.get(), 1));  // name_en
            add(col_text(stmt.get(), 2));  // name_zh
            add(col_text(stmt.get(), 3));  // name_zh_trad
            add(col_text(stmt.get(), 4));  // abbr_en
        }
    }
};

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

ScriptureEngine::ScriptureEngine() : impl_(std::make_unique<Impl>()) {}
ScriptureEngine::~ScriptureEngine() = default;
ScriptureEngine::ScriptureEngine(ScriptureEngine&&) noexcept = default;
ScriptureEngine& ScriptureEngine::operator=(ScriptureEngine&&) noexcept = default;

int ScriptureEngine::init_strongs(std::string_view strongs_db_path) {
    std::string path(strongs_db_path);
    auto* db = impl_->open_or_reuse(path);
    if (!db) return -1;

    // Verify strongs table exists
    StmtGuard stmt(db, "SELECT COUNT(*) FROM strongs LIMIT 1");
    if (!stmt || sqlite3_step(stmt.get()) != SQLITE_ROW) {
        impl_->release_db(path);
        return -2;
    }

    impl_->strongs_db = db;
    impl_->strongs_db_path = path;
    return 0;
}

int ScriptureEngine::register_plugin(std::string_view id,
                                     std::string_view db_path,
                                     const PluginConfig& config) {
    std::string sid(id);
    std::string spath(db_path);

    if (impl_->plugins.count(sid)) return -1;  // already registered

    auto* db = impl_->open_or_reuse(spath);
    if (!db) return -2;

    // Verify verses table exists
    StmtGuard stmt(db, "SELECT COUNT(*) FROM verses LIMIT 1");
    if (!stmt || sqlite3_step(stmt.get()) != SQLITE_ROW) {
        impl_->release_db(spath);
        return -3;
    }

    Impl::PluginInfo info;
    info.id = sid;
    info.db_path = spath;
    info.config = config;
    info.db = db;
    impl_->plugins[sid] = std::move(info);

    // Populate book cache from first plugin
    impl_->populate_book_cache(db);

    return 0;
}

void ScriptureEngine::unregister_plugin(std::string_view id) {
    std::string sid(id);
    auto it = impl_->plugins.find(sid);
    if (it == impl_->plugins.end()) return;

    impl_->release_db(it->second.db_path);
    impl_->plugins.erase(it);
}

std::vector<std::string> ScriptureEngine::plugin_ids() const {
    std::vector<std::string> ids;
    ids.reserve(impl_->plugins.size());
    for (const auto& [id, _] : impl_->plugins) {
        ids.push_back(id);
    }
    return ids;
}

// ---------------------------------------------------------------------------
// Navigation
// ---------------------------------------------------------------------------

std::vector<Book> ScriptureEngine::get_books(std::string_view /*plugin_id*/,
                                             std::string_view testament) const {
    if (testament.empty()) return impl_->book_cache;

    std::vector<Book> filtered;
    for (const auto& b : impl_->book_cache) {
        if (b.testament == testament) {
            filtered.push_back(b);
        }
    }
    return filtered;
}

Book ScriptureEngine::get_book(int book_id,
                               std::string_view /*plugin_id*/) const {
    for (const auto& b : impl_->book_cache) {
        if (b.id == book_id) return b;
    }
    return {};
}

std::vector<ParallelVerse> ScriptureEngine::get_chapter(
    int book, int chapter, const GetChapterOptions& options) const {

    // Determine which plugins to query
    std::vector<const Impl::PluginInfo*> targets;
    if (!options.plugin_id.empty()) {
        auto it = impl_->plugins.find(options.plugin_id);
        if (it != impl_->plugins.end()) targets.push_back(&it->second);
    } else {
        for (const auto& [_, p] : impl_->plugins) targets.push_back(&p);
    }

    // Map: verse_num → ParallelVerse
    std::map<int, ParallelVerse> verse_map;

    for (const auto* pi : targets) {
        std::string sql = "SELECT book, chapter, verse, " +
                          pi->config.text_column + ", " +
                          pi->config.text_plain_column +
                          " FROM verses WHERE book = ? AND chapter = ? "
                          "ORDER BY verse";

        StmtGuard stmt(pi->db, sql.c_str());
        if (!stmt) continue;

        sqlite3_bind_int(stmt.get(), 1, book);
        sqlite3_bind_int(stmt.get(), 2, chapter);

        while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
            int vnum = col_int(stmt.get(), 2);

            Verse v;
            v.book = col_int(stmt.get(), 0);
            v.chapter = col_int(stmt.get(), 1);
            v.verse = vnum;
            v.text = col_text(stmt.get(), 3);
            v.text_plain = col_text(stmt.get(), 4);
            v.plugin_id = pi->id;

            auto& pv = verse_map[vnum];
            pv.book = v.book;
            pv.chapter = v.chapter;
            pv.verse = vnum;
            pv.texts.emplace_back(pi->id, std::move(v));
        }
    }

    // Convert map to sorted vector
    std::vector<ParallelVerse> result;
    result.reserve(verse_map.size());
    for (auto& [_, pv] : verse_map) {
        result.push_back(std::move(pv));
    }
    return result;
}

ParallelVerse ScriptureEngine::get_verse(int book, int chapter,
                                         int verse) const {
    ParallelVerse pv;
    pv.book = book;
    pv.chapter = chapter;
    pv.verse = verse;

    for (const auto& [pid, pi] : impl_->plugins) {
        std::string sql = "SELECT " + pi.config.text_column + ", " +
                          pi.config.text_plain_column +
                          " FROM verses WHERE book = ? AND chapter = ? "
                          "AND verse = ? LIMIT 1";

        StmtGuard stmt(pi.db, sql.c_str());
        if (!stmt) continue;

        sqlite3_bind_int(stmt.get(), 1, book);
        sqlite3_bind_int(stmt.get(), 2, chapter);
        sqlite3_bind_int(stmt.get(), 3, verse);

        if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
            Verse v;
            v.book = book;
            v.chapter = chapter;
            v.verse = verse;
            v.text = col_text(stmt.get(), 0);
            v.text_plain = col_text(stmt.get(), 1);
            v.plugin_id = pid;
            pv.texts.emplace_back(pid, std::move(v));
        }
    }
    return pv;
}

// ---------------------------------------------------------------------------
// Strong's Concordance
// ---------------------------------------------------------------------------

StrongsEntry ScriptureEngine::get_strongs(char type, int number) const {
    if (!impl_->strongs_db) return {};

    StmtGuard stmt(impl_->strongs_db,
        "SELECT number, type, language, lemma, translit, pronunciation, "
        "kjv_def, strongs_def FROM strongs "
        "WHERE type = ? AND number = ? LIMIT 1");
    if (!stmt) return {};

    char type_str[2] = {type, 0};
    sqlite3_bind_text(stmt.get(), 1, type_str, 1, SQLITE_STATIC);
    sqlite3_bind_int(stmt.get(), 2, number);

    if (sqlite3_step(stmt.get()) != SQLITE_ROW) return {};

    StrongsEntry e;
    e.number = col_int(stmt.get(), 0);
    auto t = col_text(stmt.get(), 1);
    e.type = t.empty() ? type : t[0];
    e.language = col_text(stmt.get(), 2);
    e.lemma = col_text(stmt.get(), 3);
    e.translit = col_text(stmt.get(), 4);
    e.pronunciation = col_text(stmt.get(), 5);
    e.gloss = col_text(stmt.get(), 6);       // kjv_def → gloss
    e.definition = col_text(stmt.get(), 7);  // strongs_def → definition
    return e;
}

std::vector<ParallelVerse> ScriptureEngine::get_verses_with_strongs(
    char type, int number, int limit) const {

    // Build marker pattern: {H430} or {G3056}
    std::string marker = "{" + std::string(1, type) +
                         std::to_string(number) + "}";
    std::string pattern = "%" + marker + "%";

    // Search across all strongs-enabled plugins, dedup by (book,chapter,verse)
    std::map<std::tuple<int,int,int>, ParallelVerse> result_map;

    for (const auto& [pid, pi] : impl_->plugins) {
        if (!pi.config.has_strongs) continue;

        std::string sql = "SELECT book, chapter, verse, " +
                          pi.config.text_column + ", " +
                          pi.config.text_plain_column +
                          " FROM verses WHERE " +
                          pi.config.text_column +
                          " LIKE ? ORDER BY book, chapter, verse LIMIT ?";

        StmtGuard stmt(pi.db, sql.c_str());
        if (!stmt) continue;

        sqlite3_bind_text(stmt.get(), 1, pattern.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt.get(), 2, limit);

        while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
            int b = col_int(stmt.get(), 0);
            int c = col_int(stmt.get(), 1);
            int vn = col_int(stmt.get(), 2);
            auto key = std::make_tuple(b, c, vn);

            Verse v;
            v.book = b;
            v.chapter = c;
            v.verse = vn;
            v.text = col_text(stmt.get(), 3);
            v.text_plain = col_text(stmt.get(), 4);
            v.plugin_id = pid;

            auto& pv = result_map[key];
            pv.book = b;
            pv.chapter = c;
            pv.verse = vn;
            pv.texts.emplace_back(pid, std::move(v));
        }
    }

    std::vector<ParallelVerse> result;
    result.reserve(result_map.size());
    for (auto& [_, pv] : result_map) {
        result.push_back(std::move(pv));
        if (static_cast<int>(result.size()) >= limit) break;
    }
    return result;
}

int ScriptureEngine::count_verses_with_strongs(char type, int number) const {
    std::string marker = "{" + std::string(1, type) +
                         std::to_string(number) + "}";
    std::string pattern = "%" + marker + "%";

    // Count across all strongs-enabled plugins, take max (same DB = same count)
    int max_count = 0;
    for (const auto& [_, pi] : impl_->plugins) {
        if (!pi.config.has_strongs) continue;

        std::string sql = "SELECT COUNT(*) FROM verses WHERE " +
                          pi.config.text_column + " LIKE ?";

        StmtGuard stmt(pi.db, sql.c_str());
        if (!stmt) continue;

        sqlite3_bind_text(stmt.get(), 1, pattern.c_str(), -1,
                          SQLITE_TRANSIENT);

        if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
            int count = col_int(stmt.get(), 0);
            if (count > max_count) max_count = count;
        }
    }
    return max_count;
}

// ---------------------------------------------------------------------------
// Search
// ---------------------------------------------------------------------------

std::pair<std::vector<SearchResult>, QueryStats>
ScriptureEngine::search(std::string_view query,
                         const SearchOptions& options) const {
    auto start = std::chrono::steady_clock::now();

    std::string like_pattern = "%" + std::string(query) + "%";
    std::vector<SearchResult> results;
    std::set<int> books_seen;
    int translations_searched = 0;

    // Determine which plugins to search
    std::vector<const Impl::PluginInfo*> targets;
    if (!options.plugin_id.empty()) {
        auto it = impl_->plugins.find(options.plugin_id);
        if (it != impl_->plugins.end()) targets.push_back(&it->second);
    } else {
        for (const auto& [_, p] : impl_->plugins) targets.push_back(&p);
    }

    for (const auto* pi : targets) {
        ++translations_searched;

        std::string sql = "SELECT book, chapter, verse, " +
                          pi->config.text_column + ", " +
                          pi->config.text_plain_column +
                          " FROM verses WHERE " +
                          pi->config.text_plain_column + " LIKE ? "
                          "ORDER BY book, chapter, verse LIMIT ?";

        StmtGuard stmt(pi->db, sql.c_str());
        if (!stmt) continue;

        sqlite3_bind_text(stmt.get(), 1, like_pattern.c_str(), -1,
                          SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt.get(), 2, options.limit);

        while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
            SearchResult sr;
            sr.verse.book = col_int(stmt.get(), 0);
            sr.verse.chapter = col_int(stmt.get(), 1);
            sr.verse.verse = col_int(stmt.get(), 2);
            sr.verse.text = col_text(stmt.get(), 3);
            sr.verse.text_plain = col_text(stmt.get(), 4);
            sr.verse.plugin_id = pi->id;
            sr.relevance = 1.0f;  // LIKE search, no ranking
            books_seen.insert(sr.verse.book);
            results.push_back(std::move(sr));
        }
    }

    auto end = std::chrono::steady_clock::now();
    double ms = std::chrono::duration<double, std::milli>(end - start).count();

    QueryStats qs;
    qs.query_time_ms = ms;
    qs.total_matches = static_cast<int>(results.size());
    qs.books_matched = static_cast<int>(books_seen.size());
    qs.translations_searched = translations_searched;

    return {std::move(results), qs};
}

// ---------------------------------------------------------------------------
// Statistics
// ---------------------------------------------------------------------------

CorpusStats ScriptureEngine::compute_stats() const {
    CorpusStats stats;
    stats.plugins_registered = static_cast<int>(impl_->plugins.size());
    stats.total_books = static_cast<int>(impl_->book_cache.size());

    // Sum chapter counts
    for (const auto& b : impl_->book_cache) {
        stats.total_chapters += b.chapter_count;
    }

    // Count verses per plugin
    for (const auto& [pid, pi] : impl_->plugins) {
        StmtGuard stmt(pi.db, "SELECT COUNT(*) FROM verses");
        if (stmt && sqlite3_step(stmt.get()) == SQLITE_ROW) {
            int count = col_int(stmt.get(), 0);
            stats.total_verses += count;
            stats.verses_per_plugin.emplace_back(pid, count);
        }
    }

    // Strong's counts
    if (impl_->strongs_db) {
        {
            StmtGuard stmt(impl_->strongs_db,
                "SELECT COUNT(*) FROM strongs WHERE type = 'H'");
            if (stmt && sqlite3_step(stmt.get()) == SQLITE_ROW) {
                stats.strongs_hebrew_entries = col_int(stmt.get(), 0);
            }
        }
        {
            StmtGuard stmt(impl_->strongs_db,
                "SELECT COUNT(*) FROM strongs WHERE type = 'G'");
            if (stmt && sqlite3_step(stmt.get()) == SQLITE_ROW) {
                stats.strongs_greek_entries = col_int(stmt.get(), 0);
            }
        }
    }

    return stats;
}

StrongsStats ScriptureEngine::strongs_stats(char type, int number) const {
    StrongsStats ss;
    ss.type = type;
    ss.number = number;

    std::string marker = "{" + std::string(1, type) +
                         std::to_string(number) + "}";
    std::string pattern = "%" + marker + "%";

    for (const auto& [_, pi] : impl_->plugins) {
        if (!pi.config.has_strongs) continue;

        std::string sql = "SELECT COUNT(*) FROM verses WHERE " +
                          pi.config.text_column + " LIKE ?";

        StmtGuard stmt(pi.db, sql.c_str());
        if (!stmt) continue;

        sqlite3_bind_text(stmt.get(), 1, pattern.c_str(), -1,
                          SQLITE_TRANSIENT);

        if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
            int count = col_int(stmt.get(), 0);
            if (count > 0) {
                ss.verse_count += count;
                ss.plugin_count++;
            }
        }
    }

    return ss;
}

// ---------------------------------------------------------------------------
// Reference parsing
// ---------------------------------------------------------------------------

Reference ScriptureEngine::parse_reference(std::string_view ref) const {
    Reference result;
    if (ref.empty()) return result;

    // Trim leading/trailing whitespace
    size_t start = 0;
    while (start < ref.size() && (ref[start] == ' ' || ref[start] == '\t'))
        ++start;
    size_t end = ref.size();
    while (end > start && (ref[end - 1] == ' ' || ref[end - 1] == '\t'))
        --end;
    if (start >= end) return result;

    auto trimmed = ref.substr(start, end - start);

    // Find the last colon — separates chapter:verse
    auto colon_pos = trimmed.rfind(':');
    if (colon_pos == std::string_view::npos) {
        // No colon — might be "Genesis 1" (chapter only)
        auto space_pos = trimmed.rfind(' ');
        if (space_pos == std::string_view::npos) return result;

        auto book_part = trimmed.substr(0, space_pos);
        auto ch_part = trimmed.substr(space_pos + 1);

        int chapter = 0;
        auto [ptr, ec] = std::from_chars(ch_part.data(),
                                          ch_part.data() + ch_part.size(),
                                          chapter);
        if (ec != std::errc{}) return result;

        std::string lower(book_part);
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        while (!lower.empty() && lower.back() == ' ') lower.pop_back();

        auto it = impl_->book_name_index.find(lower);
        if (it == impl_->book_name_index.end()) return result;

        result.book = it->second;
        result.chapter = chapter;
        result.valid = true;
        return result;
    }

    // Extract verse and optional verse_end after the colon
    auto after_colon = trimmed.substr(colon_pos + 1);
    int verse = 0, verse_end = 0;

    auto dash_pos = after_colon.find('-');
    if (dash_pos != std::string_view::npos) {
        auto v1 = after_colon.substr(0, dash_pos);
        auto v2 = after_colon.substr(dash_pos + 1);
        auto [p1, e1] = std::from_chars(v1.data(), v1.data() + v1.size(), verse);
        if (e1 != std::errc{}) return result;
        auto [p2, e2] = std::from_chars(v2.data(), v2.data() + v2.size(), verse_end);
        if (e2 != std::errc{}) return result;
    } else {
        auto [ptr, ec] = std::from_chars(after_colon.data(),
                                          after_colon.data() + after_colon.size(),
                                          verse);
        if (ec != std::errc{}) return result;
    }

    // Extract chapter: digits immediately before the colon
    auto before_colon = trimmed.substr(0, colon_pos);
    size_t ch_start = before_colon.size();
    while (ch_start > 0 && before_colon[ch_start - 1] >= '0' &&
           before_colon[ch_start - 1] <= '9') {
        --ch_start;
    }
    if (ch_start == before_colon.size()) return result;

    auto ch_part = before_colon.substr(ch_start);
    int chapter = 0;
    {
        auto [ptr, ec] = std::from_chars(ch_part.data(),
                                          ch_part.data() + ch_part.size(),
                                          chapter);
        if (ec != std::errc{}) return result;
    }

    // Book name is everything before the chapter digits
    auto book_part = before_colon.substr(0, ch_start);
    while (!book_part.empty() &&
           (book_part.back() == ' ' || book_part.back() == '\t')) {
        book_part = book_part.substr(0, book_part.size() - 1);
    }
    if (book_part.empty()) return result;

    std::string lower(book_part);
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    auto it = impl_->book_name_index.find(lower);
    if (it == impl_->book_name_index.end()) return result;

    result.book = it->second;
    result.chapter = chapter;
    result.verse = verse;
    result.verse_end = verse_end;
    result.valid = true;
    return result;
}

std::string ScriptureEngine::format_reference(const Reference& ref,
                                               std::string_view lang) const {
    if (!ref.valid) return {};

    std::string book_name;
    for (const auto& b : impl_->book_cache) {
        if (b.id == ref.book) {
            if (lang == "zh" && !b.name_alt.empty()) {
                book_name = b.name_alt;
            } else {
                book_name = b.name;
            }
            break;
        }
    }
    if (book_name.empty()) return {};

    std::string result = book_name + " " + std::to_string(ref.chapter);
    if (ref.verse > 0) {
        result += ":" + std::to_string(ref.verse);
        if (ref.verse_end > 0) {
            result += "-" + std::to_string(ref.verse_end);
        }
    }
    return result;
}

}  // namespace bible
