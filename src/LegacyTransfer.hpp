// LegacyTransfer — 阻塞式文件传输策略
// 从 FsFileHandlers.cpp handleGetFsDownloadCtx 提取核心传输逻辑。
// send() 在调用线程阻塞执行：小文件全量/serveFile，大文件分块+背压。

#ifndef INTERTWINE_CPP_FRAMEWORK_LEGACY_TRANSFER_HPP
#define INTERTWINE_CPP_FRAMEWORK_LEGACY_TRANSFER_HPP

#include "intertwine/fw/IFileTransfer.hpp"

namespace intertwine {
namespace fw {

class LegacyTransfer : public IFileTransfer {
public:
    void send(Context& c, const TransferParams& params);
    const char* name() const { return "legacy"; }
};

} // namespace fw
} // namespace intertwine

#endif
