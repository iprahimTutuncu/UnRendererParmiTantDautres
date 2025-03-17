#pragma once

#include <spdlog/spdlog.h>
#include <memory>
#include <string>

class LogManager {
public:
    static void init(const std::string& logFileName = "app_logs.txt");

    static void shutdown();

private:
    static bool isInitialized; 
    static std::shared_ptr<spdlog::logger> logger_;
};

