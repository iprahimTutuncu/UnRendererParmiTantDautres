#pragma once
#include "pch.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#if defined(PLATFORM_WINDOWS)
#define GTS_BREAK __debugbreak();
#else
#define GTS_BREAK _builtin_trap();
#endif

#ifdef DEBUG
#define GTS_TRACE(...)      spdlog::get("GTS")->trace(__VA_ARGS__)
#define GTS_INFO(...)       spdlog::get("GTS")->info(__VA_ARGS__)
#define GTS_WARN(...)       spdlog::get("GTS")->warn(__VA_ARGS__)
#define GTS_ERROR(...)      spdlog::get("GTS")->error(__VA_ARGS__)
#define GTS_CRITICAL(...)   spdlog::get("GTS")->critical(__VA_ARGS__)

#define GTS_ASSERT(x, msg)  if ((x)) {} else {GTS_CRITICAL("ASSERT - {}\n\t{}\n\tin file: {}\n\ton line: {}", x, msg, __FILE__, __LINE__); GTS_BREAK;}
#else
#define GTS_TRACE(...)      (void)0
#define GTS_WARN(...)       (void)0
#define GTS_ERROR(...)      (void)0
#define GTS_INFO(...)       (void)0
#define GTS_CRITICAL(...)   (void)0

#define GTS_ASSERT(x, msg)  if ((x)) {} else {GTS_CRITICAL("ASSERT - {}\n\t{}\n\tin file: {}\n\ton line: {}", x, msg, __FILE__, __LINE__);};
#endif
