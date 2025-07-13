#include <iostream>
#include "../include/logger.hpp"
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
using std::string;




int main(int argc, char **argv)
{
    init_logger();
    spdlog::info("second hello spdlog!");
}