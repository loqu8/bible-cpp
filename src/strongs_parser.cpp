/// @file strongs_parser.cpp
/// @brief Strong's text parser and helper implementations.

#include "bible/types.hpp"

#include <charconv>

namespace bible {

// ---------------------------------------------------------------------------
// StrongsEntry::label()
// ---------------------------------------------------------------------------

std::string StrongsEntry::label() const {
    std::string result;
    result += type;
    result += std::to_string(number);
    return result;
}

// ---------------------------------------------------------------------------
// ParsedWord::strongs_label()
// ---------------------------------------------------------------------------

std::string ParsedWord::strongs_label() const {
    if (!has_strongs) return {};
    std::string result;
    result += strongs_type;
    result += std::to_string(strongs_number);
    return result;
}

// ---------------------------------------------------------------------------
// parse_strongs_text()
// ---------------------------------------------------------------------------

/// Parse Strong's markers from tagged text.
///
/// Supported formats:
///   - Bible SuperSearch: "{H7225}beginning" — marker before word
///   - Inline: "beginning{H7225}" — marker after word
///
/// The parser handles both. Words without markers are emitted as plain text.
std::vector<ParsedWord> ScriptureEngine::parse_strongs_text(
    std::string_view text) {
    std::vector<ParsedWord> words;
    if (text.empty()) return words;

    size_t i = 0;
    const size_t len = text.size();

    // Pending Strong's from a leading marker (Bible SuperSearch format)
    char pending_type = 0;
    int pending_number = 0;
    bool has_pending = false;

    while (i < len) {
        // Skip whitespace, accumulate as separator
        if (text[i] == ' ' || text[i] == '\t' || text[i] == '\n') {
            ++i;
            continue;
        }

        // Check for Strong's marker: {H####} or {G####}
        if (text[i] == '{' && i + 2 < len &&
            (text[i + 1] == 'H' || text[i + 1] == 'G')) {
            char type = text[i + 1];
            size_t start = i + 2;
            size_t end = text.find('}', start);
            if (end != std::string_view::npos) {
                int number = 0;
                auto sv = text.substr(start, end - start);
                auto [ptr, ec] = std::from_chars(
                    sv.data(), sv.data() + sv.size(), number);
                if (ec == std::errc{}) {
                    // If there's a pending marker and we see another marker,
                    // emit the pending as a standalone
                    if (has_pending) {
                        ParsedWord pw;
                        pw.text = "";
                        pw.has_strongs = true;
                        pw.strongs_type = pending_type;
                        pw.strongs_number = pending_number;
                        words.push_back(std::move(pw));
                    }
                    pending_type = type;
                    pending_number = number;
                    has_pending = true;
                    i = end + 1;
                    continue;
                }
            }
        }

        // Accumulate a word (non-whitespace, non-marker text)
        size_t word_start = i;
        while (i < len && text[i] != ' ' && text[i] != '\t' &&
               text[i] != '\n' && text[i] != '{') {
            ++i;
        }

        if (i > word_start) {
            ParsedWord pw;
            pw.text = std::string(text.substr(word_start, i - word_start));

            // Check for trailing marker: word{H####}
            if (i < len && text[i] == '{' && i + 2 < len &&
                (text[i + 1] == 'H' || text[i + 1] == 'G') && !has_pending) {
                char type = text[i + 1];
                size_t start = i + 2;
                size_t end = text.find('}', start);
                if (end != std::string_view::npos) {
                    int number = 0;
                    auto sv = text.substr(start, end - start);
                    auto [ptr, ec] = std::from_chars(
                        sv.data(), sv.data() + sv.size(), number);
                    if (ec == std::errc{}) {
                        pw.has_strongs = true;
                        pw.strongs_type = type;
                        pw.strongs_number = number;
                        i = end + 1;
                    }
                }
            } else if (has_pending) {
                // Apply pending leading marker to this word
                pw.has_strongs = true;
                pw.strongs_type = pending_type;
                pw.strongs_number = pending_number;
                has_pending = false;
            }

            words.push_back(std::move(pw));
        }
    }

    // Emit any trailing pending marker
    if (has_pending) {
        ParsedWord pw;
        pw.text = "";
        pw.has_strongs = true;
        pw.strongs_type = pending_type;
        pw.strongs_number = pending_number;
        words.push_back(std::move(pw));
    }

    return words;
}

}  // namespace bible
