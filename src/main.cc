#include <iostream>

#include "base/version.h"

int main() {
    std::cout << "mini-redis " << mini_redis::version() << '\n';
    std::cout << "Project scaffold ready; the network server is not implemented yet.\n";
    return 0;
}
