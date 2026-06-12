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
T ParseNum(std::string_view value) {
    T result{};
    const char* begin = value.data();
    const char* end = value.data() + value.size();
    auto [ptr, ec] = std::from_chars(begin, end, result);
    if (ec != std::errc{} || ptr != end) {
        throw std::invalid_argument("Invalid num value");
    }
    return result;
}

inline int64_t DateToDays(std::string_view date) {
    int64_t year = ParseNum<int64_t>(std::string_view(date.data(), 4)) -
                   kBaseYear;
    int64_t month = ParseNum<int64_t>(std::string_view(date.data() + 5, 2)) - 1;
    int64_t day = ParseNum<int64_t>(std::string_view(date.data() + 8, 2)) - 1;

    return (year * kMonthsPerYear + month) * kDaysPerMonth + day;
}

inline int64_t TimeToSeconds(std::string_view time) {
    int64_t hour = ParseNum<int64_t>(std::string_view(time.data() + 11, 2));
    int64_t minute = ParseNum<int64_t>(std::string_view(time.data() + 14, 2));
    int64_t second = ParseNum<int64_t>(std::string_view(time.data() + 17, 2));

    return DateToDays(time) * kSecondsPerDay +
           hour * kMinutesPerHour * kSecondsPerMinute +
           minute * kSecondsPerMinute + second;
}
}  // namespace csv
