#include "threadpool.h"
#include <chrono>

// std::atomic<int> m_idleThread;
//     std::atomic<int> m_maxThread;
//     std::atomic<int> m_currentThread;
ThreadPool::ThreadPool(int baseThreadNum, int maxThreadNum, int maxTaskNum)
    :m_idleThread(baseThreadNum),m_maxThread(maxThreadNum),m_maxTask(maxTaskNum)
{
    m_managerThread = new std::thread(&ThreadPool::manager,this);
}

ThreadPool::~ThreadPool()
{
    m_stop = true;
    m_cv.notify_all();
    for(auto& [id,threadptr] : m_threadMap)
    {
        if(threadptr && threadptr->joinable())
        {
            
            threadptr->join();
            std::cout << "线程" << id << "退出" << std::endl;
        }
    }
    m_idscv.notify_one();
    if(m_managerThread->joinable())
    {
        m_managerThread->join();
    }
    delete m_managerThread;

}

void ThreadPool::addTask(std::function<void()> f)
{
    {
        std::unique_lock<std::mutex> lock(m_taskMtx);
        if (!m_taskQue.size() > m_maxTask)
        {
            std::cerr << "任务队列已满" << std::endl;
            return;
        }
        else
        {
            m_taskQue.emplace(f);
        }
    }
    m_cv.notify_one();
}

void ThreadPool::manager()
{
    while (!m_stop.load())
    {
        std::vector<std::thread::id> idsToRemove;
        {
            std::unique_lock<std::mutex> lock(m_idsMtx);
            m_idscv.wait_for(lock, std::chrono::seconds(60), [&]()
                             { return !m_ids.empty() || m_stop.load(); });

            if (!m_ids.empty())
            {
                m_ids.swap(idsToRemove);
            }
        }
        if (!idsToRemove.empty())
        {
            std::lock_guard<std::mutex> lck(m_threadmapMtx);
            for (auto &val : idsToRemove)
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

void ThreadPool::worker()
{
    while(!m_stop.load())
    {
        std::function<void()> task = nullptr;
        {
            std::unique_lock<std::mutex> lock(m_taskMtx);
            if(m_cv.wait_for(lock,std::chrono::seconds(60),[&](){
                return !m_taskQue.empty() || m_stop;
            }))
            {
                //有任务或者停止了
                if(m_stop)
                {
                    break;
                }

            }else
            {
                //超时
                
            }
            
        }

    }
}
