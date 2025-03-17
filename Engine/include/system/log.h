#pragma once
#include "pch.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#if defined(PLATFORM_WINDOWS)
#define TS_BREAK __debugbreak();
#else
#define TS_BREAK _builtin_trap();
#endif

#ifdef DEBUG
#define TS_TRACE(...)      spdlog::get("THSAN")->trace(__VA_ARGS__)
#define TS_INFO(...)       spdlog::get("THSAN")->info(__VA_ARGS__)
#define TS_WARN(...)       spdlog::get("THSAN")->warn(__VA_ARGS__)
#define TS_ERROR(...)      spdlog::get("THSAN")->error(__VA_ARGS__)
#define TS_CRITICAL(...)   spdlog::get("THSAN")->critical(__VA_ARGS__)

#define TS_ASSERT(x, msg)  if ((x)) {} else {TS_CRITICAL("ASSERT - {}\n\t{}\n\tin file: {}\n\ton line: {}", x, msg, __FILE__, __LINE__); TS_BREAK;}
#else
#define TS_TRACE(...)      (void)0
#define TS_WARN(...)       (void)0
#define TS_ERROR(...)      (void)0
#define TS_INFO(...)       (void)0
#define TS_CRITICAL(...)   (void)0

#define TS_ASSERT(x, msg)  if ((x)) {} else {TS_CRITICAL("ASSERT - {}\n\t{}\n\tin file: {}\n\ton line: {}", x, msg, __FILE__, __LINE__);};
#endif
