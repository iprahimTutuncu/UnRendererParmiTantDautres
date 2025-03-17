#include "pch.h"

#include "system/log_manager.h"
#include "system/log.h"

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

bool LogManager::isInitialized = false;
std::shared_ptr<spdlog::logger> LogManager::logger_ = nullptr;

void LogManager::init(const std::string& logFileName)
{
    if (!isInitialized)
    {
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_level(spdlog::level::trace);

        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFileName, true);
        file_sink->set_level(spdlog::level::debug);

        logger_ = std::make_shared<spdlog::logger>("THSAN", spdlog::sinks_init_list{ console_sink, file_sink });
        spdlog::set_default_logger(logger_);

        spdlog::set_level(spdlog::level::trace);

        spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%t] %^[%l] %v%$");

        TS_INFO("Logger initialized.");
        isInitialized = true;
    }
}

void LogManager::shutdown()
{
    if (isInitialized) 
    {
        spdlog::shutdown();
        isInitialized = false;
    }
}