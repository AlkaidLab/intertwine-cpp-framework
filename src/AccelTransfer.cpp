#include "AccelTransfer.hpp"
#include "intertwine/fw/Context.hpp"
#include "intertwine/fw/HttpConstants.hpp"

namespace intertwine {
namespace fw {

static std::string safeFilenameForHeader(const std::string& name) {
    std::string safe;
    safe.reserve(name.size());
    for (size_t i = 0; i < name.size(); ++i) {
        char ch = name[i];
        if (ch >= 0x20 && ch < 0x7F && ch != '"' && ch != '\\') {
            safe += ch;
        } else {
            safe += '_';
        }
    }
    return safe;
}

AccelTransfer::AccelTransfer(const std::string& prefix)
    : m_prefix(prefix) {}

void AccelTransfer::send(Context& c, const TransferParams& params) {
    /* 从 physicalPath 提取文件名（最后一个 '/' 之后的部分） */
    std::string fileId;
    size_t pos = params.physicalPath.rfind('/');
    if (pos != std::string::npos) {
        fileId = params.physicalPath.substr(pos + 1);
    } else {
        fileId = params.physicalPath;
    }
    c.setHeader("X-Accel-Redirect", m_prefix + fileId);
    c.setContentTypeByFilename(params.displayName.c_str());
    std::string disposition = params.inlineDisposition ? "inline" : "attachment";
    c.setHeader("Content-Disposition",
                disposition + "; filename=\"" + safeFilenameForHeader(params.displayName) + "\"");
    c.setStatus(HttpStatus::Ok);
    c.setBody("");

    /* nginx 传输结果未知，假设成功 */
    bool isRange = (params.rangeStart >= 0 && params.rangeEnd >= 0);
    int64_t sendLen = isRange ? (params.rangeEnd - params.rangeStart + 1) : params.fileSize;
    auto startTime = m_stats.recordStart(sendLen);
    m_stats.recordEnd(true, startTime);
    if (params.onComplete) params.onComplete(true);
}

} // namespace fw
} // namespace intertwine
