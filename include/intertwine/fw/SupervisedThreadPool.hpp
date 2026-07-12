#ifndef FW_SUPERVISED_THREAD_POOL_HPP
#define FW_SUPERVISED_THREAD_POOL_HPP

#include <cstddef>
#include <boost/function.hpp>
#include <vector>
#include <queue>
#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/atomic.hpp>

namespace intertwine {
namespace fw {

/**
 * 受监督线程池：worker 从任务队列中持续取任务执行；异常退出时 supervisor 自动拉起新线程替代。
 * 框架内通过 getInstance() 获取应用级单例，main 启动时 start()、退出时 stop()；各模块 submit(task) 投递异步任务。
 * 任务队列可设上限（maxQueueSize），防止无界增长；满时 submit 返回 false，调用方可选择丢弃或重试。
 */
class SupervisedThreadPool {
public:
    using Task = boost::function<void()>;

    /** 应用级单例，供框架内各模块提交任务 */
    static SupervisedThreadPool& getInstance();

    SupervisedThreadPool();
    ~SupervisedThreadPool();

    /** 启动 numWorkers 个 worker 和 1 个 supervisor；重复调用先 stop 再 start */
    void start(std::size_t numWorkers);

    /** 任务队列上限，0 表示不限制；需在 start 前设置生效 */
    void setMaxQueueSize(std::size_t maxSize) { m_maxQueueSize = maxSize; }
    std::size_t maxQueueSize() const { return m_maxQueueSize; }

    /**
     * 提交任务（非阻塞）。worker 会持续从队列取任务执行。
     * @return true 入队成功；false 池未启动、或队列已满（maxQueueSize>0 且 size>=max）
     */
    bool submit(Task task);

    /** 停止：置位 running=false，唤醒所有 worker 与 supervisor，并 join 所有线程 */
    void stop();

    /** 当前 worker 数量（配置值，非实时存活数） */
    std::size_t workerCount() const { return m_numWorkers; }

    /** 当前任务队列深度 */
    std::size_t queueSize() const;

    /** 是否已调用 start 且未 stop */
    bool isRunning() const { return m_running.load(boost::memory_order_relaxed); }

    SupervisedThreadPool(const SupervisedThreadPool&) = delete;
    SupervisedThreadPool& operator=(const SupervisedThreadPool&) = delete;

private:
    void workerLoop(std::size_t workerId);
    void supervisorLoop();
    void pushFinished(std::size_t workerId);

    std::size_t m_numWorkers;
    std::size_t m_maxQueueSize;
    boost::atomic<bool> m_running;
    std::vector<boost::thread> m_workers;
    boost::thread m_supervisor;

    std::queue<Task> m_tasks;
    mutable boost::mutex m_tasksMutex;
    boost::condition_variable m_tasksCond;

    std::queue<std::size_t> m_finishedIds;
    mutable boost::mutex m_finishedMutex;
    boost::condition_variable m_finishedCond;
};

} // namespace fw
} // namespace intertwine

#endif // FW_SUPERVISED_THREAD_POOL_HPP
