#ifndef MC_CORE_PRIORITY_QUEUE_H
#define MC_CORE_PRIORITY_QUEUE_H

#include <atomic>
#include <boost/asio.hpp>
#include <boost/asio/post.hpp>
#include <condition_variable>
#include <deque>
#include <functional>
#include <iostream> // 添加iostream头文件以使用std::cerr
#include <limits>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <tuple>
#include <vector>

namespace mc {

/**
 * @brief 任务优先级常量
 */
struct priority {
    static constexpr int lowest = std::numeric_limits<int>::min();
    static constexpr int low = -1000;
    static constexpr int normal_low = 200;
    static constexpr int normal = 300;
    static constexpr int normal_high = 400;
    static constexpr int high = 1000;
    static constexpr int highest = std::numeric_limits<int>::max();
};

template <typename ContextType = boost::asio::io_context>
class priority_queue_executor {
    class handler_base {
    public:
        handler_base(int p, size_t order) : m_priority(p), m_order(order) {
        }

        virtual ~handler_base() = default;
        virtual void execute() = 0;

        friend bool operator<(const handler_base& a, const handler_base& b) noexcept {
            return std::tie(a.m_priority, a.m_order) < std::tie(b.m_priority, b.m_order);
        }

    private:
        int m_priority;
        size_t m_order;
    };

    template <typename Function>
    class handler : public handler_base {
    public:
        handler(int p, size_t order, Function f)
            : handler_base(p, order), m_function(std::move(f)) {
        }

        void execute() override {
            m_function();
        }

    private:
        Function m_function;
    };

    struct handler_less {
        template <typename Pointer>
        bool operator()(const Pointer& a, const Pointer& b) const noexcept {
            return *a < *b;
        }
    };

    using handler_ptr = std::unique_ptr<handler_base>;
    using handler_queue = std::priority_queue<handler_ptr, std::deque<handler_ptr>, handler_less>;

public:
    using context_type = ContextType;
    using size_type = std::size_t;

    enum class QueueState {
        Stopped,   // 停止状态：接受任务但不执行
        Running,   // 运行状态：接受任务并执行
        Paused,    // 暂停状态：接受任务但暂停执行
        Processing // 正在处理状态：有任务正在执行
    };

    explicit priority_queue_executor(context_type& context, bool auto_start = true)
        : m_context(context), m_order(std::numeric_limits<size_type>::max()),
          m_is_task_scheduled(false),
          m_state(auto_start ? QueueState::Running : QueueState::Stopped) {
    }

    ~priority_queue_executor() {
        execute_all();
    }

    // 启动队列处理
    void start() {
        std::lock_guard<std::mutex> lock(m_state_mutex);
        if (m_state != QueueState::Running) {
            m_state = QueueState::Running;
            try_start_processing();
        }
    }

    // 停止队列处理
    void stop() {
        std::lock_guard<std::mutex> lock(m_state_mutex);
        m_state = QueueState::Stopped;
    }

    // 暂停队列处理
    void pause() {
        std::lock_guard<std::mutex> lock(m_state_mutex);
        if (m_state == QueueState::Running) {
            m_state = QueueState::Paused;
        }
    }

    // 恢复队列处理
    void resume() {
        std::lock_guard<std::mutex> lock(m_state_mutex);
        if (m_state == QueueState::Paused) {
            m_state = QueueState::Running;
            try_start_processing();
        }
    }

    template <typename Function>
    void execute(Function&& f, int p = priority::normal) {
        add(p, std::forward<Function>(f));
    }

    template <typename Function>
    void add(int p, Function f) {
        auto h = std::make_unique<handler<Function>>(p, --m_order, std::move(f));

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_task_queue.push(std::move(h));
        }

        std::lock_guard<std::mutex> lock(m_state_mutex);
        try_start_processing();
    }

    void execute_all() {
        while (execute_highest()) {
        }
    }

    bool execute_highest() {
        handler_ptr task;
        bool has_more = get_next_task(task);
        while (!task && has_more) {
            has_more = get_next_task(task);
        }

        execute_task(std::move(task));
        return has_more;
    }

    std::size_t size() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_task_queue.size();
    }

    template <typename Function>
    boost::asio::executor_binder<Function, priority_queue_executor> wrap(int p,
                                                                         Function&& func) const {
        return boost::asio::bind_executor(*this, std::forward<Function>(func));
    }

    friend bool operator==(const priority_queue_executor& a,
                           const priority_queue_executor& b) noexcept {
        return &a.m_context == &b.m_context;
    }

    friend bool operator!=(const priority_queue_executor& a,
                           const priority_queue_executor& b) noexcept {
        return !(a == b);
    }

private:
    void try_start_processing() {
        if (m_state != QueueState::Running) {
            return; // 非运行状态不启动处理
        }

        if (!m_task_queue.empty() && !m_is_task_scheduled) {
            m_is_task_scheduled = true;
            boost::asio::post(m_context, [this]() {
                process_next_task();
            });
        }
    }

    // 记录错误日志
    void log_error(const std::string& message, const std::exception& e) {
        std::cerr << message << ": " << e.what() << std::endl;
    }

    void log_error(const std::string& message) {
        std::cerr << message << std::endl;
    }

    void process_next_task() {
        std::lock_guard<std::mutex> lock(m_state_mutex);
        if (m_state != QueueState::Running) {
            m_is_task_scheduled = false;
            return;
        }

        // 执行任务并检查是否继续
        if (execute_highest()) {
            boost::asio::post(m_context, [this]() {
                process_next_task();
            });
        } else {
            m_is_task_scheduled = false;
        }
    }

    // 从任务队列中获取下一个任务
    bool get_next_task(handler_ptr& task) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_task_queue.empty()) {
            return false;
        }

        task = std::move(const_cast<handler_ptr&>(m_task_queue.top()));
        m_task_queue.pop();
        return !m_task_queue.empty();
    }

    // 执行任务并处理异常
    void execute_task(handler_ptr task) {
        if (!task) {
            return;
        }

        try {
            task->execute();
        } catch (const std::exception& e) {
            log_error("Task execution error", e);
        } catch (...) {
            log_error("Task execution error: Unknown exception");
        }
    }

    context_type& m_context;
    std::mutex m_mutex;
    handler_queue m_task_queue;
    std::size_t m_order;

    std::mutex m_state_mutex;
    bool m_is_task_scheduled;
    QueueState m_state;
};

} // namespace mc

namespace boost {
namespace asio {

template <typename ContextType>
struct is_executor<mc::priority_queue_executor<ContextType>> : std::true_type {};

} // namespace asio
} // namespace boost

#endif // MC_CORE_PRIORITY_QUEUE_H