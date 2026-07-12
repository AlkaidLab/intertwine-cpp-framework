#include "intertwine/fw/SupervisedThreadPool.hpp"
#include "intertwine/fw/Logger.hpp"

namespace intertwine {
namespace fw {

#define LOG_POOL_INFO(msg)  LOG_INFO("[pool] " msg)
#define LOG_POOL_WARN(msg)  LOG_WARN("[pool] " msg)

SupervisedThreadPool& SupervisedThreadPool::getInstance() {
    static SupervisedThreadPool s_instance;
    return s_instance;
}

SupervisedThreadPool::SupervisedThreadPool()
    : m_numWorkers(0), m_maxQueueSize(0), m_running(false) {}

SupervisedThreadPool::~SupervisedThreadPool() {
    try {
        stop();
    } catch (...) {}
}

void SupervisedThreadPool::start(std::size_t numWorkers) {
    if (numWorkers == 0) {
        return;
    }
    stop();
    m_numWorkers = numWorkers;
    m_running.store(true, boost::memory_order_release);
    m_workers.clear();
    m_workers.reserve(numWorkers);
    for (std::size_t i = 0; i < numWorkers; ++i) {
        m_workers.push_back(boost::thread(&SupervisedThreadPool::workerLoop, this, i));
    }
    m_supervisor = boost::thread(&SupervisedThreadPool::supervisorLoop, this);
    intertwine::Logger::getInstance().info("[pool] started: " + std::to_string(numWorkers) + " workers + 1 supervisor thread");
}

bool SupervisedThreadPool::submit(Task task) {
    if (!task || !m_running.load(boost::memory_order_acquire)) {
        return false;
    }
    {
        boost::mutex::scoped_lock lock(m_tasksMutex);
        if (m_maxQueueSize > 0 && m_tasks.size() >= m_maxQueueSize) {
            return false;
        }
        m_tasks.push(std::move(task));
    }
    m_tasksCond.notify_one();
    return true;
}

std::size_t SupervisedThreadPool::queueSize() const {
    boost::mutex::scoped_lock lock(m_tasksMutex);
    return m_tasks.size();
}

void SupervisedThreadPool::stop() {
    if (!m_running.load(boost::memory_order_acquire)) {
        return;
    }
    m_running.store(false, boost::memory_order_release);
    m_tasksCond.notify_all();
    m_finishedCond.notify_all();
    LOG_POOL_INFO("stopping workers and supervisor");
    for (auto& w : m_workers) {
        if (w.joinable()) {
            w.join();
        }
    }
    if (m_supervisor.joinable()) {
        m_supervisor.join();
    }
}

void SupervisedThreadPool::pushFinished(std::size_t workerId) {
    boost::mutex::scoped_lock lock(m_finishedMutex);
    m_finishedIds.push(workerId);
    m_finishedCond.notify_one();
}

void SupervisedThreadPool::workerLoop(std::size_t workerId) {
    struct ExitNotify {
        SupervisedThreadPool* pool;
        std::size_t id;
        ~ExitNotify() {
            try {
                if (pool) {
                    pool->pushFinished(id);
                }
            } catch (...) {}
        }
    } guard = { this, workerId };
    (void)guard;

    while (m_running.load(boost::memory_order_acquire)) {
        Task task;
        {
            boost::mutex::scoped_lock lock(m_tasksMutex);
            while (m_tasks.empty() && m_running.load(boost::memory_order_acquire)) {
                m_tasksCond.wait(lock);
            }
            if (!m_running.load(boost::memory_order_acquire)) {
                break;
            }
            if (m_tasks.empty()) {
                continue;
            }
            task = std::move(m_tasks.front());
            m_tasks.pop();
        }
        if (task) {
            try {
                task();
            } catch (const std::exception& e) {
                intertwine::Logger::getInstance().error("[pool] worker " + std::to_string(workerId) + " task exception: " + e.what());
            } catch (...) {
                intertwine::Logger::getInstance().error("[pool] worker " + std::to_string(workerId) + " task unknown exception");
            }
        }
    }
}

void SupervisedThreadPool::supervisorLoop() {
    while (m_running.load(boost::memory_order_acquire)) {
        std::size_t id = 0;
        bool have = false;
        {
            boost::mutex::scoped_lock lock(m_finishedMutex);
            while (m_finishedIds.empty() && m_running.load(boost::memory_order_acquire)) {
                m_finishedCond.wait(lock);
            }
            if (!m_running.load(boost::memory_order_acquire)) {
                break;
            }
            if (!m_finishedIds.empty()) {
                id = m_finishedIds.front();
                m_finishedIds.pop();
                have = true;
            }
        }
        if (!have) {
            continue;
        }
        if (id >= m_workers.size()) {
            continue;
        }
        if (m_workers[id].joinable()) {
            m_workers[id].join();
        }
        if (!m_running.load(boost::memory_order_acquire)) {
            break;
        }
        m_workers[id] = boost::thread(&SupervisedThreadPool::workerLoop, this, id);
        intertwine::Logger::getInstance().info("[pool] restarted worker " + std::to_string(id));
    }
}

} // namespace fw
} // namespace intertwine
