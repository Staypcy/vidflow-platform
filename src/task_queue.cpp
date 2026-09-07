#include "task_queue.h"
#include "db.h"
#include "download.h"
#include "frame_analyzer.h"
#include "threadpool.h"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <algorithm>
#include <memory>

using json = nlohmann::json;
namespace fs =std::filesystem;

class TaskQueue::Impl{
public:
    ThreadPool pool;
    std::shared_ptr<DB>db;
    APIConfig cfg;
    Impl(size_t n,std::shared_ptr<DB>d,const APIConfig& c)
        :pool(n),db(std::move(d)),cfg(c)
    {}
};

TaskQueue::TaskQueue(size_t workers, std::shared_ptr<DB> db, const APIConfig &cfg)
    :m_impl(std::make_unique<Impl>(workers,std::move(db),cfg))
{}

TaskQueue::~TaskQueue()=default;

void TaskQueue::push(Task t)
{
    Task copy=t;
    m_impl->pool.enqueue([this,copy]{run(copy);});
}