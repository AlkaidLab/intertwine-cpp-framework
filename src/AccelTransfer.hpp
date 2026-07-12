// AccelTransfer — X-Accel-Redirect 传输策略
// 设置 nginx X-Accel-Redirect 头后立即返回，由反代完成文件传输。

#ifndef INTERTWINE_CPP_FRAMEWORK_ACCEL_TRANSFER_HPP
#define INTERTWINE_CPP_FRAMEWORK_ACCEL_TRANSFER_HPP

#include "intertwine/fw/IFileTransfer.hpp"
#include <string>

namespace intertwine {
namespace fw {

class AccelTransfer : public IFileTransfer {
public:
    explicit AccelTransfer(const std::string& prefix);
    void send(Context& c, const TransferParams& params);
    const char* name() const { return "accel"; }

private:
    std::string m_prefix;  // e.g. "/internal/files/"
};

} // namespace fw
} // namespace intertwine

#endif
