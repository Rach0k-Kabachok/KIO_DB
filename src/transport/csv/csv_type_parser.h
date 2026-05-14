#pragma once

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string_view>
#include <system_error>

namespace csv {
constexpr int64_t kBaseYear = 1970;
constexpr int64_t kMonthsPerYear = 12;
constexpr int64_t kDaysPerMonth = 31;
constexpr int64_t kHoursPerDay = 24;
constexpr int64_t kMinutesPerHour = 60;
constexpr int64_t kSecondsPerMinute = 60;
constexpr int64_t kSecondsPerDay =
    kHoursPerDay * kMinutesPerHour * kSecondsPerMinute;

template <typename T>
T ParseInteger(std::string_view value) {
    T result{};
    const char* begin = value.data();
    const char* end = value.data() + value.size();
    auto [ptr, ec] = std::from_chars(begin, end, result);
    if (ec != std::errc{} || ptr != end) {
        throw std::invalid_argument("Invalid integer value");
    }
    return result;
}

inline int64_t ParseDigits(std::string_view value, size_t pos, size_t len) {
    int64_t result = 0;
    for (size_t i = 0; i < len; ++i) {
        result = result * 10 + value[pos + i] - '0';
    }
    return result;
}

inline int64_t DateToDays(std::string_view date) {
    int64_t year = ParseDigits(date, 0, 4) - kBaseYear;
    int64_t month = ParseDigits(date, 5, 2) - 1;
    int64_t day = ParseDigits(date, 8, 2) - 1;

    return (year * kMonthsPerYear + month) * kDaysPerMonth + day;
}

inline int64_t TimeToSeconds(std::string_view time) {
    int64_t hour = ParseDigits(time, 11, 2);
    int64_t minute = ParseDigits(time, 14, 2);
    int64_t second = ParseDigits(time, 17, 2);

    return DateToDays(time) * kSecondsPerDay +
           hour * kMinutesPerHour * kSecondsPerMinute +
           minute * kSecondsPerMinute + second;
}
}  // namespace csv
