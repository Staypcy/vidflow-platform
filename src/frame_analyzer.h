#pragma once
#include <string>
#include <vector>

#include"config.h"

struct FrameResult{
    std::string frame;      //帧文件路径 
    std::string analysis;   //模型分析结果
};

int extractFrames(const std::string& videoPath,const std::string&outDir,std::string& err);

std::vector<FrameResult>analyzeFrames(const std::vector<std::string>&frames,const APIConfig& cfg);