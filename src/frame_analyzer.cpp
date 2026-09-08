#include "frame_analyzer.h"
#include"config.h"

#include<nlohmann/json.hpp>
#include<curl/curl.h>

#include<fstream>
#include<filesystem>
#include<vector>
#include<string>
#include<system_error>
#include<chrono>
#include<mutex>
#include<thread>
#include<semaphore>
#include<algorithm>
#include<future>
#include<atomic>

using json = nlohmann::json;

#ifdef _WIN32
#include <windows.h>

class WinProcess {
public:
    WinProcess() : m_pi{} {}
    ~WinProcess() {
        if (m_pi.hProcess) CloseHandle(m_pi.hProcess);
        if (m_pi.hThread)  CloseHandle(m_pi.hThread);
    }
    PROCESS_INFORMATION* operator&() { return &m_pi; }
    DWORD waitAndGetExitCode() {
        WaitForSingleObject(m_pi.hProcess, INFINITE);
        DWORD code = 0;
        GetExitCodeProcess(m_pi.hProcess, &code);
        return code;
    }
private:
    PROCESS_INFORMATION m_pi{};
};

static bool runProcess(const std::string& cmd, std::string& err) {
    STARTUPINFOA si{};
    si.cb = sizeof(si);
    std::vector<char> cmdline(cmd.begin(), cmd.end());
    cmdline.push_back('\0');

    WinProcess proc;
    if (!CreateProcessA(nullptr, cmdline.data(), nullptr, nullptr, FALSE,
                        CREATE_NO_WINDOW, nullptr, nullptr, &si, &proc)) {
        err = "启动进程失败: " + cmd;
        return false;
    }
    DWORD code = proc.waitAndGetExitCode();
    if (code != 0) {
        err = "命令退出码 " + std::to_string(code) + ": " + cmd;
        return false;
    }
    return true;
}
#else
#include <cstdlib>
static bool runProcess(const std::string& cmd, std::string& err) {
    int ret = std::system(cmd.c_str());
    if (ret != 0) {
        err = "命令失败 (code " + std::to_string(ret) + "): " + cmd;
        return false;
    }
    return true;
}
#endif

static std::string base64Encode(const unsigned char* data,size_t len){
    static const char table[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string out;
    out.reserve((len + 2) / 3 * 4);

    for(size_t i=0;i<len;i+=3){
        unsigned int n=static_cast<unsigned int>(data[i])<<16;
        if (i + 1 < len) n |= static_cast<unsigned int>(data[i + 1]) << 8;
        if (i + 2 < len) n |= static_cast<unsigned int>(data[i + 2]);
        out.push_back(table[(n >> 18) & 0x3F]);
        out.push_back(table[(n >> 12) & 0x3F]);
        out.push_back(i + 1 < len ? table[(n >> 6) & 0x3F] : '=');
        out.push_back(i + 2 < len ? table[n & 0x3F] : '=');
    }
    return out;
}

static std::string base64EncodeFile(const std::string& path){
    std::ifstream f(path,std::ios::binary);
    if(!f)return "";
    
    f.seekg(0,std::ios::end);
    auto size=f.tellg();
    f.seekg(0,std::ios::beg);

    std::string buffer(static_cast<size_t>(size),'\0');
    if(!f.read(buffer.data(),buffer.size()))return "";
    
    return base64Encode(reinterpret_cast<const unsigned char*>(buffer.data()),buffer.size());
}


struct CurlDeleter{
    void operator()(CURL* c)const{if(c) curl_easy_cleanup(c);}
};

using CurlPtr =std::unique_ptr<CURL,CurlDeleter>;

struct CurlSlistDeleter {
    void operator()(curl_slist* s) const { if (s) curl_slist_free_all(s); }
};

using CurlSlistPtr = std::unique_ptr<curl_slist, CurlSlistDeleter>;


static size_t writeToString(char* ptr,size_t size,size_t nmemb,void* userdata){
    auto* s=static_cast<std::string*>(userdata);
    s->append(static_cast<char*>(ptr),size*nmemb);
    return size* nmemb;
}

static long httpPostJson(const std::string& url , const std::string& body , const std::string& apikey,std::string& response){
    CurlPtr curl(curl_easy_init());
    if (!curl) return -1;

    CurlSlistPtr headers(nullptr);

    headers.reset(curl_slist_append(headers.get(), "Content-Type: application/json"));
    headers.reset(curl_slist_append(headers.get(), ("Authorization: Bearer " + apikey).c_str()));

    curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, headers.get());
    curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
    curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, writeToString);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT, 300L);

    CURLcode res = curl_easy_perform(curl.get());

    long code = 0;
    if (res == CURLE_OK) {
        curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &code);
    } else {
        code = -res;
        response = "CURL error: " + std::to_string(res);
    }
    return code;
}

