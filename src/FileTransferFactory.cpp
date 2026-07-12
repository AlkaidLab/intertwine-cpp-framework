#include "intertwine/fw/IFileTransfer.hpp"
#include "LegacyTransfer.hpp"
#include "AccelTransfer.hpp"
#include "StreamTransfer.hpp"
#include <boost/algorithm/string.hpp>

namespace intertwine {
namespace fw {

std::unique_ptr<IFileTransfer> FileTransferFactory::create(
    const std::string& mode,
    const std::string& accelPrefix) {
    std::string lower = boost::to_lower_copy(mode);

    if (lower == "legacy") {
        return std::unique_ptr<IFileTransfer>(new LegacyTransfer());
    }
    if (lower == "accel") {
        return std::unique_ptr<IFileTransfer>(new AccelTransfer(accelPrefix));
    }
    // "stream" / "auto" / default: 事件驱动 onwrite 状态机，零线程占用
    if (lower == "stream" || lower == "auto" || lower.empty()) {
        return std::unique_ptr<IFileTransfer>(new StreamTransfer());
    }

    return nullptr;
}

} // namespace fw
} // namespace intertwine
