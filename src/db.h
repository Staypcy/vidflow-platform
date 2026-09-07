#pragma once
#include<sqlite3.h>
#include<string>
#include "task.h"

//数据库封装
class DB{
public:
    explicit DB(const std::string& path);

    //接受线调用：登记
    void insertTask(const Task& t);
    
    //处理线调用：更新
    void updateStatus(const std::string& id,const std::string& status,const std::string& error);

    //处理线调用：写报告
    void setReport(const std::string& id,const std::string&report);

    //查询线调用
    bool getTask(const std::string& id,Task& out);
private:
    sqlite3* m_db=nullptr;
};