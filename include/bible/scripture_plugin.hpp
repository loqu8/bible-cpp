#pragma once

/// @file scripture_plugin.hpp
/// @brief Plugin interface for registering scripture databases.
///
/// Each translation (KJV, CUV, Russian Synodal, Tao Te Ching, etc.) is a
/// plugin — a SQLite database conforming to a standard schema. The core
/// engine queries across all registered plugins transparently.

#include <string>
#include <string_view>

namespace bible {

/// Configuration for registering a scripture plugin.
struct PluginConfig {
    std::string language;           ///< ISO 639-1 code (e.g., "en", "zh", "ru")
    std::string name;               ///< Human-readable name (e.g., "King James Version")
    std::string name_native;        ///< Name in native language (e.g., "和合本")
    bool has_strongs = false;       ///< Whether text contains {H/G####} markers
    bool is_rtl = false;            ///< Right-to-left text (for future Hebrew/Arabic plugins)

    /// Optional: column name for the primary text in the verses table.
    /// Defaults to "text". Use this when a single database has multiple
    /// text columns (e.g., "text_cuv_simp", "text_kjv").
    std::string text_column = "text";

    /// Optional: column name for the plain (stripped) text.
    /// Defaults to "text_plain".
    std::string text_plain_column = "text_plain";
};

/// Interface that the ScriptureEngine uses to query a plugin's database.
///
/// The default implementation reads from a SQLite database conforming to
/// the standard schema (see README.md). Custom implementations can wrap
/// other data sources (e.g., in-memory, network, SWORD modules).
class IScripturePlugin {
public:
    virtual ~IScripturePlugin() = default;

    /// Unique identifier for this plugin (e.g., "kjv", "cuv_simp").
    [[nodiscard]] virtual std::string_view id() const = 0;

    /// Plugin configuration.
    [[nodiscard]] virtual const PluginConfig& config() const = 0;

    /// Path to the underlying database (empty for non-file plugins).
    [[nodiscard]] virtual std::string_view db_path() const = 0;

    /// Whether the plugin is initialized and ready.
    [[nodiscard]] virtual bool is_ready() const = 0;
};

}  // namespace bible
