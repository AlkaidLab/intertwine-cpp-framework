#include "intertwine/fw/JwtUtil.hpp"
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/crypto.h>
#include <boost/chrono.hpp>
#include <boost/thread/lock_guard.hpp>
#include <cstring>
#include <sstream>
#include <vector>
#include <cstdlib>

namespace intertwine {
namespace fw {

/* 黑名单静态成员 */
std::set<JwtUtil::RevokedEntry> JwtUtil::s_blacklist;
boost::mutex JwtUtil::s_blacklistMutex;
boost::function<void(const std::string&, int64_t)> JwtUtil::s_persistCallback;
int JwtUtil::s_maxEntries = 1000;
int64_t JwtUtil::s_startTimeSeconds = 0;

static int64_t nowSeconds() {
    return static_cast<int64_t>(boost::chrono::duration_cast<boost::chrono::seconds>(
        boost::chrono::system_clock::now().time_since_epoch()).count());
}

std::string JwtUtil::base64UrlEncode(const std::string& data) {
    static const char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    result.reserve((data.size() + 2) / 3 * 4);
    const unsigned char* src = reinterpret_cast<const unsigned char*>(data.data());
    size_t len = data.size();
    for (size_t i = 0; i < len; i += 3) {
        unsigned int n = static_cast<unsigned int>(src[i]) << 16;
        if (i + 1 < len) {
            n |= static_cast<unsigned int>(src[i + 1]) << 8;
        }
        if (i + 2 < len) {
            n |= static_cast<unsigned int>(src[i + 2]);
        }
        result += table[(n >> 18) & 0x3F];
        result += table[(n >> 12) & 0x3F];
        result += (i + 1 < len) ? table[(n >> 6) & 0x3F] : '=';
        result += (i + 2 < len) ? table[n & 0x3F] : '=';
    }
    /* base64 -> base64url: + -> -, / -> _, 去掉尾部 = */
    for (size_t i = 0; i < result.size(); ++i) {
        if (result[i] == '+') {
            result[i] = '-';
        } else if (result[i] == '/') {
            result[i] = '_';
        }
    }
    while (!result.empty() && result.back() == '=') {
        result.pop_back();
    }
    return result;
}

std::string JwtUtil::base64UrlDecode(const std::string& input) {
    std::string data = input;
    /* base64url -> base64 */
    for (size_t i = 0; i < data.size(); ++i) {
        if (data[i] == '-') {
            data[i] = '+';
        } else if (data[i] == '_') {
            data[i] = '/';
        }
    }
    while (data.size() % 4 != 0) {
        data += '=';
    }

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
    result.reserve(data.size() * 3 / 4);
    for (size_t i = 0; i + 3 < data.size(); i += 4) {
        unsigned char a = decode_table[static_cast<unsigned char>(data[i])];
        unsigned char b = decode_table[static_cast<unsigned char>(data[i + 1])];
        unsigned char c = decode_table[static_cast<unsigned char>(data[i + 2])];
        unsigned char d = decode_table[static_cast<unsigned char>(data[i + 3])];
        if (a == 255 || b == 255) {
            break;
        }
        result += static_cast<char>((a << 2) | (b >> 4));
        if (data[i + 2] != '=' && c != 255) {
            result += static_cast<char>(((b & 0x0F) << 4) | (c >> 2));
            if (data[i + 3] != '=' && d != 255) {
                result += static_cast<char>(((c & 0x03) << 6) | d);
            }
        }
    }
    return result;
}

std::string JwtUtil::hmacSha256(const std::string& key, const std::string& data) {
    unsigned char result[EVP_MAX_MD_SIZE];
    unsigned int len = 0;
    HMAC(EVP_sha256(),
         key.data(), static_cast<int>(key.size()),
         reinterpret_cast<const unsigned char*>(data.data()), data.size(),
         result, &len);
    return std::string(reinterpret_cast<char*>(result), len);
}

std::string JwtUtil::sign(const std::string& secret, int expireSeconds) {
    int64_t now = nowSeconds();
    int64_t exp = now + expireSeconds;

    std::string header = "{\"alg\":\"HS256\",\"typ\":\"JWT\"}";
    std::ostringstream payload;
    payload << "{\"sub\":\"admin\",\"iat\":" << now << ",\"exp\":" << exp << "}";

    std::string headerEnc = base64UrlEncode(header);
    std::string payloadEnc = base64UrlEncode(payload.str());
    std::string sigInput = headerEnc + "." + payloadEnc;
    std::string sig = hmacSha256(secret, sigInput);

    return sigInput + "." + base64UrlEncode(sig);
}

bool JwtUtil::verify(const std::string& token, const std::string& secret) {
    /* 拆分 header.payload.signature */
    size_t dot1 = token.find('.');
    if (dot1 == std::string::npos) {
        return false;
    }
    size_t dot2 = token.find('.', dot1 + 1);
    if (dot2 == std::string::npos) {
        return false;
    }
    if (token.find('.', dot2 + 1) != std::string::npos) {
        return false; /* 多余的 dot */
    }

    std::string sigInput = token.substr(0, dot2);
    std::string sigEncoded = token.substr(dot2 + 1);

    /* 验签：常量时间比较，防止时序攻击侧信道 */
    std::string expectedSig = base64UrlEncode(hmacSha256(secret, sigInput));
    if (expectedSig.size() != sigEncoded.size() ||
        CRYPTO_memcmp(expectedSig.data(), sigEncoded.data(), expectedSig.size()) != 0) {
        return false;
    }

    /* 检查过期 */
    int64_t exp = getExp(token);
    if (exp == 0 || nowSeconds() >= exp) {
        return false;
    }

    /* 拒绝在本次启动之前签发的 token（重启踢出旧会话） */
    if (s_startTimeSeconds > 0) {
        int64_t iat = getIat(token);
        if (iat > 0 && iat < s_startTimeSeconds) {
            return false;
        }
    }

    /* 检查黑名单 */
    if (isRevoked(token)) {
        return false;
    }

    return true;
}

std::string JwtUtil::extractBearer(const std::string& authHeader) {
    if (authHeader.size() > 7 && authHeader.compare(0, 7, "Bearer ") == 0) {
        return authHeader.substr(7);
    }
    return "";
}

int64_t JwtUtil::getExp(const std::string& token) {
    size_t dot1 = token.find('.');
    if (dot1 == std::string::npos) {
        return 0;
    }
    size_t dot2 = token.find('.', dot1 + 1);
    if (dot2 == std::string::npos) {
        return 0;
    }

    std::string payloadJson = base64UrlDecode(token.substr(dot1 + 1, dot2 - dot1 - 1));
    size_t expPos = payloadJson.find("\"exp\"");
    if (expPos == std::string::npos) {
        return 0;
    }
    expPos = payloadJson.find(':', expPos);
    if (expPos == std::string::npos) {
        return 0;
    }
    ++expPos;
    while (expPos < payloadJson.size() && (payloadJson[expPos] == ' ' || payloadJson[expPos] == '\t')) {
        ++expPos;
    }
    size_t expEnd = payloadJson.find_first_not_of("0123456789", expPos);
    if (expEnd == std::string::npos) {
        expEnd = payloadJson.size();
    }
    try { return std::stoll(payloadJson.substr(expPos, expEnd - expPos)); } catch (...) { return 0; }
}

int64_t JwtUtil::getIat(const std::string& token) {
    size_t dot1 = token.find('.');
    if (dot1 == std::string::npos) {
        return 0;
    }
    size_t dot2 = token.find('.', dot1 + 1);
    if (dot2 == std::string::npos) {
        return 0;
    }

    std::string payloadJson = base64UrlDecode(token.substr(dot1 + 1, dot2 - dot1 - 1));
    size_t iatPos = payloadJson.find("\"iat\"");
    if (iatPos == std::string::npos) {
        return 0;
    }
    iatPos = payloadJson.find(':', iatPos);
    if (iatPos == std::string::npos) {
        return 0;
    }
    ++iatPos;
    while (iatPos < payloadJson.size() && (payloadJson[iatPos] == ' ' || payloadJson[iatPos] == '\t')) {
        ++iatPos;
    }
    size_t iatEnd = payloadJson.find_first_not_of("0123456789", iatPos);
    if (iatEnd == std::string::npos) {
        iatEnd = payloadJson.size();
    }
    try { return std::stoll(payloadJson.substr(iatPos, iatEnd - iatPos)); } catch (...) { return 0; }
}

void JwtUtil::setStartTimeSeconds(int64_t seconds) {
    s_startTimeSeconds = seconds;
}

void JwtUtil::revoke(const std::string& token) {
    int64_t exp = getExp(token);
    if (exp == 0) {
        return; /* 无效 token 不入黑名单 */
    }
    boost::lock_guard<boost::mutex> lock(s_blacklistMutex);
    /* 顺带清理已过期条目 */
    int64_t now = nowSeconds();
    for (std::set<RevokedEntry>::iterator it = s_blacklist.begin(); it != s_blacklist.end(); ) {
        if (it->expireAt <= now) {
            it = s_blacklist.erase(it);
        } else {
            ++it;
        }
    }
    /* 超过上限时淘汰最早过期的条目 */
    while (s_maxEntries > 0 && static_cast<int>(s_blacklist.size()) >= s_maxEntries) {
        /* 找 expireAt 最小的条目淘汰 */
        std::set<RevokedEntry>::iterator oldest = s_blacklist.begin();
        for (std::set<RevokedEntry>::iterator it = s_blacklist.begin(); it != s_blacklist.end(); ++it) {
            if (it->expireAt < oldest->expireAt) {
                oldest = it;
            }
        }
        s_blacklist.erase(oldest);
    }
    RevokedEntry entry;
    entry.token = token;
    entry.expireAt = exp;
    s_blacklist.insert(entry);
    /* 持久化回调：同步写入数据库 */
    if (s_persistCallback) {
        s_persistCallback(token, exp);
    }
}

bool JwtUtil::isRevoked(const std::string& token) {
    boost::lock_guard<boost::mutex> lock(s_blacklistMutex);
    RevokedEntry query;
    query.token = token;
    query.expireAt = 0;
    return s_blacklist.find(query) != s_blacklist.end();
}

void JwtUtil::cleanup() {
    boost::lock_guard<boost::mutex> lock(s_blacklistMutex);
    int64_t now = nowSeconds();
    for (std::set<RevokedEntry>::iterator it = s_blacklist.begin(); it != s_blacklist.end(); ) {
        if (it->expireAt <= now) {
            it = s_blacklist.erase(it);
        } else {
            ++it;
        }
    }
}

void JwtUtil::setPersistCallback(boost::function<void(const std::string& token, int64_t expireAt)> cb) {
    boost::lock_guard<boost::mutex> lock(s_blacklistMutex);
    s_persistCallback = cb;
}

void JwtUtil::loadEntries(const std::vector<std::pair<std::string, int64_t>>& entries) {
    boost::lock_guard<boost::mutex> lock(s_blacklistMutex);
    for (size_t i = 0; i < entries.size(); ++i) {
        RevokedEntry e;
        e.token = entries[i].first;
        e.expireAt = entries[i].second;
        s_blacklist.insert(e);
    }
}

void JwtUtil::setMaxEntries(int maxEntries) {
    boost::lock_guard<boost::mutex> lock(s_blacklistMutex);
    s_maxEntries = maxEntries;
}

size_t JwtUtil::getBlacklistSize() {
    boost::lock_guard<boost::mutex> lock(s_blacklistMutex);
    return s_blacklist.size();
}

int JwtUtil::getMaxEntries() {
    boost::lock_guard<boost::mutex> lock(s_blacklistMutex);
    return s_maxEntries;
}

} // namespace fw
} // namespace intertwine
