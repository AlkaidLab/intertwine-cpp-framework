#include "intertwine/fw/TimeUtil.hpp"
#include <boost/chrono.hpp>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <cstring>

namespace intertwine {
namespace fw {

std::string TimeUtil::toISO8601(time_t timestamp, int microseconds, const std::string& timezone) {
    // 转换为 Boost.Chrono 时间点
    auto timePoint = boost::chrono::system_clock::from_time_t(timestamp);
    timePoint += boost::chrono::microseconds(microseconds);
    
    return toISO8601(timePoint, timezone);
}

std::string TimeUtil::toISO8601(const boost::chrono::system_clock::time_point& timePoint,
                                  const std::string& timezone) {
    std::ostringstream oss;
    
    // 转换为 time_t（秒部分）
    auto timeSinceEpoch = timePoint.time_since_epoch();
    auto seconds = boost::chrono::duration_cast<boost::chrono::seconds>(timeSinceEpoch);
    time_t timeT = seconds.count();
    
    // 获取微秒部分
    auto microseconds = boost::chrono::duration_cast<boost::chrono::microseconds>(
        timeSinceEpoch - seconds
    ).count();
    
    // 转换为本地时间结构
    struct tm timeInfo;
    struct tm* timeInfoPtr = nullptr;
    
    if (timezone.empty()) {
        // 使用系统时区
        timeInfoPtr = localtime_r(&timeT, &timeInfo);
    } else {
        // 使用 UTC（如果指定了时区，需要手动计算）
        timeInfoPtr = gmtime_r(&timeT, &timeInfo);
    }
    
    if (!timeInfoPtr) {
        return ""; // 转换失败
    }
    
    // 格式化日期和时间部分：YYYY-MM-DDTHH:mm:ss
    oss << std::setfill('0')
        << std::setw(4) << (timeInfo.tm_year + 1900) << "-"
        << std::setw(2) << (timeInfo.tm_mon + 1) << "-"
        << std::setw(2) << timeInfo.tm_mday << "T"
        << std::setw(2) << timeInfo.tm_hour << ":"
        << std::setw(2) << timeInfo.tm_min << ":"
        << std::setw(2) << timeInfo.tm_sec;
    
    // 添加毫秒部分（从微秒转换）
    int milliseconds = static_cast<int>(microseconds / 1000);
    if (milliseconds > 0) {
        oss << "." << std::setw(3) << milliseconds;
    }
    
    // 添加时区部分
    if (timezone.empty()) {
        // 使用系统时区
        std::string sysTz = getSystemTimezone();
        oss << sysTz;
    } else {
        // 使用指定的时区
        oss << timezone;
    }
    
    return oss.str();
}

int64_t TimeUtil::durationMs(const boost::chrono::system_clock::time_point& start,
                              const boost::chrono::system_clock::time_point& end) {
    auto duration = end - start;
    return boost::chrono::duration_cast<boost::chrono::milliseconds>(duration).count();
}

std::string TimeUtil::nowISO8601(const std::string& timezone) {
    auto now = boost::chrono::system_clock::now();
    return toISO8601(now, timezone);
}

std::string TimeUtil::getSystemTimezone() {
    // 获取系统时区偏移
    time_t now = time(nullptr);
    struct tm localTime;
    struct tm utcTime;
    
    localtime_r(&now, &localTime);
    gmtime_r(&now, &utcTime);
    
    // 计算时区偏移（秒）
    int offsetSeconds = mktime(&localTime) - mktime(&utcTime);
    
    // 转换为小时和分钟
    int hours = offsetSeconds / 3600;
    int minutes = (offsetSeconds % 3600) / 60;
    
    // 格式化为 ±HH:MM
    std::ostringstream oss;
    if (offsetSeconds >= 0) {
        oss << "+";
    } else {
        oss << "-";
        hours = -hours;
        minutes = -minutes;
    }
    
    oss << std::setfill('0')
        << std::setw(2) << hours << ":"
        << std::setw(2) << minutes;
    
    return oss.str();
}

std::string TimeUtil::toMillisecondsString(const boost::chrono::system_clock::time_point& timePoint) {
    auto duration = timePoint.time_since_epoch();
    auto milliseconds = boost::chrono::duration_cast<boost::chrono::milliseconds>(duration).count();
    return std::to_string(milliseconds);
}

int64_t TimeUtil::nowMs() {
    return static_cast<int64_t>(boost::chrono::duration_cast<boost::chrono::milliseconds>(
        boost::chrono::system_clock::now().time_since_epoch()).count());
}

std::string TimeUtil::formatTimePart(const boost::chrono::system_clock::time_point& timePoint) {
    // 这个方法可以用于内部格式化，暂时不需要单独实现
    return "";
}

} // namespace fw
} // namespace intertwine

