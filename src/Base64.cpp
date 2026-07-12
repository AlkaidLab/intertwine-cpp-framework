#include "intertwine/fw/Base64.hpp"
#include <hv/base64.h>

namespace intertwine {
namespace fw {

std::string Base64::encode(const unsigned char* data, size_t len) {
    if (!data || len == 0) return "";
    return hv::Base64Encode(data, static_cast<unsigned int>(len));
}

std::string Base64::decode(const char* data, size_t len) {
    if (!data || len == 0) return "";
    return hv::Base64Decode(data, static_cast<unsigned int>(len));
}

std::string Base64::encode(const std::string& data) {
    if (data.empty()) return "";
    return encode(reinterpret_cast<const unsigned char*>(data.c_str()), data.length());
}

std::string Base64::decode(const std::string& encoded) {
    if (encoded.empty()) return "";
    return decode(encoded.c_str(), encoded.length());
}

std::string Base64::encode(const std::vector<unsigned char>& data) {
    if (data.empty()) return "";
    return encode(data.data(), data.size());
}

} // namespace fw
} // namespace intertwine
