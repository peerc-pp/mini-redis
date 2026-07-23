#pragma once

#include <cstdint>
#include <string>

namespace mini_redis {

struct BlockingEchoServerConfig {
  std::string bind_address;
  std::uint16_t port;
  int backlog;
};

class BlockingEchoServer final {
 public:
  explicit BlockingEchoServer(BlockingEchoServerConfig config);

  [[nodiscard]] int run();

 private:
  BlockingEchoServerConfig config_;
};

}  // namespace mini_redis