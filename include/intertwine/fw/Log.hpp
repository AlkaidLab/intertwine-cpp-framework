#ifndef INTERTWINE_CPP_FRAMEWORK_LOG_HPP
#define INTERTWINE_CPP_FRAMEWORK_LOG_HPP

#include <hv/hlog.h>
#include <string>

// fw 内部日志宏（基于 libhv hlog）
#ifndef LOG_ERROR
#define LOG_ERROR(msg) do { std::string _fw_m(msg); hloge("%s", _fw_m.c_str()); } while(0)
#endif
#ifndef LOG_WARN
#define LOG_WARN(msg)  do { std::string _fw_m(msg); hlogw("%s", _fw_m.c_str()); } while(0)
#endif
#ifndef LOG_INFO
#define LOG_INFO(msg)  do { std::string _fw_m(msg); hlogi("%s", _fw_m.c_str()); } while(0)
#endif
#ifndef LOG_DEBUG
#define LOG_DEBUG(msg) do { std::string _fw_m(msg); hlogd("%s", _fw_m.c_str()); } while(0)
#endif

#endif // INTERTWINE_CPP_FRAMEWORK_LOG_HPP
