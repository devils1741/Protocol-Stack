#include <iostream>
#include "logger.hpp"
using std::string;




int main(int argc, char **argv)
{
    init_logger();
    SPDLOG_INFO("The APP start!");
}