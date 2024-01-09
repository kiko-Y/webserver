#ifndef _HTTPRESPONSE_H_
#define _HTTPRESPONSE_H_

#include <unordered_map>
#include <fcntl.h>       // open
#include <unistd.h>      // close
#include <sys/stat.h>    // stat
#include <sys/mman.h>    // mmap, munmap

#include "../buffer/buffer.h"
#include "../log/log.h"

class HttpResponse {
public:
    HttpResponse();
    ~HttpResponse();

    void init(const std::string &srcDir, const std::string& path, bool isKeepAlive = false, int code = -1);
    void makeResponse(Buffer& buff);
    void unmapFile();
    char* file();
    size_t fileLen() const;
    void errorContent(Buffer& buff, std::string message);
    int code() const { return code_; }


private:
    void addStateLine_(Buffer &buff);
    void addHeader_(Buffer &buff);
    void addContent_(Buffer &buff);

    // 如果 code 是错误码, 将 path_ 改为对应的 html 路径
    void errorHtml_();
    
    //  获取文件对应的 Content-Type
    std::string getFileType_() const;

    int code_;
    bool isKeepAlive_;

    std::string path_;
    std::string srcDir_;

    char* mmFile_;
    struct stat mmFileStat_;

    static const std::unordered_map<std::string, std::string> SUFFIX_TYPE;
    static const std::unordered_map<int, std::string> CODE_STATUS;
    static const std::unordered_map<int, std::string> CODE_PATH;
};

#endif
