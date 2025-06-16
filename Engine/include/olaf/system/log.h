#pragma once
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#ifdef _WIN32
#define OLAF_BREAK __debugbreak();
#else
#define OLAF_BREAK __builtin_trap();
#endif

#ifdef DEBUG
#define OLAF_TRACE(...)    spdlog::get("OLAF")->trace(__VA_ARGS__)
#define OLAF_INFO(...)     spdlog::get("OLAF")->info(__VA_ARGS__)
#define OLAF_WARN(...)     spdlog::get("OLAF")->warn(__VA_ARGS__)
#define OLAF_ERROR(...)    spdlog::get("OLAF")->error(__VA_ARGS__)
#define OLAF_CRITICAL(...) spdlog::get("OLAF")->critical(__VA_ARGS__)

#define OLAF_ASSERT(x, msg)                                                                           \
    if ((x)) {                                                                                        \
    } else {                                                                                          \
        OLAF_CRITICAL("ASSERT - {}\n\t{}\n\tin file: {}\n\ton line: {}", x, msg, __FILE__, __LINE__); \
        OLAF_BREAK;                                                                                   \
    }
#else
#define OLAF_TRACE(...)    (void)0
#define OLAF_WARN(...)     (void)0
#define OLAF_ERROR(...)    (void)0
#define OLAF_INFO(...)     (void)0
#define OLAF_CRITICAL(...) (void)0

#define OLAF_ASSERT(x, msg)                                                                           \
    if ((x)) {                                                                                        \
    } else {                                                                                          \
        OLAF_CRITICAL("ASSERT - {}\n\t{}\n\tin file: {}\n\ton line: {}", x, msg, __FILE__, __LINE__); \
    };
#endif
