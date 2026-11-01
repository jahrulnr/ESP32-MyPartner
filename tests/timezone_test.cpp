#include <cstdint>
#include <app_config.h>
#include <cassert>
#include <cstdlib>
#include <ctime>
#include <iostream>

int main() {
  setenv("TZ", partner::config::timezone, 1);
  tzset();
  for (const int hour : {0, 9, 23}) {
    // 2026-11-01 00:00 UTC + hour.
    const time_t epoch = 1793491200 + hour * 3600;
    tm local {};
    assert(localtime_r(&epoch, &local));
    assert(local.tm_hour == (hour + 7) % 24);
    assert(local.tm_mday == (hour + 7 >= 24 ? 2 : 1));
  }
  std::cout << "timezone: configured WIB offset UTC+7 and date rollover passed\n";
}
