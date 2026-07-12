#ifndef INTERTWINE_CPP_FRAMEWORK_TIME_UTIL_HPP
#define INTERTWINE_CPP_FRAMEWORK_TIME_UTIL_HPP

#include <ctime>
#include <string>
#include <boost/chrono.hpp>

namespace intertwine {
namespace fw {

class TimeUtil {
public:
    static std::string toISO8601(time_t timestamp, 
                                  int microseconds = 0,
                                  const std::string& timezone = "");
    
    static std::string toISO8601(const boost::chrono::system_clock::time_point& timePoint,
                                  const std::string& timezone = "");
    
    static int64_t durationMs(const boost::chrono::system_clock::time_point& start,
                               const boost::chrono::system_clock::time_point& end);
    
    static std::string nowISO8601(const std::string& timezone = "");
    
    static std::string getSystemTimezone();

    static std::string toMillisecondsString(const boost::chrono::system_clock::time_point& timePoint);

    static int64_t nowMs();
    
private:
    static std::string formatTimePart(const boost::chrono::system_clock::time_point& timePoint);
};

} // namespace fw
} // namespace intertwine

#endif // INTERTWINE_CPP_FRAMEWORK_TIME_UTIL_HPP
