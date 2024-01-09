#include "httpresponse.h"

using namespace std;

const unordered_map<string, string> HttpResponse::SUFFIX_TYPE = {
    { ".html",  "text/html" },
    { ".xml",   "text/xml" },
    { ".xhtml", "application/xhtml+xml" },
    { ".txt",   "text/plain" },
    { ".rtf",   "application/rtf" },
    { ".pdf",   "application/pdf" },
    { ".word",  "application/nsword" },
    { ".png",   "image/png" },
    { ".gif",   "image/gif" },
    { ".jpg",   "image/jpeg" },
    { ".jpeg",  "image/jpeg" },
    { ".au",    "audio/basic" },
    { ".mpeg",  "video/mpeg" },
    { ".mpg",   "video/mpeg" },
    { ".avi",   "video/x-msvideo" },
    { ".gz",    "application/x-gzip" },
    { ".tar",   "application/x-tar" },
    { ".css",   "text/css "},
    { ".js",    "text/javascript "},
};

const unordered_map<int, string> HttpResponse::CODE_STATUS = {
    { 200, "OK" },
    { 400, "Bad Request" },
    { 403, "Forbidden" },
    { 404, "Not Found" },
};

const unordered_map<int, string> HttpResponse::CODE_PATH = {
    { 400, "/400.html" },
    { 403, "/403.html" },
    { 404, "/404.html" },
};

HttpResponse::HttpResponse() {
    code_ = -1;
    path_ = srcDir_ = "";
    isKeepAlive_ = false;
    mmFile_ = nullptr; 
    mmFileStat_ = { 0 };
}

HttpResponse::~HttpResponse() {
    unmapFile();
}

void HttpResponse::init(const std::string &srcDir, const std::string& path, bool isKeepAlive, int code) {
    assert(srcDir != "");
    if (mmFile_) unmapFile();
    code_ = code;
    isKeepAlive_ = isKeepAlive;
    path_ = path;
    srcDir_ = srcDir;
    mmFile_ = nullptr; 
    mmFileStat_ = { 0 };
}

void HttpResponse::makeResponse(Buffer& buff) {
    if (stat((srcDir_ + path_).data(), &mmFileStat_) < 0 || S_ISDIR(mmFileStat_.st_mode)) {
        code_ = 404;
    } else if(!(mmFileStat_.st_mode & S_IROTH)) {
        code_ = 403;
    } else if (code_ == -1) {
        code_ = 200;
    }
    errorHtml_();
    addStateLine_(buff);
    addHeader_(buff);
    addContent_(buff);
}

void HttpResponse::errorHtml_() {
    if (CODE_PATH.count(code_)) {
        path_ = CODE_PATH.at(code_);
        stat((srcDir_ + path_).data(), &mmFileStat_);
    }
}

void HttpResponse::addStateLine_(Buffer &buff) {
    if (!CODE_STATUS.count(code_)) code_ = 400;
    string status = CODE_STATUS.at(code_);
    buff.append("HTTP/1.1 " + to_string(code_) + " " + status + "\r\n");
}

void HttpResponse::addHeader_(Buffer &buff) {
    buff.append("Connection: ");
    if(isKeepAlive_) {
        buff.append("keep-alive\r\n");
        buff.append("keep-alive: max=6, timeout=120\r\n");
    } else{
        buff.append("close\r\n");
    }
    buff.append("Content-type: " + getFileType_() + "\r\n");
}

void HttpResponse::addContent_(Buffer &buff) {
    int srcFd = open((srcDir_ + path_).data(), O_RDONLY);
    if (srcFd < 0) {
        Log_Error("file not found: %s", (srcDir_ + path_).data());
        errorContent(buff, "File NotFound!");
        return;
    }
    Log_Debug("file path %s", (srcDir_ + path_).data());
    // 内存映射
    int *mmRet = (int*)mmap(nullptr, mmFileStat_.st_size, PROT_READ, MAP_PRIVATE, srcFd, 0);
    if (mmRet == MAP_FAILED) {
        errorContent(buff, "File NotFound");
        return;
    }
    mmFile_ = (char*)mmRet;
    close(srcFd);
    buff.append("Content-length: " + to_string(mmFileStat_.st_size) + "\r\n\r\n");
}

void HttpResponse::errorContent(Buffer& buff, std::string message) {
    string body;
    string status;
    body += "<html><title>Error</title>";
    body += "<body bgcolor=\"ffffff\">";
    if(CODE_STATUS.count(code_) == 1) {
        status = CODE_STATUS.find(code_)->second;
    } else {
        status = "Bad Request";
    }
    body += to_string(code_) + " : " + status  + "\n";
    body += "<p>" + message + "</p>";
    body += "<hr><em>TinyWebServer</em></body></html>";

    buff.append("Content-length: " + to_string(body.size()) + "\r\n\r\n");
    buff.append(body);
}


string HttpResponse::getFileType_() const {
    size_t idx = path_.find_last_of('.');
    if (idx != string::npos) {
        string fileExt = path_.substr(idx);
        if (SUFFIX_TYPE.count(fileExt)) {
            return SUFFIX_TYPE.at(fileExt);
        }
    }
    return "text/plain";
}


void HttpResponse::unmapFile() {
    if (mmFile_) {
        munmap(mmFile_, mmFileStat_.st_size);
        mmFile_ = nullptr;
    }
}

char* HttpResponse::file() {
    return mmFile_;
}

size_t HttpResponse::fileLen() const {
    return mmFileStat_.st_size;
}


