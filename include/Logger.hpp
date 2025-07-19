#ifndef __SPLOG_H__
#define __SPLOG_H__
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/daily_file_sink.h>
#include <string>
#include <unordered_map>
#include <iostream>

using std::cerr;
using std::endl;
using std::string;
using std::unordered_map;
/**
 * @brief 日志等级,从字符串到对象的映射
 * @return 日志等级
 * @throws 找不到会直接报错
 */
spdlog::level::level_enum logLevelFromString(const string &logLevel);

/**
 * @brief 初始化日志函数
 * @param logFilePath 日志存储路径+名称
 * @param logLevel 日志等级
 */
void initLogger(const std::string &logFilePath = "../log/log.txt", const std::string &logLevel = "info");

#endif