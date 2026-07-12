#ifndef INTERTWINE_CPP_FRAMEWORK_PASSWORD_UTIL_HPP
#define INTERTWINE_CPP_FRAMEWORK_PASSWORD_UTIL_HPP

#include <string>

namespace intertwine {
namespace fw {

/**
 * 密码哈希工具：使用 PBKDF2-HMAC-SHA256 + 随机盐值。
 * 存储格式：$pbkdf2-sha256$<iterations>$<base64_salt>$<base64_hash>
 * 默认 310000 次迭代（OWASP 2023 推荐），32 字节盐，32 字节哈希。
 */
class PasswordUtil {
public:
    static std::string hash(const std::string& password);
    static bool verify(const std::string& password, const std::string& encoded);
    static bool isHashed(const std::string& s);

private:
    static const int kDefaultIterations = 310000;
    static const int kSaltBytes = 32;
    static const int kHashBytes = 32;

    static std::string base64Encode(const unsigned char* data, int len);
    static std::string base64Decode(const std::string& encoded);
};

} // namespace fw
} // namespace intertwine

#endif // INTERTWINE_CPP_FRAMEWORK_PASSWORD_UTIL_HPP
