#pragma once
#include<string>

struct APIConfig{
    std::string apikey;
    std::string model="deepseek-v4-flash";
    std::string baseUrl="https://api.deepseek.com";
    int maxConcurrent=2;                //最大请求数
    std::string prompt=
        "你是视频内容审核助手。请用一句话描述这个视频帧里发生了什么，"
        "如果画面异常（广告、水印、低俗内容等）请明确指出。";
};