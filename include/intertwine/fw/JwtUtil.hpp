#ifndef INTERTWINE_CPP_FRAMEWORK_JWT_UTIL_HPP
#define INTERTWINE_CPP_FRAMEWORK_JWT_UTIL_HPP

#include <string>
#include <cstdint>
#include <set>
#include <vector>
#include <boost/function.hpp>
#include <boost/thread/mutex.hpp>

namespace intertwine {
namespace fw {

class JwtUtil {
public:
    static std::string sign(const std::string& secret, int expireSeconds = 7200);
    static bool verify(const std::string& token, const std::string& secret);
    static std::string extractBearer(const std::string& authHeader);
    static int64_t getExp(const std::string& token);

    static void revoke(const std::string& token);
    static bool isRevoked(const std::string& token);
    static void cleanup();
    static void setPersistCallback(boost::function<void(const std::string& token, int64_t expireAt)> cb);
    static void loadEntries(const std::vector<std::pair<std::string, int64_t>>& entries);
    static void setMaxEntries(int maxEntries);
    static size_t getBlacklistSize();
    static int getMaxEntries();
    static void setStartTimeSeconds(int64_t seconds);
    static int64_t getIat(const std::string& token);

private:
    static std::string base64UrlEncode(const std::string& data);
    static std::string base64UrlDecode(const std::string& data);
    static std::string hmacSha256(const std::string& key, const std::string& data);

    struct RevokedEntry {
        std::string token;
        int64_t expireAt;
        bool operator<(const RevokedEntry& o) const { return token < o.token; }
    };
    static std::set<RevokedEntry> s_blacklist;
    static boost::mutex s_blacklistMutex;
    static boost::function<void(const std::string&, int64_t)> s_persistCallback;
    static int s_maxEntries;
    static int64_t s_startTimeSeconds;
};

} // namespace fw
} // namespace intertwine

#endif // INTERTWINE_CPP_FRAMEWORK_JWT_UTIL_HPP
