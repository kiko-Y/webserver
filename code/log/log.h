#ifndef _LOG_H_
#define _LOG_H_

#include "../buffer/buffer.h"
#include "blockingqueue.h"
#include <string>
#include <memory>
#include <thread>
#include <mutex>
#include <cstdarg>
#include <ctime>
#include <sys/stat.h>
#include <sys/time.h>

class Log {
public:
    enum LogLevel { DEBUG, INFO, WARN, ERROR };
    // 初始化日志
    void init(LogLevel level = LogLevel::INFO, const std::string& path = "./log",
              const std::string& suffix = ".log", int maxQueueCapacity = 1024);
    // 获取单例
    static Log* Instance();
    static void flushLogThread();

    void write(LogLevel level, const std::string& format, va_list v);
    void flush();

    LogLevel getLevel();
    void setLevel(LogLevel level);
    bool isOpen() { return isOpen_; }

private:

    Log();
    virtual ~Log();
    void appendLogLevelTitle_(LogLevel level);
    void asyncWrite_();
    bool isSameDay(tm ta, tm tb) {
        return ta.tm_year == tb.tm_year 
                && ta.tm_mon == tb.tm_mon 
                && ta.tm_mday == tb.tm_mday;
    }

    static const int LOG_PATH_LEN = 256;
    static const int LOG_NAME_LEN = 256;
    static const int MAX_LINES = 50000;

    std::string path_;
    std::string suffix_;

    int lineCount_;
    tm today_;

    bool isOpen_;

    Buffer buff_;
    LogLevel level_;

    // 是否是异步写日志
    bool isAsync_;
    
    FILE* fp_;
    std::unique_ptr<BlockingQueue<std::string>> que_;
    std::unique_ptr<std::thread> writeThread_;
    std::mutex mtx_;

};

inline void Log_Base(Log::LogLevel level, const std::string& format, va_list v) {
    Log* log = Log::Instance();
    if (log->isOpen() && log->getLevel() <= level) {
        log->write(level, format, v);
        log->flush();
    }
}

inline void Log_Debug(const std::string& format, ...) {
    va_list vaList;
    va_start(vaList, format);
    Log_Base(Log::LogLevel::DEBUG, format, vaList);
    va_end(vaList);
}

inline void Log_Info(const std::string& format, ...) {
    va_list vaList;
    va_start(vaList, format);
    Log_Base(Log::LogLevel::INFO, format, vaList);
    va_end(vaList);
}

inline void Log_Warn(const std::string& format, ...) {
    va_list vaList;
    va_start(vaList, format);
    Log_Base(Log::LogLevel::WARN, format, vaList);
    va_end(vaList);
}

inline void Log_Error(const std::string& format, ...) {
    va_list vaList;
    va_start(vaList, format);
    Log_Base(Log::LogLevel::ERROR, format, vaList);
    va_end(vaList);
}

#endif
