#include "Logger.hpp"
/**
 * @brief 日志等级,从字符串到对象的映射
 * @return 日志等级
 * @throws 找不到会直接报错
 */
spdlog::level::level_enum logLevelFromString(const string &logLevel)
{
    static const unordered_map<string, spdlog::level::level_enum> level_map = {
        {"trace", spdlog::level::trace},
        {"debug", spdlog::level::debug},
        {"info", spdlog::level::info},
        {"warn", spdlog::level::warn},
        {"err", spdlog::level::err},
        {"critical", spdlog::level::critical},
        {"off", spdlog::level::off}};

    auto it = level_map.find(logLevel);
    if (it != level_map.end())
    {
        return it->second;
    }
    else
    {
        cerr << "Invalid log level: " << logLevel << ", defaulting to info" << endl;
        return spdlog::level::info;
    }
}

/**
 * @brief 初始化日志函数
 * @param logFilePath 日志存储路径+名称
 * @param logLevel 日志等级
 */
void initLogger(const std::string &logFilePath, const std::string &logLevel )
{
    // 初始化线程池（全局操作，确保只调用一次）
    static bool initialized = false;
    if (!initialized)
    {
        spdlog::init_thread_pool(8192, 1);
        initialized = true;
    }

    // 创建异步日志
    auto logger = spdlog::daily_logger_mt<spdlog::async_factory>("global", logFilePath, 0, 0);

    // 设置日志格式
    logger->set_pattern("[%n] [%Y-%m-%d %H:%M:%S.%e] [%l] [%t] [%s %!:%#]  %v");

    // 设置默认日志器
    spdlog::set_default_logger(logger);

    // 设置日志级别
    spdlog::set_level(spdlog::level::from_str(logLevel));

    // 设置自动刷新级别
    spdlog::flush_on(spdlog::level::info);
}