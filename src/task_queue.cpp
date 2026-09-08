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

void TaskQueue::run(Task t){
    fs::path workDir = fs::temp_directory_path() / "vidflow" / t.id;
    std::error_code ec;
    fs::create_directories(workDir, ec);

    std::string err;

    // Step 1: 下载视频
    std::string video = (workDir / "video.mp4").string();
    if (!downloadFile(t.url, video, err)) {
        m_impl->db->updateStatus(t.id, Status::FAILED, "download failed: " + err);
        fs::remove_all(workDir, ec);
        return;
    }

    // Step 2: ffmpeg 抽帧
    std::string framesDir = (workDir / "frames").string();
    if (extractFrames(video, framesDir, err) != 0) {
        m_impl->db->updateStatus(t.id, Status::FAILED, "ffmpeg failed: " + err);
        fs::remove_all(workDir, ec);
        return;
    }

    std::vector<std::string> frames;
    for (const auto& entry : fs::directory_iterator(framesDir))
        if (entry.path().extension() == ".jpg")
            frames.push_back(entry.path().string());
    std::sort(frames.begin(), frames.end());

    if (frames.empty()) {
        m_impl->db->updateStatus(t.id, Status::FAILED, "these are no frames.maybe vedio is bad.");
        fs::remove_all(workDir, ec);
        return;
    }

    // Step 3: 并发调用多模态
    std::vector<FrameResult>results=analyzeFrames(frames,m_impl->cfg);


    json report=json::array();
    for(const auto& r:results){
        report.push_back({
            {"frame", r.frame},
            {"analysis", r.analysis}
        });
    }
    m_impl->db->setReport(t.id,report.dump());

    fs::remove_all(workDir,ec);
}