#include"download.h"
#include<curl/curl.h>
#include<cstdio>

static size_t writeToFile(char*ptr,size_t size,size_t nmemb,void* userdata){
    FILE* f=static_cast<FILE*>(userdata);
    return fwrite(ptr,size,nmemb,f);
}

bool downloadFile(const std::string &url, const std::string &outPath, std::string &err)
{
    FILE* f=fopen(outPath.c_str(),"wb");
    if(!f){
        err="failed to create file: " + outPath;
        return false;
    }

    CURL* curl=curl_easy_init();
    bool ok=false;
    if(curl){
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);   // 跟随 301/302 重定向
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeToFile);  
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, f);               // 回调的 userdata = 文件
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 600L);       
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "vidflow/0.1");  

        CURLcode res = curl_easy_perform(curl);   // 开始下载（阻塞，直到完成或出错）
        if(res==CURLE_OK){
            ok=true;
        }else{
            err=std::string("download failed: ")+ curl_easy_strerror(res);
        }
        curl_easy_cleanup(curl);
    }else{
        err="curl init failed.";
    }

    fclose(f);
    if(!ok){
        std::remove(outPath.c_str());
    }
    return ok;
}
