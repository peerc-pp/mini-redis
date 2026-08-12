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
    // 解析期间仅观察 Buffer；确认完整解析后才消费数据。
    std::string_view input_view(input.peek(), input.readable_bytes());

    // offset 是本地解析游标，推进它不会直接修改 Buffer。
    std::size_t offset = 0;

    ParseResult result = parse_one(input_view, offset, 0);

    // 半包或协议错误时不消费数据，以保持解析操作的事务性。
    if (result.status == ParseStatus::kComplete) {
        input.retrieve(offset);
    }

    return result;
}

ParseResult RespParser::parse_one(std::string_view input, std::size_t& offset, std::size_t depth) {
    // 没有足够字节读取类型前缀时，按半包处理。
    if (offset >= input.size()) {
        return make_need_more_data();
    }

    const char type = input[offset];

    // 根据 RESP 类型前缀分派给对应解析器。
    if (type == '$') {
        return parse_bulk_string(input, offset);
    }

    if (type == '*') {
        return parse_array(input, offset, depth);
    }

    // 当前阶段只支持 Bulk String 和 Array。
    return make_error("unsupported RESP type");
}

ParseResult RespParser::parse_bulk_string(std::string_view input, std::size_t& offset) {
    // 要解析的格式：
    //
    // $<length>\r\n<payload>\r\n
    //
    // Null Bulk String：
    //
    // $-1\r\n

    // 调用者已根据 '$' 前缀完成分派，跳过类型前缀后解析长度。
    offset++;

    // 长度行不完整时等待更多数据。
    const auto line_end = find_crlf(input, offset);

    if (!line_end) {
        return make_need_more_data();
    }

    const std::string_view length_text = input.substr(offset, *line_end - offset);

    // 长度字段格式错误不是半包，应返回协议错误。
    std::optional<std::int64_t> len = parse_integer(length_text);
    if (!len.has_value()) {
        return make_error("invalid bulk string length");
    }
    if (*len < -1) {
        return make_error("invalid bulk string length");
    } else if (*len > static_cast<std::int64_t>(kMaxBulkLength)) {
        return make_error("bulk string length exceeds maximum limit");
    }
    // 跳过长度行末尾的 CRLF，进入 payload。
    offset = *line_end + 2;
    if (*len == -1) {
        return make_complete(RespValue::null_bulk_string());
    }
    // 前面已经排除了负数，此时可以安全转换。
    const std::size_t bulk_length = static_cast<std::size_t>(*len);

    // offset 理论上不会超过 input.size()，
    // 这里保留检查可以避免无符号减法下溢。
    if (offset > input.size()) {
        return make_need_more_data();
    }

    const std::size_t remaining = input.size() - offset;

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
    const std::size_t payload_end = offset + bulk_length;

    // 精确检查 payload 后面的 CRLF。
    if (input[payload_end] != '\r' || input[payload_end + 1] != '\n') {
        return make_error("bulk string payload not terminated with CRLF");
    }

    // 使用地址和明确长度构造，支持 payload 中包含 '\0' 或 CRLF。
    std::string payload(input.data() + offset, bulk_length);

    // 移动到整条 Bulk String 后面。
    offset = payload_end + 2;

    return make_complete(RespValue::bulk_string(std::move(payload)));
}

ParseResult RespParser::parse_array(std::string_view input, std::size_t& offset,
                                    std::size_t depth) {
    // 要解析的格式：
    //
    // *<element-count>\r\n
    // <element-1>
    // <element-2>
    // ...

    // 限制递归深度，避免恶意嵌套耗尽调用栈。
    if (depth >= kMaxNestingDepth) return make_error("maximum RESP nesting depth exceeded");

    // 调用者已根据 '*' 前缀完成分派。
    offset++;

    // 数组长度行不完整时等待更多数据。
    const auto line_end = find_crlf(input, offset);

    if (!line_end) {
        return make_need_more_data();
    }

    const std::string_view length_text = input.substr(offset, *line_end - offset);

    // 数组长度必须是合法的非负整数。
    std::optional<std::int64_t> len = parse_integer(length_text);

    if (!len.has_value()) {
        return make_error("invalid array length");
    }
    if (*len < 0) {
        return make_error("invalid array length");
    } else if (*len > static_cast<std::int64_t>(kMaxArrayLength)) {
        return make_error("array length exceeds maximum limit");
    }
    // 跳过长度行末尾的 CRLF，进入第一个元素。
    offset = *line_end + 2;

    std::vector<RespValue> elements;

    // 递归解析元素；子元素的半包或错误立即向上传播。
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

    // 所有元素完整后才能构造并返回数组。

    return make_complete(RespValue::array(std::move(elements)));
}

std::optional<std::size_t> RespParser::find_crlf(std::string_view input,
                                                 std::size_t start) noexcept {
    // 循环条件同时保证 i + 1 不越界。
    for (std::size_t i = start; i + 1 < input.size(); ++i) {
        if (input[i] == '\r' && input[i + 1] == '\n') {
            return i;
        }
    }

    // 未找到通常表示长度行尚未接收完整。
    return std::nullopt;
}

std::optional<std::int64_t> RespParser::parse_integer(std::string_view text) noexcept {
    if (text.empty()) {
        return std::nullopt;
    }
    std::int64_t result = 0;
    const auto conversion = std::from_chars(text.data(), text.data() + text.size(), result);

    // 转换必须成功且消费整个字段；例如 "12x" 不是合法整数。
    if (conversion.ec != std::errc{}) {
        return std::nullopt;
    }

    if (conversion.ptr != text.data() + text.size()) {
        return std::nullopt;
    }

    return result;
}

ParseResult RespParser::make_complete(RespValue value) {
    ParseResult result;
    result.status = ParseStatus::kComplete;
    result.value = std::move(value);
    result.error_message = "";
    return result;
}

ParseResult RespParser::make_need_more_data() {
    ParseResult result;
    result.status = ParseStatus::kNeedMoreData;
    result.value = std::nullopt;
    result.error_message = "";
    return result;
}

ParseResult RespParser::make_error(std::string message) {
    ParseResult result;
    result.status = ParseStatus::kError;
    result.value = std::nullopt;
    result.error_message = std::move(message);
    return result;
}

}  // namespace mini_redis
