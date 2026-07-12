#include "intertwine/fw/HashUtil.hpp"
#include <fstream>
#include <vector>

#ifdef HAVE_OPENSSL_CRYPTO
#include <openssl/evp.h>
#endif

namespace intertwine {
namespace fw {

#ifdef HAVE_OPENSSL_CRYPTO
namespace {

const size_t BUF_SIZE = 65536;

std::string toHex(const unsigned char* data, size_t len) {
    static const char hex[] = "0123456789abcdef";
    std::string out;
    out.reserve(len * 2);
    for (size_t i = 0; i < len; ++i) {
        out += hex[(data[i] >> 4) & 0x0F];
        out += hex[data[i] & 0x0F];
    }
    return out;
}

} // namespace
#endif

std::string HashUtil::fileMD5Hex(const std::string& filePath) {
#ifdef HAVE_OPENSSL_CRYPTO
    std::ifstream ifs(filePath, std::ios::binary);
    if (!ifs) {
        return "";
    }
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) {
        return "";
    }
    if (EVP_DigestInit_ex(ctx, EVP_md5(), nullptr) != 1) {
        EVP_MD_CTX_free(ctx);
        return "";
    }
    std::vector<char> buf(BUF_SIZE);
    while (ifs.read(buf.data(), static_cast<std::streamsize>(buf.size())) || ifs.gcount() > 0) {
        unsigned int n = static_cast<unsigned int>(ifs.gcount());
        if (EVP_DigestUpdate(ctx, buf.data(), n) != 1) {
            EVP_MD_CTX_free(ctx);
            return "";
        }
        if (!ifs) {
            break;
        }
    }
    unsigned char md[EVP_MAX_MD_SIZE];
    unsigned int mdLen = 0;
    if (EVP_DigestFinal_ex(ctx, md, &mdLen) != 1) {
        EVP_MD_CTX_free(ctx);
        return "";
    }
    EVP_MD_CTX_free(ctx);
    return toHex(md, mdLen);
#else
    (void)filePath;
    return "";
#endif
}

std::string HashUtil::fileSHA256Hex(const std::string& filePath) {
#ifdef HAVE_OPENSSL_CRYPTO
    std::ifstream ifs(filePath, std::ios::binary);
    if (!ifs) {
        return "";
    }
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) {
        return "";
    }
    if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1) {
        EVP_MD_CTX_free(ctx);
        return "";
    }
    std::vector<char> buf(BUF_SIZE);
    while (ifs.read(buf.data(), static_cast<std::streamsize>(buf.size())) || ifs.gcount() > 0) {
        unsigned int n = static_cast<unsigned int>(ifs.gcount());
        if (EVP_DigestUpdate(ctx, buf.data(), n) != 1) {
            EVP_MD_CTX_free(ctx);
            return "";
        }
        if (!ifs) {
            break;
        }
    }
    unsigned char md[EVP_MAX_MD_SIZE];
    unsigned int mdLen = 0;
    if (EVP_DigestFinal_ex(ctx, md, &mdLen) != 1) {
        EVP_MD_CTX_free(ctx);
        return "";
    }
    EVP_MD_CTX_free(ctx);
    return toHex(md, mdLen);
#else
    (void)filePath;
    return "";
#endif
}

bool HashUtil::verify(const std::string& filePath,
                      const std::string& expectedMD5,
                      const std::string& expectedSHA256) {
#ifndef HAVE_OPENSSL_CRYPTO
    (void)filePath;
    return expectedMD5.empty() && expectedSHA256.empty();
#else
    if (!expectedMD5.empty()) {
        std::string actual = fileMD5Hex(filePath);
        if (actual.empty()) {
            return false;
        }
        if (actual.size() != expectedMD5.size()) {
            return false;
        }
        for (size_t i = 0; i < actual.size(); ++i) {
            char a = actual[i];
            char b = expectedMD5[i];
            if (a >= 'A' && a <= 'Z') {
                a += 32;
            }
            if (b >= 'A' && b <= 'Z') {
                b += 32;
            }
            if (a != b) {
                return false;
            }
        }
    }
    if (!expectedSHA256.empty()) {
        std::string actual = fileSHA256Hex(filePath);
        if (actual.empty()) {
            return false;
        }
        if (actual.size() != expectedSHA256.size()) {
            return false;
        }
        for (size_t i = 0; i < actual.size(); ++i) {
            char a = actual[i];
            char b = expectedSHA256[i];
            if (a >= 'A' && a <= 'Z') {
                a += 32;
            }
            if (b >= 'A' && b <= 'Z') {
                b += 32;
            }
            if (a != b) {
                return false;
            }
        }
    }
    return true;
#endif
}

} // namespace fw
} // namespace intertwine
