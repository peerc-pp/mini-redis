#include "base/version.h"

#include <iostream>
#include <string_view>

int main() {
    constexpr std::string_view kExpectedVersion{"0.1.0"};
    const std::string_view actual_version = mini_redis::version();

    if (actual_version != kExpectedVersion) {
        std::cerr << "Expected version " << kExpectedVersion << ", got " << actual_version << '\n';
        return 1;
    }

    return 0;
}
