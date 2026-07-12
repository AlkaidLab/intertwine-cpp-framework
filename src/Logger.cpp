#include "intertwine/fw/Logger.hpp"
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <vector>

namespace intertwine {
namespace fw {

Logger::Logger()
    : m_level(LogLevel::INFO)
    , m_consoleEnabled(false)  // 默认禁用控制台
    , m_maxFileSize(104857600)
    , m_maxFiles(5)
{
    initializeLogger();
}

Logger::~Logger() {
    if (m_logger) {
        m_logger->flush();
    }
}

Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

void Logger::initializeLogger() {
    recreateLogger();
}

void Logger::recreateLogger() {
    try {
        std::vector<spdlog::sink_ptr> sinks;

        // 1. 控制台 Sink
        if (m_consoleEnabled) {
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            console_sink->set_level(spdlog::level::trace);
            console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] %v");
            sinks.push_back(console_sink);
        }

        // 2. 文件 Sink
        if (!m_logFile.empty()) {
            auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(m_logFile, m_maxFileSize, m_maxFiles);
            file_sink->set_level(spdlog::level::trace);
            file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%t] %v");
            sinks.push_back(file_sink);
        }

        // 创建新 logger
        auto new_logger = std::make_shared<spdlog::logger>("intertwine", sinks.begin(), sinks.end());
        new_logger->set_level(toSpdlogLevel(m_level));
        new_logger->flush_on(spdlog::level::warn);

        // 原子替换
        m_logger = new_logger;
        
    } catch (const spdlog::spdlog_ex& ex) {
        std::cerr << "Logger recreation failed: " << ex.what() << '\n';
    }
}

void Logger::setConsoleEnabled(bool enabled) {
    m_consoleEnabled = enabled;
    recreateLogger();
}

void Logger::setLevel(LogLevel level) {
    m_level = level;
    if (m_logger) {
        m_logger->set_level(toSpdlogLevel(level));
    }
}

LogLevel Logger::getLevel() const {
    return m_level;
}

void Logger::setLogFile(const std::string& logDir,
                        size_t maxFileSize,
                        size_t maxFiles) {
    if (logDir.empty()) {
        m_logFile.clear();
        recreateLogger();
        return;
    }
    
    m_maxFileSize = maxFileSize;
    m_maxFiles = maxFiles;
    m_logFile = logDir + "/error.log";
    
    recreateLogger();
}

void Logger::log(LogLevel level, const std::string& message) {
    if (level < m_level || !m_logger) {
        return;
    }
    
    spdlog::level::level_enum spdlog_level = toSpdlogLevel(level);
    m_logger->log(spdlog_level, message);
}

void Logger::trace(const std::string& message) {
    log(LogLevel::TRACE, message);
}

void Logger::debug(const std::string& message) {
    log(LogLevel::DEBUG, message);
}

void Logger::info(const std::string& message) {
    log(LogLevel::INFO, message);
}

void Logger::warn(const std::string& message) {
    log(LogLevel::WARN, message);
}

void Logger::error(const std::string& message) {
    log(LogLevel::ERROR, message);
}

void Logger::tracef(const char* format, ...) { // NOLINT(cert-dcl50-cpp)
    if (m_level > LogLevel::TRACE || !m_logger) {
        return;
    }
    char buffer[4096];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    log(LogLevel::TRACE, std::string(buffer));
}

void Logger::debugf(const char* format, ...) { // NOLINT(cert-dcl50-cpp)
    if (m_level > LogLevel::DEBUG || !m_logger) {
        return;
    }
    char buffer[4096];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    log(LogLevel::DEBUG, std::string(buffer));
}

void Logger::infof(const char* format, ...) { // NOLINT(cert-dcl50-cpp)
    if (m_level > LogLevel::INFO || !m_logger) {
        return;
    }
    char buffer[4096];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    log(LogLevel::INFO, std::string(buffer));
}

void Logger::warnf(const char* format, ...) { // NOLINT(cert-dcl50-cpp)
    if (m_level > LogLevel::WARN || !m_logger) {
        return;
    }
    char buffer[4096];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    log(LogLevel::WARN, std::string(buffer));
}

void Logger::errorf(const char* format, ...) { // NOLINT(cert-dcl50-cpp)
    if (m_level > LogLevel::ERROR || !m_logger) {
        return;
    }
    char buffer[4096];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    log(LogLevel::ERROR, std::string(buffer));
}

void Logger::flush() {
    if (m_logger) {
        m_logger->flush();
    }
}

spdlog::level::level_enum Logger::toSpdlogLevel(LogLevel level) const {
    switch (level) {
        case LogLevel::TRACE: return spdlog::level::trace;
        case LogLevel::DEBUG: return spdlog::level::debug;
        case LogLevel::INFO:  return spdlog::level::info;
        case LogLevel::WARN:  return spdlog::level::warn;
        case LogLevel::ERROR: return spdlog::level::err;
        default: return spdlog::level::info;
    }
}

LogLevel Logger::fromSpdlogLevel(spdlog::level::level_enum level) const {
    switch (level) {
        case spdlog::level::trace: return LogLevel::TRACE;
        case spdlog::level::debug: return LogLevel::DEBUG;
        case spdlog::level::info:  return LogLevel::INFO;
        case spdlog::level::warn:  return LogLevel::WARN;
        case spdlog::level::err:
        case spdlog::level::critical:
        case spdlog::level::off:   return LogLevel::ERROR;
        default: return LogLevel::INFO;
    }
}

} // namespace fw
} // namespace intertwine
