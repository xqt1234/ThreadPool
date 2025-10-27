#include "threadpool.h"
#include <chrono>

ThreadPool::ThreadPool(int baseThreadNum, int maxThreadNum, int maxTaskNum)
    : m_initThread(baseThreadNum), m_maxThread(maxThreadNum), m_maxTask(maxTaskNum), m_idleThread(baseThreadNum), m_ThreadNum(baseThreadNum)
{
    std::cout << "初始线程数量:" << m_initThread
              << "最大线程数量:" << m_maxThread
              << "最大任务数量:" << m_maxTask << std::endl;
    m_managerThread = new std::thread(&ThreadPool::manager, this);
    for (int i = 0; i < m_initThread; ++i)
    {
        auto newThread = std::make_unique<std::thread>(&ThreadPool::worker, this);
        std::cout << "初始创建新线程" << newThread->get_id() << std::endl;
        m_threadMap[newThread->get_id()] = std::move(newThread);
    }
}

ThreadPool::~ThreadPool()
{
    m_stop = true;
    m_taskCv.notify_all();
    std::lock_guard<std::mutex> lock(m_threadmapMtx);
    for (auto &[id, threadptr] : m_threadMap)
    {
        if (threadptr && threadptr->joinable())
        {
            threadptr->join();
            std::cout << "线程析构" << id << "退出" << std::endl;
        }
    }
    m_managerCv.notify_one();
    if (m_managerThread->joinable())
    {
        m_managerThread->join();
    }
    delete m_managerThread;
}

void ThreadPool::addTask(std::function<void()> f)
{
    {
        std::unique_lock<std::mutex> lock(m_taskMtx);
        if (m_taskQue.size() >= m_maxTask)
        {
            std::cerr << "任务队列已满" << std::endl;
            return;
        }
        else
        {
            m_taskQue.emplace(f);
            std::cout << "添加任务成功" << std::endl;
        }
    }
    m_taskCv.notify_one();
    checkthread();
}

void ThreadPool::worker()
{
    while (!m_stop.load())
    {
        std::function<void()> task = nullptr;
        {
            std::unique_lock<std::mutex> lock(m_taskMtx);
            if (m_taskCv.wait_for(lock, std::chrono::seconds(60), [&]()
                              { return !m_taskQue.empty() || m_stop; }))
            {
                if (m_stop)
                {
                    break;
                }
                if (!m_taskQue.empty())
                {
                    task = std::move(m_taskQue.front());
                    m_taskQue.pop();
                    m_idleThread--;
                    std::cout << "取走一个任务" << std::endl;
                }
            }
        }
        if (task)
        {
            task();
            m_idleThread++;
        }
        else
        {
            int idle = m_idleThread;
            int workthread = m_ThreadNum - idle;
            if (workthread < m_initThread)
            {
                idle = idle - (m_initThread - workthread);
                workthread = m_initThread;
            }
            if (idle * 1.0 / workthread > 0.2)
            {
                {
                    std::lock_guard<std::mutex> lock(m_idsMtx);
                    m_ids.push_back(std::this_thread::get_id());
                    m_idleThread--;
                    m_ThreadNum--;
                    std::cout << "线程空闲过多" << std::this_thread::get_id() << "退出" << std::endl;
                    return;
                }
            }
        }
    }
}

void ThreadPool::checkthread()
{
    // 这里容易出问题，一下子添加100个，但是，线程还没有从任务队列中取数据，
    //  所以就会出现有空闲线程，但是，任务却堆积在队列中的情况。
    // 这里改为根据任务数量新建线程。
    std::cout << m_idleThread << " " << m_ThreadNum << " " << m_maxThread << std::endl;
    int tasknum = 0;
    if (m_ThreadNum < m_maxThread)
    {
        std::lock_guard<std::mutex> lock(m_taskMtx);
        tasknum = m_taskQue.size() - m_idleThread;
        int canCreateNum = m_maxThread - m_ThreadNum;
        tasknum = tasknum > canCreateNum ? canCreateNum : tasknum;
    }
    if (tasknum > 0)
    {
        std::lock_guard<std::mutex> lock(m_threadmapMtx);
        for (int i = 0; i < 1; ++i)
        {
            auto newThread = std::make_unique<std::thread>(&ThreadPool::worker, this);
            std::cout << "动态创建新线程" << newThread->get_id() << std::endl;
            m_threadMap[newThread->get_id()] = std::move(newThread);
            m_ThreadNum++;
            m_idleThread++;
        }
    }
}

void ThreadPool::manager()
{
    while (!m_stop.load())
    {
        std::unique_lock<std::mutex> lock(m_idsMtx);
        m_managerCv.wait_for(lock, std::chrono::seconds(2), [&]()
                             { return m_ids.size() > 0 || m_stop.load(); });
        if (m_ids.size() > 0)
        {
            for (auto &val : m_ids)
            {
                auto it = m_threadMap.find(val);
                if (it != m_threadMap.end())
                {
                    it->second->join();
                    m_threadMap.erase(it);
                }
            }
        }
    }
}
