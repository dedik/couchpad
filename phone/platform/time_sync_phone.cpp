// Zegar telefonu synchronizuje system — pilot ustawia tylko strefę czasową.
// Implementuje nagłówek transport/time_sync.hpp z firmware'u.

#include <cstdlib>
#include <ctime>

#include "transport/time_sync.hpp"

namespace transport {
namespace {

constexpr char kTimezone[] = "CET-1CEST,M3.5.0,M10.5.0/3";
constexpr int kFirstSaneYear = 2024;
bool g_timezone_set = false;

}  // namespace

void set_timezone(const char* posix) {
    setenv("TZ", posix, 1);
    tzset();
    g_timezone_set = true;
}

void start_time_sync() {
    if (!g_timezone_set) {
        set_timezone(kTimezone);
    }
}

void service_time_sync() {}

bool time_known() {
    std::time_t now = std::time(nullptr);
    std::tm parts = {};
    localtime_r(&now, &parts);
    return parts.tm_year + 1900 >= kFirstSaneYear;
}

}  // namespace transport
