#include "eventmgr.hpp"
#include "threadmgr.hpp"

Executor threadPoolExecutor(ThreadPool& pool)
{
    return [&pool](std::function<void()> fn) {
        pool.submit(std::move(fn));
    };
}
