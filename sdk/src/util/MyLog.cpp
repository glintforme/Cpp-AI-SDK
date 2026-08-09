#include "../../include/util/MyLog.h"
#include <memory>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/async.h>

namespace ai_chat_sdk{
std::shared_ptr<spdlog::logger> Logger::_logger = nullptr; //日志记录器
std::mutex Logger::_mutex;   //互斥锁，用于线程安全
//默认构造函数
Logger::Logger()
{}

void Logger::init_logger(const std::string &logger_name, const std::string &logger_file,spdlog::level::level_enum logger_level)
{
    if(_logger == nullptr){
        std::lock_guard<std::mutex> lock(_mutex);
        if(_logger == nullptr){
            spdlog::flush_on(logger_level); //全局自动刷新，设置日志级别，大于等于该级别的日志才会被记录
            spdlog::init_thread_pool(32768,1);  //初始化线程池，每个线程的栈大小为32768字节，线程数为1
            if(logger_file == "stdout"){
                _logger = spdlog::stdout_color_mt(logger_name); //一个带颜色输出到控制台的日志器
            }
            else{
                _logger = spdlog::basic_logger_mt<spdlog::async_factory>(logger_name,logger_file);  //输出到指定文件的异步日志记录器
            }
            //设置日志格式
            //%Y-%m-%d %H:%M:%S.%e: 日志时间，%l: 日志级别，%v: 日志内容
            //%n: 日志记录器名称
            _logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
            _logger->set_level(logger_level);
        }
    }
}

std::shared_ptr<spdlog::logger> Logger::getLogger()
{
    return _logger;
}
}   //ai_chat_sdk 结束