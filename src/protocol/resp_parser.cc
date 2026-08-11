#include "protocol/resp_parser.h"

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mini_redis {

ParseResult RespParser::parse(Buffer& input) {
  // TODO 1：使用 input.peek() 和 input.readable_bytes()
  // 创建一个只读的 std::string_view。
  //
  // 注意：
  // - 不要从 Buffer 复制全部数据。
  // - 解析过程中不要修改 Buffer。
    std::string_view input_view(input.peek(), input.readable_bytes());

  // TODO 2：创建 offset，并初始化为 0。
  // offset 表示当前解析到了 string_view 的哪个位置。
    std::size_t offset = 0;

  // TODO 3：调用 parse_value()。
  // 初始 depth 应该表示最外层解析深度。
    ParseResult result = parse_one(input_view, offset, 0);

  // TODO 4：检查 ParseResult。
  //
  // 如果是 kComplete：
  // - 使用最终 offset 调用 input.retrieve()；
  // - 只消费当前这一条 RESP value 的字节。
  //
  // 如果是 kNeedMoreData 或 kError：
  // - 不要消费 Buffer 中的任何数据。
    if(result.status == ParseStatus::kComplete) {
        input.retrieve(offset);
    }

  // TODO 5：返回 ParseResult。
    return result;
}

ParseResult RespParser::parse_one(std::string_view input,
                                    std::size_t& offset,
                                    std::size_t depth) {
  // TODO 1：检查 offset 是否已经到达 input 末尾。
  // 如果没有可用字节，应返回 kNeedMoreData。
    if(offset >= input.size()) {
        return make_need_more_data();
    }

  const char type = input[offset];


  // TODO 3：如果当前字节是 '$'，
  // 调用 parse_bulk_string()。
  if (type == '$') {
    return parse_bulk_string(input, offset);
  }

  // TODO 4：如果当前字节是 '*'，
  // 调用 parse_array()，并传入当前 depth。
if(type == '*') {
    return parse_array(input, offset, depth);
  }
  // TODO 5：如果是当前阶段不支持的类型，
  // 返回包含明确错误信息的 kError。
  if (type != '$' && type != '*') {
    return make_error("unsupported RESP type");
  }
}

ParseResult RespParser::parse_bulk_string(std::string_view input,
                                          std::size_t& offset) {
  // 要解析的格式：
  //
  // $<length>\r\n<payload>\r\n
  //
  // Null Bulk String：
  //
  // $-1\r\n

  // TODO 1：记录进入本函数时的起始 offset。
  // 这个位置应该指向 '$'。
  std::size_t start_offset = offset;

  // TODO 2：跳过 '$'，让 offset 指向长度字段的第一个字符。
  offset++;  // Move past the '$' character

  // TODO 3：调用 find_crlf() 查找长度行的结尾。
  //
  // 如果没有找到 CRLF：
  // - 数据可能是半包；
  // - 返回 kNeedMoreData。
  const auto line_end = find_crlf(input, offset);

  if (!line_end) {
    return make_need_more_data();
  }



  // TODO 4：从 input 中截取长度字段。
  //
  // 示例：
  // "$3\r\nfoo\r\n" 中应截取到 "3"。
  const std::string_view length_text =
    input.substr(offset, *line_end - offset);


  // TODO 5：调用 parse_integer() 严格解析长度。
  //
  // 如果无法转换：
  // - 返回 kError；
  // - 不要把格式错误当成 kNeedMoreData。
std::optional<std::int64_t>len= parse_integer(length_text);
  // TODO 6：处理长度的合法性。
  //
  // - -1：表示 Null Bulk String。
  // - 小于 -1：协议错误。
  // - 非负数：普通 Bulk String。
  // - 超过 kMaxBulkLength：资源限制错误。
  if(!len.has_value()) {
    return make_error("invalid bulk string length");
  }
  if (*len < -1) {
      return make_error("invalid bulk string length");
  } else if (*len > static_cast<std::int64_t>(kMaxBulkLength)) {
      return make_error("bulk string length exceeds maximum limit");
  }
  // TODO 7：把 offset 移动到 payload 的开始位置。
  offset = *line_end + 2;  // Move past the CRLF after the length line
  // TODO 8：检查 Buffer 中是否已经包含：
  //
  // - 指定长度的 payload；
  // - payload 末尾的两个 CRLF 字节。
  //
  // 数据不足时返回 kNeedMoreData。
  //
  // 计算长度时注意整数溢出。
  if (*len == -1) {
    return make_complete(
        RespValue::null_bulk_string());
  }
 // 前面已经排除了负数，此时可以安全转换。
  const std::size_t bulk_length =
      static_cast<std::size_t>(*len);

  // offset 理论上不会超过 input.size()，
  // 这里保留检查可以避免无符号减法下溢。
  if (offset > input.size()) {
    return make_need_more_data();
  }

  const std::size_t remaining =
      input.size() - offset;

  // 先确认 payload 足够长。
  if (bulk_length > remaining) {
    return make_need_more_data();
  }

  // payload 后面还必须有两个 CRLF 字节。
  // 使用减法检查，避免 bulk_length + 2 发生溢出。
  if (remaining - bulk_length < 2) {
    return make_need_more_data();
  }

  // payload 的结束位置由声明长度决定，不能搜索 CRLF。
  const std::size_t payload_end =
      offset + bulk_length;

  // 精确检查 payload 后面的 CRLF。
  if (input[payload_end] != '\r' ||
      input[payload_end + 1] != '\n') {
    return make_error(
        "bulk string payload not terminated with CRLF");
  }

  // 使用地址和明确长度构造，支持 payload 中包含 '\0' 或 CRLF。
  std::string payload(
      input.data() + offset,
      bulk_length);

  // 移动到整条 Bulk String 后面。
  offset = payload_end + 2;

  return make_complete(RespValue::bulk_string(std::move(payload)));
}

ParseResult RespParser::parse_array(std::string_view input,
                                    std::size_t& offset,
                                    std::size_t depth) {
  // 要解析的格式：
  //
  // *<element-count>\r\n
  // <element-1>
  // <element-2>
  // ...

  // TODO 1：检查 depth 是否超过 kMaxNestingDepth。
  // 超过限制时返回 kError。
  if (depth >= kMaxNestingDepth)  return make_error("maximum RESP nesting depth exceeded");

  // TODO 2：确认进入本函数时 offset 指向 '*'，
  // 然后跳过该类型前缀。
  offset++; // Move past the '*' character

  // TODO 3：调用 find_crlf() 查找数组长度行的结尾。
  //
  // 找不到时返回 kNeedMoreData。
    const auto line_end = find_crlf(input, offset);

  if (!line_end) {
    return make_need_more_data();
  }

  // TODO 4：截取数组元素数量字段。
  const std::string_view length_text =
    input.substr(offset, *line_end - offset);

  // TODO 5：调用 parse_integer() 解析元素数量。
  //
  // 解析失败时返回 kError。
    std::optional<std::int64_t>len= parse_integer(length_text);


  // TODO 6：验证数组长度。
  //
  // - 负数如何处理，需要符合当前 RespValue 的类型设计。
  // - 当前 RespValue 没有 Null Array 类型。
  // - 超过 kMaxArrayLength 时返回 kError。
   if(!len.has_value()) {
    return make_error("invalid bulk string length");
  }
  if (*len < -1) {
      return make_error("invalid bulk string length");
  } else if (*len > static_cast<std::int64_t>(kMaxArrayLength)) {
      return make_error("bulk string length exceeds maximum limit");
  }
  // TODO 7：把 offset 移动到第一个数组元素的位置。
  offset = *line_end + 2;  // Move past the CRLF after the length line

  // TODO 8：创建 std::vector<RespValue> 保存数组元素。
  //
  // 可以根据已经验证过的元素数量预留空间，
  // 但不要在验证长度之前分配内存。
  std::vector<RespValue> elements;


  // TODO 9：循环解析指定数量的元素。
  //
  // 每个元素都调用 parse_value()。
  // 递归深度需要增加。
  while (elements.size() < static_cast<std::size_t>(*len)) {
    ParseResult element_result = parse_one(input, offset, depth + 1);

    if (element_result.status == ParseStatus::kComplete) {
        elements.push_back(std::move(*element_result.value));
    } else if (element_result.status == ParseStatus::kNeedMoreData) {
        return make_need_more_data();
    } else if (element_result.status == ParseStatus::kError) {
        return make_error(element_result.error_message);
    }
  }

  // TODO 10：处理子元素的 ParseResult。
  //
  // kComplete：
  // - 从 optional 中取得 RespValue；
  // - 移动到 elements 中。
  //
  // kNeedMoreData：
  // - 立即向上传递；
  // - 最外层 parse() 不应消费 Buffer。
  //
  // kError：
  // - 立即向上传递错误。

  // TODO 11：所有元素解析完成后，
  // 调用你已经实现的：
  //

  return make_complete(RespValue::array(std::move(elements)));



  // TODO 12：使用 make_complete() 包装并返回。
}

std::optional<std::size_t> RespParser::find_crlf(
    std::string_view input,
    std::size_t start) noexcept {
  // TODO 1：从 start 开始扫描 input。
  for (std::size_t i = start; i + 1 < input.size(); ++i) {
    if (input[i] == '\r' && input[i + 1] == '\n') {
      return i;  // Found CRLF at position i
    }
  }

  // TODO 2：确保读取当前位置的下一个字节前，
  // 下一个字节仍在 input 范围内。

  // TODO 3：寻找连续的 '\r' 和 '\n'。

  // TODO 4：找到后返回 '\r' 所在的下标。

  // TODO 5：扫描完仍未找到时返回 std::nullopt。
  //
  // 注意：
  // 没找到 CRLF 表示数据可能尚未接收完整，
  // 不一定是协议错误。
  return std::nullopt;
}

std::optional<std::int64_t> RespParser::parse_integer(
    std::string_view text) noexcept {
  // TODO 1：拒绝空字符串。
      if (text.empty()) {
        return std::nullopt;
      }
  // TODO 2：准备一个 std::int64_t 保存解析结果。
      std::int64_t result = 0;
  // TODO 3：使用 std::from_chars() 解析十进制整数。
        const auto conversion =std::from_chars(text.data(), text.data() + text.size(), result);

  // TODO 4：检查 std::from_chars() 返回的错误状态。
  //
  // 需要识别：
  // - 非法字符；
  // - 数值溢出。
  if (conversion.ec != std::errc{}) {
    return std::nullopt;
  }

  if (conversion.ptr != end) {
    return std::nullopt;
  }

  // TODO 5：检查解析结束位置。
  //
  // 必须消费 text 的全部内容。
  // 例如 "12x" 不能被当成合法的 12。
 if (conversion.ptr != end) {
    return std::nullopt;
  }

  // TODO 6：成功时返回整数，
  // 失败时返回 std::nullopt。
  return result;
}

ParseResult RespParser::make_complete(RespValue value) {
  // TODO：创建 ParseResult，并保证：
  //
  // status        == ParseStatus::kComplete
  // value         包含传入的 RespValue
  // error_message 为空
  //
  // 应移动 value，避免不必要的复制。
    ParseResult result;
    result.status = ParseStatus::kComplete;
    result.value = value;
    result.error_message = "";
    return result;

}

ParseResult RespParser::make_need_more_data() {
  // TODO：创建 ParseResult，并保证：
  //
  // status        == ParseStatus::kNeedMoreData
  // value         == std::nullopt
  // error_message 为空
    ParseResult result;
    result.status = ParseStatus::kNeedMoreData;
    result.value = std::nullopt;
    result.error_message = "";
     return result;
}

ParseResult RespParser::make_error(std::string message) {
  // TODO：创建 ParseResult，并保证：
  //
  // status        == ParseStatus::kError
  // value         == std::nullopt
  // error_message 包含传入的信息
  //
  // 应移动 message，避免不必要的复制。
    ParseResult result;
    result.status = ParseStatus::kError;  
    result.value = std::nullopt;
    result.error_message = std::move(message);
     return result;
}

}  // namespace mini_redis
