#ifndef INTERTWINE_CPP_FRAMEWORK_PATHGUARD_HPP
#define INTERTWINE_CPP_FRAMEWORK_PATHGUARD_HPP

#include <string>
#include <cstring>
#include <boost/filesystem.hpp>

namespace intertwine {
namespace fw {

class PathGuard {
public:
    static bool isSafe(const std::string& path, std::string& reason) {
        if (path.empty()) {
            reason = "path is empty";
            return false;
        }

        std::string resolved;
        boost::system::error_code ec;
        boost::filesystem::path canon = boost::filesystem::canonical(path, ec);
        if (!ec) {
            resolved = canon.string();
        } else {
            if (path[0] != '/') {
                boost::filesystem::path cwd = boost::filesystem::current_path(ec);
                if (!ec) {
                    resolved = (cwd / path).string();
                } else {
                    resolved = path;
                }
            } else {
                resolved = path;
            }
        }

        while (resolved.size() > 1 && resolved[resolved.size() - 1] == '/') {
            resolved.erase(resolved.size() - 1);
        }

        if (resolved == "/") {
            reason = "path resolves to filesystem root '/'";
            return false;
        }

        static const char* const BLOCKED[] = {
            "/bin", "/sbin", "/usr", "/usr/bin", "/usr/sbin", "/usr/lib", "/usr/lib64",
            "/lib", "/lib64",
            "/etc", "/dev", "/proc", "/sys",
            "/boot", "/run",
            "/root",
            "/home",
            "/var", "/var/log", "/var/run", "/var/lib",
            nullptr
        };
        for (int i = 0; BLOCKED[i] != nullptr; ++i) {
            if (resolved == BLOCKED[i]) {
                reason = "path resolves to protected system directory '" + resolved + "'";
                return false;
            }
        }

        if (resolved[0] == '/') {
            int depth = 0;
            for (size_t i = 1; i < resolved.size(); ++i) {
                if (resolved[i] == '/') {
                    ++depth;
                }
            }
            if (depth < 1) {
                reason = "absolute path too shallow (need at least 2 levels): '" + resolved + "'";
                return false;
            }
        }

        return true;
    }

    static bool isSafe(const std::string& path) {
        std::string unused;
        return isSafe(path, unused);
    }
};

} // namespace fw
} // namespace intertwine

#endif // INTERTWINE_CPP_FRAMEWORK_PATHGUARD_HPP
