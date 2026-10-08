#ifndef INTERTWINE_FW_FILE_TRANSFER_HEADERS_HPP
#define INTERTWINE_FW_FILE_TRANSFER_HEADERS_HPP

#include <string>

namespace intertwine {
namespace fw {

inline std::string fileContentDisposition(const std::string& name, bool inlineDisposition) {
    std::string fallback;
    std::string encoded;
    static const char hex[] = "0123456789ABCDEF";
    for (unsigned char ch : name) {
        fallback += ch >= 0x20 && ch < 0x7f && ch != '"' && ch != '\\'
            ? static_cast<char>(ch) : '_';
        // RFC 8187 attr-char; encode UTF-8 bytes and parameter delimiters.
        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
            (ch >= '0' && ch <= '9') || ch == '!' || ch == '#' ||
            ch == '$' || ch == '&' || ch == '+' || ch == '-' || ch == '.' ||
            ch == '^' || ch == '_' || ch == '`' || ch == '|' || ch == '~') {
            encoded += static_cast<char>(ch);
        } else {
            encoded += '%';
            encoded += hex[ch >> 4];
            encoded += hex[ch & 0x0f];
        }
    }
    return std::string(inlineDisposition ? "inline" : "attachment") +
        "; filename=\"" + fallback + "\"; filename*=UTF-8''" + encoded;
}

} // namespace fw
} // namespace intertwine

#endif
