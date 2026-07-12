#ifndef INTERTWINE_CPP_FRAMEWORK_HASH_UTIL_HPP
#define INTERTWINE_CPP_FRAMEWORK_HASH_UTIL_HPP

#include <string>

namespace intertwine {
namespace fw {

class HashUtil {
public:
    static std::string fileMD5Hex(const std::string& filePath);
    static std::string fileSHA256Hex(const std::string& filePath);
    static bool verify(const std::string& filePath,
                       const std::string& expectedMD5,
                       const std::string& expectedSHA256);
};

} // namespace fw
} // namespace intertwine

#endif // INTERTWINE_CPP_FRAMEWORK_HASH_UTIL_HPP
