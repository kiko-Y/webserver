#include "log.h"

using namespace std;

Log::Log() {
    lineCount_ = 0;
    isAsync_ = false;
    writeThread_ = nullptr;
    que_ = nullptr;
    today_ = {0};
    fp_ = nullptr;
}

Log::~Log() {
    // 如果是异步写的话把剩下的没写的全部写入
    if (writeThread_ && writeThread_->joinable()) {
        while (!que_->empty()) que_->flush();
        que_->close();
        writeThread_->join();
    }
    // 全部写完之后关闭 fp
    if (fp_) {
        std::lock_guard<std::mutex> locker(mtx_);
        flush();
        fclose(fp_);
    }
}

void Log::init(LogLevel level, const std::string& path,
              const std::string& suffix, int maxQueueCapacity) {
    isOpen_ = true;
    assert(maxQueueCapacity >= 0);
    if (maxQueueCapacity == 0) {
        isAsync_ = false;
    } else {
        isAsync_ = true;
        que_ = std::make_unique<BlockingQueue<std::string>>(maxQueueCapacity);
        writeThread_ = std::make_unique<std::thread>(flushLogThread);
    }
    time_t timer = time(nullptr);
    tm* sysTime = localtime(&timer);
    tm t = *sysTime;
    path_ = path;
    suffix_ = suffix;
    char fileName[LOG_NAME_LEN] = {0};
    snprintf(fileName, LOG_NAME_LEN - 1, "%s/%04d_%02d_%02d%s",
             path_.c_str(), t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, suffix_.c_str());
    today_ = t;

    {
        std::lock_guard<std::mutex> locker(mtx_);
        lineCount_ = 0;
        level_ = level;
        buff_.retriveAll();
        if (fp_) {
            flush();
            fclose(fp_);
        }
        fp_ = fopen(fileName, "a");
        if (fp_ == nullptr) {
            // path not exist
            mkdir(path_.c_str(), 0777);
            fp_ = fopen(fileName, "a");
        }
        assert(fp_ != nullptr);
    }

}

Log::LogLevel Log::getLevel() {
    std::lock_guard<std::mutex> locker(mtx_);
    return level_;
}

void Log::setLevel(LogLevel level) {
    std::lock_guard<std::mutex> locker(mtx_);
    level_ = level;
}

void Log::write(LogLevel level, const std::string& format, va_list v) {
    timeval now;
    gettimeofday(&now, nullptr);
    time_t curTime = time(nullptr);
    tm *sysTime = localtime(&curTime);
    tm t = *sysTime;
    // 不是同一天了或者写满了
    std::lock_guard<std::mutex> locker(mtx_);
    if (!isSameDay(today_, t) || (lineCount_ && (lineCount_ % MAX_LINES == 0))) {
        char newFileName[LOG_NAME_LEN];
        char tail[36] = {0};
        snprintf(tail, 36, "%04d_%02d_%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
        if (!isSameDay(today_, t)) {
            snprintf(newFileName, LOG_NAME_LEN - 72, "%s/%s%s", path_.c_str(), tail, suffix_.c_str());
            today_ = t;
            lineCount_ = 0;
        } else {
            snprintf(newFileName, LOG_NAME_LEN - 72, "%s/%s-%d%s", path_.c_str(), tail, (lineCount_  / MAX_LINES), suffix_.c_str());
        }
        flush();
        fclose(fp_);
        fp_ = fopen(newFileName, "a");
        assert(fp_ != nullptr);
    }
    lineCount_++;
    int n = snprintf(buff_.beginWrite(), 128, "%d-%02d-%02d %02d:%02d:%02d.%06ld ",
                    t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                    t.tm_hour, t.tm_min, t.tm_sec, now.tv_usec);
    buff_.hasWritten(n);
    appendLogLevelTitle_(level);
    int m = vsnprintf(buff_.beginWrite(), buff_.writableBytes(), format.c_str(), v);
    buff_.hasWritten(m);
    buff_.append("\n\0", 2);
    if (isAsync_ && que_ && !que_->full()) {
        que_->push(buff_.retriveAllToString());
    } else {
        fputs(buff_.peek(), fp_);
    }
    buff_.retriveAll();

}

void Log::appendLogLevelTitle_(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG:
            buff_.append("[debug]: ", 9);
            break;
        case LogLevel::INFO:
            buff_.append("[info]: ", 8);
            break;
        case LogLevel::WARN:
            buff_.append("[warn]: ", 8);
            break;
        case LogLevel::ERROR:
            buff_.append("[error]: ", 9);
            break;
        default:
            buff_.append("[info]: ", 8);
            break;
    }
}

void Log::flush() {
    // if (isAsync_) {
    //     que_->flush();
    // }
    fflush(fp_);
}

void Log::asyncWrite_() {
    string str = "";
    while (que_->pop(str)) {
        std::lock_guard<std::mutex> locker(mtx_);
        fputs(str.c_str(), fp_);
    }
}

Log* Log::Instance() {
    static Log log;
    return &log;
}

void Log::flushLogThread() {
    Log::Instance()->asyncWrite_();
}
