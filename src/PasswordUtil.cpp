#include "intertwine/fw/PasswordUtil.hpp"

#ifdef HAVE_OPENSSL_CRYPTO
#include <openssl/evp.h>
#include <openssl/rand.h>
#endif

#include <cstring>
#include <sstream>
#include <vector>

namespace intertwine {
namespace fw {

std::string PasswordUtil::base64Encode(const unsigned char* data, int len) {
    static const char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    result.reserve((static_cast<size_t>(len) + 2) / 3 * 4);
    for (int i = 0; i < len; i += 3) {
        unsigned int n = static_cast<unsigned int>(data[i]) << 16;
        if (i + 1 < len) {
            n |= static_cast<unsigned int>(data[i + 1]) << 8;
        }
        if (i + 2 < len) {
            n |= static_cast<unsigned int>(data[i + 2]);
        }
        result += table[(n >> 18) & 0x3F];
        result += table[(n >> 12) & 0x3F];
        result += (i + 1 < len) ? table[(n >> 6) & 0x3F] : '=';
        result += (i + 2 < len) ? table[n & 0x3F] : '=';
    }
    return result;
}

std::string PasswordUtil::base64Decode(const std::string& encoded) {
    static const unsigned char decode_table[256] = {
        255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
        255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
        255,255,255,255,255,255,255,255,255,255,255, 62,255,255,255, 63,
         52, 53, 54, 55, 56, 57, 58, 59, 60, 61,255,255,255,  0,255,255,
        255,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
         15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,255,255,255,255,255,
        255, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
         41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51,255,255,255,255,255,
        255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
        255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
        255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
        255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
        255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
        255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
        255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
        255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    };
    std::string result;
    result.reserve(encoded.size() * 3 / 4);
    for (size_t i = 0; i + 3 < encoded.size(); i += 4) {
        unsigned char a = decode_table[static_cast<unsigned char>(encoded[i])];
        unsigned char b = decode_table[static_cast<unsigned char>(encoded[i + 1])];
        unsigned char c = decode_table[static_cast<unsigned char>(encoded[i + 2])];
        unsigned char d = decode_table[static_cast<unsigned char>(encoded[i + 3])];
        if (a == 255 || b == 255) {
            break;
        }
        result += static_cast<char>((a << 2) | (b >> 4));
        if (encoded[i + 2] != '=' && c != 255) {
            result += static_cast<char>(((b & 0x0F) << 4) | (c >> 2));
            if (encoded[i + 3] != '=' && d != 255) {
                result += static_cast<char>(((c & 0x03) << 6) | d);
            }
        }
    }
    return result;
}

std::string PasswordUtil::hash(const std::string& password) {
#ifdef HAVE_OPENSSL_CRYPTO
    /* 生成随机盐 */
    unsigned char salt[kSaltBytes];
    if (RAND_bytes(salt, kSaltBytes) != 1) {
        return ""; /* 随机数生成失败 */
    }

    /* PBKDF2-HMAC-SHA256 */
    unsigned char derived[kHashBytes];
    if (PKCS5_PBKDF2_HMAC(
            password.c_str(), static_cast<int>(password.size()),
            salt, kSaltBytes,
            kDefaultIterations,
            EVP_sha256(),
            kHashBytes, derived) != 1) {
        return "";
    }

    /* 编码为 $pbkdf2-sha256$iterations$salt$hash */
    std::ostringstream oss;
    oss << "$pbkdf2-sha256$" << kDefaultIterations << "$"
        << base64Encode(salt, kSaltBytes) << "$"
        << base64Encode(derived, kHashBytes);
    return oss.str();
#else
    (void)password;
    return "";
#endif
}

bool PasswordUtil::verify(const std::string& password, const std::string& encoded) {
#ifdef HAVE_OPENSSL_CRYPTO
    /* 解析 $pbkdf2-sha256$iterations$salt$hash */
    if (!isHashed(encoded)) {
        return false;
    }

    /* 跳过前缀 "$pbkdf2-sha256$" */
    const std::string prefix = "$pbkdf2-sha256$";
    std::string rest = encoded.substr(prefix.size());

    /* 分割 iterations$salt$hash */
    size_t d1 = rest.find('$');
    if (d1 == std::string::npos) {
        return false;
    }
    size_t d2 = rest.find('$', d1 + 1);
    if (d2 == std::string::npos) {
        return false;
    }

    int iterations = 0;
    try { iterations = std::stoi(rest.substr(0, d1)); } catch (...) { return false; }
    if (iterations <= 0) {
        return false;
    }

    std::string saltB64 = rest.substr(d1 + 1, d2 - d1 - 1);
    std::string hashB64 = rest.substr(d2 + 1);

    std::string saltRaw = base64Decode(saltB64);
    std::string hashExpected = base64Decode(hashB64);
    if (saltRaw.empty() || hashExpected.empty()) {
        return false;
    }

    int hashLen = static_cast<int>(hashExpected.size());

    /* 用相同参数派生 */
    std::vector<unsigned char> derived(static_cast<size_t>(hashLen));
    if (PKCS5_PBKDF2_HMAC(
            password.c_str(), static_cast<int>(password.size()),
            reinterpret_cast<const unsigned char*>(saltRaw.data()),
            static_cast<int>(saltRaw.size()),
            iterations,
            EVP_sha256(),
            hashLen, derived.data()) != 1) {
        return false;
    }

    /* 常量时间比较（防时序攻击） */
    if (static_cast<int>(hashExpected.size()) != hashLen) {
        return false;
    }
    volatile unsigned char diff = 0;
    for (int i = 0; i < hashLen; ++i) {
        diff |= static_cast<unsigned char>(derived[static_cast<size_t>(i)]) ^
                static_cast<unsigned char>(hashExpected[static_cast<size_t>(i)]);
    }
    return diff == 0;
#else
    (void)password;
    (void)encoded;
    return false;
#endif
}

bool PasswordUtil::isHashed(const std::string& s) {
    return s.size() > 15 && s.compare(0, 15, "$pbkdf2-sha256$") == 0;
}

} // namespace fw
} // namespace intertwine
