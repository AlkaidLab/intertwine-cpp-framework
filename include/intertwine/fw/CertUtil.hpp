#ifndef INTERTWINE_CPP_FRAMEWORK_CERT_UTIL_HPP
#define INTERTWINE_CPP_FRAMEWORK_CERT_UTIL_HPP

#include <string>
#include <vector>

namespace intertwine {
namespace fw {

struct CertInfo {
    std::string subject;
    std::vector<std::string> domains;
    std::string notBefore;
    std::string notAfter;
    bool selfSigned;
};

class CertUtil {
public:
    static bool generateSelfSigned(const std::string& certPath,
                                   const std::string& keyPath,
                                   int days = 3650,
                                   const std::string& cn = "Self-Signed");

    static bool readCertInfo(const std::string& certPath, CertInfo& info);
};

} // namespace fw
} // namespace intertwine

#endif // INTERTWINE_CPP_FRAMEWORK_CERT_UTIL_HPP
