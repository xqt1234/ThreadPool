#pragma once
#include <thread>
#include <mutex>
#include <unordered_map>
#include <functional>
#include <future>
#include <atomic>
#include <queue>
#include <type_traits>
#include <optional>
#include <condition_variable>
#include <iostream>
/**
 * @brief 线程池思路
 * 创建一些线程，然后从任务队列中取任务，依次执行就行了，
 * 当任务执行完之后，线程空闲，在线程内，线程自己根据空闲超时和空闲线程数量停止线程循环，
 * 把自己的线程id放入空闲线程vector中，唤醒管理线程回收该线程。
 * 线程回收策略，当空闲线程大于工作线程的五分之一的时候，进行回收。
 *
 */


class ThreadPool
{
private:
    std::thread *m_managerThread;
    std::condition_variable m_managerCv;
    std::unordered_map<std::thread::id, std::unique_ptr<std::thread>> m_threadMap;
    std::mutex m_threadmapMtx;
    std::vector<std::thread::id> m_ids;
    std::mutex m_idsMtx;
    std::queue<std::function<void()>> m_taskQue;
    std::mutex m_taskMtx;
    int m_maxTask{0};
    std::condition_variable m_taskCv;
    std::atomic<bool> m_stop{false};
    std::atomic<int> m_idleThread;
    std::atomic<int> m_initThread;
    int m_maxThread;
    std::atomic<int> m_ThreadNum;
    

public:
    ThreadPool(int baseThreadNum,int maxThreadNum,int maxTaskNum);
    ~ThreadPool();
    void addTask(std::function<void()> f);
    template<typename Func,typename... Args>
    auto addTask(Func&& func,Args... args)->std::optional<std::future<std::invoke_result_t<Func,Args...>>>
    {
        using RType = std::invoke_result_t<Func,Args...>;
        auto task = std::make_shared<std::packaged_task<RType()>>(
            std::bind(std::forward<Func>(func), std::forward<Args>(args)...));
        std::future<RType> result = task->get_future();
        {
            std::unique_lock<std::mutex> lock(m_taskMtx);
            if(m_taskQue.size() >= m_maxTask)
            {
                std::cerr << "任务队列已满" << std::endl;
                return std::nullopt;
            }else
            {
                m_taskQue.emplace([task](){
                    (*task)();
                });
                // 任务添加后，如果线程没有及时没有取走，那任务队列就会满
                // 所以每次运行的时候，任务添加的数量可能不一样。
                // 但根据当前任务数量创建线程时，解决了这个问题。
            }
        }
        m_taskCv.notify_one();
        checkthread();
        return result;
    }
private:
    void worker();
    void checkthread();
    void manager();
};