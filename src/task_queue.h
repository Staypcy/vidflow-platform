#pragma once
#include<memory>
#include<string>
#include"task.h"
#include"config.h"

class DB;

//处理线的唯一入口
class TaskQueue{
public:
    TaskQueue(size_t workers,std::shared_ptr<DB>db,const APIConfig& cfg);

    ~TaskQueue();

    void push(Task t);
private:
    void run(Task t);

    class Impl;
    std::unique_ptr<Impl>m_impl;
};