static std::string analyzeOneFrame(const std::string& path,const APIConfig& cfg){
    std::string b64 = base64EncodeFile(path);
    if (b64.empty()) {
        return "Error: failed to encode image";
    }

    json payload = {
        {"model", cfg.model},
        {"messages", {
            {
                {"role", "user"},
                {"content", {
                    {
                        {"type", "image_url"},
                        {"image_url", {
                            {"url", "data:image/jpeg;base64," + b64}
                        }}
                    },
                    {
                        {"type", "text"},
                        {"text", cfg.prompt}
                    }
                }}
            }
        }},
        {"max_tokens", 1024}
    };

    std::string response;
    long status = httpPostJson(cfg.baseUrl+"/v1/chat/completions",payload.dump(),cfg.apikey , response);

    if(status!=200){
        return "HTTP error: "+ std::to_string(status)+": "+response;
    }

    try{
        auto respJson=json::parse(response);

        if (respJson.contains("choices") && respJson["choices"].is_array() &&
            !respJson["choices"].empty()) {
            auto& choice = respJson["choices"][0];
            if (choice.contains("message") && choice["message"].contains("content")) {
                return choice["message"]["content"].get<std::string>();
            }
        }
        return "No content in response";
    }catch(const json::parse_error& e){
        return "JSON parse error: "+std::string(e.what());
    }
}

int extractFrames(const std::string &videoPath, const std::string &outDir,std::string &err)
{
    std::error_code ec;
    std::filesystem::create_directories(outDir,ec);
    if(ec){
        err="创建目录失败: "+ ec.message();
        return -1;
    }

    std::string cmd=
    "ffmpeg -y -nostdin -loglevel error -i \"" + videoPath + "\" "
        "-vf \"fps=1,scale='min(768,iw)':-2\" -q:v 3 \"" + outDir + "/frame_%05d.jpg\"";
    
    if(!runProcess(cmd,err)){
        return -1;
    }

    int count=0;
    try {
        for (const auto& entry : std::filesystem::directory_iterator(outDir)) {
            if (entry.path().extension() == ".jpg") ++count;
        }
    } catch (const std::filesystem::filesystem_error& e) {
        err = "遍历目录失败: " + std::string(e.what());
        return -1;
    }
    return count;
}

std::vector<FrameResult> analyzeFrames(const std::vector<std::string> &frames, const APIConfig &cfg)
{
    std::vector<FrameResult>results(frames.size());
    
    std::vector<std::future<void>> futures;
    std::atomic<size_t>nextIndex{0};
    const size_t total =frames.size();

    for(int i=0;i<cfg.maxConcurrent&& i<static_cast<int>(total);i++){
        futures.emplace_back(std::async(std::launch::async,[&](){
            while(true){
                size_t idx=nextIndex.fetch_add(1);
                if(idx>=total)break;

                results[idx].frame=frames[idx];
                results[idx].analysis=analyzeOneFrame(frames[idx],cfg);
            }
        }));
    }

    for(auto& f: futures){
        if(f.valid())f.wait();
    }

    return results;
}
