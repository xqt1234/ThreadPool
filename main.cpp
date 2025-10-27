#include <iostream>
#include "threadpool.h"

int add(int a,int b)
{
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "任务执行完毕" << std::endl;
    return a + b;
}

void addnum(int a ,int b)
{
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "任务执行完毕" << std::endl;
}
int main()
{
    ThreadPool pool(2,5,10);
    for(int i = 0;i < 2;++i)
    {
        pool.addTask(addnum,1,2);
    }
    
    std::cout << "等待2秒执行" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(5));
    std::cout << "2秒结束准备关闭程序" << std::endl;
    auto res1 = pool.addTask(add,80,3);
    if(res1)
    {
        std::cout << "输出的值是" << res1->get() << std::endl;
    }else
    {
        std::cout << "没有执行" << std::endl;
    }
    std::cout << "程序都结束了，还没有执行吗？" << std::endl;
    std::cout << "睡个5秒"<< std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(5));
    return 0;
}