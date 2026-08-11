#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "net/buffer.h"
#include "protocol/resp_value.h"

namespace mini_redis {

enum class ParseStatus {
  kComplete,
  kNeedMoreData,
  kError,
};

struct ParseResult {
  ParseStatus status;
  std::optional<RespValue> value;
  std::string error_message;
};

class RespParser {
 public:
  [[nodiscard]] ParseResult parse(Buffer& input);

 private:
  static constexpr std::size_t kMaxBulkLength =
      64 * 1024 * 1024;
  static constexpr std::size_t kMaxArrayLength = 1024;
  static constexpr std::size_t kMaxNestingDepth = 32;

  // 从 input 的 offset 位置解析一个 RESP 对象。
  static ParseResult parse_one(
      std::string_view input,
      std::size_t& offset,
      std::size_t depth);

  static ParseResult parse_bulk_string(
      std::string_view input,
      std::size_t& offset);

  static ParseResult parse_array(
      std::string_view input,
      std::size_t& offset,
      std::size_t depth);

  static std::optional<std::size_t> find_crlf(
      std::string_view input,
      std::size_t start) noexcept;

  static std::optional<std::int64_t> parse_integer(
      std::string_view text) noexcept;

  static ParseResult make_complete(RespValue value);
  static ParseResult make_need_more_data();
  static ParseResult make_error(std::string message);
};

}  // namespace mini_redis