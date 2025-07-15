#include <iostream>
#include "logger.hpp"
#include "configManager.hpp"
using std::string;

int main(int argc, char **argv)
{
    init_logger();
    SPDLOG_INFO("The APP start!");

    ConfigManager &configManager = ConfigManager::getInstance();
    configManager.loadConfig("/repo/Protocol-Stack/config/args.json");
    SPDLOG_INFO(configManager.toString());

}
