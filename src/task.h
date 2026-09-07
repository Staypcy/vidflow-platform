/*
*   task描述
*    id
*    url
*    status:processing / done / failed
*    report:最终报告
*    error
*/

#pragma once 
#include<string>

struct Task
{
    std::string id;     //任务编号
    std::string url;    //视频地址
    std::string status; //processing / done / failed
    std::string report; //最终报告，json格式
    std::string error;  //错误原因
};

//状态常量
namespace Status{
    inline const std::string PROCESSING = "processing";
    inline const std::string DONE= "done";
    inline const std::string FAILED = "failed";
}

