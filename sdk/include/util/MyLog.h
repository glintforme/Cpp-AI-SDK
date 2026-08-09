#pragma once
#include <spdlog/spdlog.h>
#include <spdlog/logger.h>
#include <mutex>

namespace ai_chat_sdk{
    
class Logger{
public:
    //初始化日志记录器
    //logger_name: 日志记录器名称
    //logger_file: 日志文件路径
    //logger_level: 日志级别
    static void init_logger(const std::string &logger_name,const std::string &logger_file,spdlog::level::level_enum logger_level);
    static std::shared_ptr<spdlog::logger> getLogger(); //获取日志记录器
private:
    //将日志的构造、重载私有
    Logger();   //默认构造函数
    Logger(const Logger &) = delete;    //删除复制构造函数
    Logger &operator=(const Logger &) = delete; //删除赋值运算符
private:
    static std::shared_ptr<spdlog::logger> _logger; //日志记录器
    static std::mutex _mutex;   //互斥锁，用于线程安全
};

//日志宏定义
//使用了fmt库的格式化字符串
#define DBG(format, ...) ai_chat_sdk::Logger::getLogger()->debug(std::string("[{:>10s}:{:<4d}] ")+format,__FILE__,__LINE__,##__VA_ARGS__)
#define ERR(format, ...) ai_chat_sdk::Logger::getLogger()->error(std::string("[{:>10s}:{:<4d}] ")+format,__FILE__,__LINE__,##__VA_ARGS__)
#define WARN(format, ...) ai_chat_sdk::Logger::getLogger()->warn(std::string("[{:>10s}:{:<4d}] ")+format,__FILE__,__LINE__,##__VA_ARGS__)
#define INFO(format, ...) ai_chat_sdk::Logger::getLogger()->info(std::string("[{:>10s}:{:<4d}] ")+format,__FILE__,__LINE__,##__VA_ARGS__)
#define CRIT(format, ...) ai_chat_sdk::Logger::getLogger()->critical(std::string("[{:>10s}:{:<4d}] ")+format,__FILE__,__LINE__,##__VA_ARGS__)
}    //ai_chat_sdk 结束