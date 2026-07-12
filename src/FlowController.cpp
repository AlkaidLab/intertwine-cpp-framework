#include "intertwine/fw/FlowController.hpp"

namespace intertwine {
namespace fw {

FlowController::FlowController(size_t maxQueueSize, bool dropOnFull)
    : m_maxQueueSize(maxQueueSize)
    , m_dropOnFull(dropOnFull)
    , m_dropCount(0)
    , m_totalPushAttempts(0)
    , m_totalDrops(0)
{
}

FlowController::~FlowController() {
}

bool FlowController::canPush(size_t currentSize) const {
    return currentSize < m_maxQueueSize;
}

bool FlowController::tryPush(size_t currentSize) {
    m_totalPushAttempts.fetch_add(1, boost::memory_order_relaxed);

    if (canPush(currentSize)) {
        return true;
    }

    if (m_dropOnFull) {
        recordDrop();
        return false;
    } else {
        return false;
    }
}

void FlowController::recordDrop() {
    m_dropCount.fetch_add(1, boost::memory_order_relaxed);
    m_totalDrops.fetch_add(1, boost::memory_order_relaxed);
}

uint64_t FlowController::getDropCount() const {
    return m_dropCount.load(boost::memory_order_relaxed);
}

void FlowController::resetDropCount() {
    m_dropCount.store(0, boost::memory_order_relaxed);
}

size_t FlowController::getMaxQueueSize() const {
    return m_maxQueueSize;
}

void FlowController::setMaxQueueSize(size_t maxSize) {
    m_maxQueueSize = maxSize;
}

bool FlowController::isDropOnFull() const {
    return m_dropOnFull;
}

void FlowController::setDropOnFull(bool dropOnFull) {
    m_dropOnFull = dropOnFull;
}

} // namespace fw
} // namespace intertwine
